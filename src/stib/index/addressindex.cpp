// Copyright (c) 2017-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stib/index/addressindex.h>
#include <index/txindex.h>
#include <validation.h>
#include <key_io.h>
#include <rpc/server.h>

constexpr char DB_ADDRESSINDEX = 'a';
constexpr char DB_ADDRESSUNSPENTINDEX = 'u';

std::unique_ptr<AddressIndex> g_addressindex;

struct CDiskTxPos : public FlatFilePos
{
    unsigned int nTxOffset; // after header

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITEAS(FlatFilePos, *this);
        READWRITE(VARINT(nTxOffset));
    }

    CDiskTxPos(const FlatFilePos &blockIn, unsigned int nTxOffsetIn) : FlatFilePos(blockIn.nFile, blockIn.nPos), nTxOffset(nTxOffsetIn) {
    }

    CDiskTxPos() {
        SetNull();
    }

    void SetNull() {
        FlatFilePos::SetNull();
        nTxOffset = 0;
    }
};

struct CAddressUnspentKey {
    unsigned int type;
    uint160 hashBytes;
    uint256 txhash;
    size_t index;

    size_t GetSerializeSize() const {
        return 57 ;
    }
    template<typename Stream>
    void Serialize(Stream& s) const {
        ser_writedata8(s, type);
        hashBytes.Serialize(s);
        txhash.Serialize(s);
        ser_writedata32(s, index);
    }
    template<typename Stream>
    void Unserialize(Stream& s) {
        type = ser_readdata8(s);
        hashBytes.Unserialize(s);
        txhash.Unserialize(s);
        index = ser_readdata32(s);
    }

    CAddressUnspentKey(unsigned int addressType, uint160 addressHash, uint256 txid, size_t indexValue) {
        type = addressType;
        hashBytes = addressHash;
        txhash = txid;
        index = indexValue;
    }
    
    std::string ToString()
    {
        
        std::stringstream ss;
        Serialize(ss);
        return HexStr(ss.str());
    }

    CAddressUnspentKey() {
        SetNull();
    }

    void SetNull() {
        type = 0;
        hashBytes.SetNull();
        txhash.SetNull();
        index = 0;
    }
};

struct CAddressUnspentValue {
    CAmount satoshis;
    CScript script;
    int blockHeight;

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action) {
        READWRITE(satoshis);
        READWRITE(*(CScriptBase*)(&script));
        READWRITE(blockHeight);
    }

    CAddressUnspentValue(CAmount sats, CScript scriptPubKey, int height) {
        satoshis = sats;
        script = scriptPubKey;
        blockHeight = height;
    }

    CAddressUnspentValue() {
        SetNull();
    }

    void SetNull() {
        satoshis = -1;
        script.clear();
        blockHeight = 0;
    }

    bool IsNull() const {
        return (satoshis == -1);
    }
};

struct CAddressIndexKey {
    unsigned int type;
    uint160 hashBytes;
    int blockHeight;
    unsigned int txindex;
    uint256 txhash;
    size_t index;
    bool spending;

    size_t GetSerializeSize() const {
        return 34;
    }
    template<typename Stream>
    void Serialize(Stream& s) const {
        ser_writedata8(s, type);
        hashBytes.Serialize(s);
        // Heights are stored big-endian for key sorting in LevelDB
        ser_writedata32be(s, blockHeight);
        ser_writedata32be(s, txindex);
        txhash.Serialize(s);
        ser_writedata32(s, index);
        char f = spending;
        ser_writedata8(s, f);
    }
    template<typename Stream>
    void Unserialize(Stream& s) {
        type = ser_readdata8(s);
        hashBytes.Unserialize(s);
        blockHeight = ser_readdata32be(s);
        txindex = ser_readdata32be(s);
        txhash.Unserialize(s);
        index = ser_readdata32(s);
        char f = ser_readdata8(s);
        spending = f;
    }

    CAddressIndexKey(unsigned int addressType, uint160 addressHash, int height, int blockindex,
                     uint256 txid, size_t indexValue, bool isSpending) {
        type = addressType;
        hashBytes = addressHash;
        blockHeight = height;
        txindex = blockindex;
        txhash = txid;
        index = indexValue;
        spending = isSpending;
    }

    CAddressIndexKey() {
        SetNull();
    }

    void SetNull() {
        type = 0;
        hashBytes.SetNull();
        blockHeight = 0;
        txindex = 0;
        txhash.SetNull();
        index = 0;
        spending = false;
    }

};

struct CAddressIndexIteratorKey {
    unsigned int type;
    uint160 hashBytes;
/*
    size_t GetSerializeSize() const {
        return 21;
    }*/
    template<typename Stream>
    void Serialize(Stream& s) const {
        ser_writedata8(s, type);
        hashBytes.Serialize(s);
    }
    template<typename Stream>
    void Unserialize(Stream& s) {
        type = ser_readdata8(s);
        hashBytes.Unserialize(s);
    }

    CAddressIndexIteratorKey(unsigned int addressType, uint160 addressHash) {
        type = addressType;
        hashBytes = addressHash;
    }
    
    std::string ToString()
    {
        
        std::stringstream ss;
        Serialize(ss);
        //std::string r = ss;
        return HexStr(ss.str());
    }

    CAddressIndexIteratorKey() {
        SetNull();
    }

    void SetNull() {
        type = 0;
        hashBytes.SetNull();
    }
};

/**
 * Access to the txindex database (indexes/txindex/)
 *
 * The database stores a block locator of the chain the database is synced to
 * so that the TxIndex can efficiently determine the point it last stopped at.
 * A locator is used instead of a simple hash of the chain tip because blocks
 * and block index entries may not be flushed to disk until after this database
 * is updated.
 */
class AddressIndex::DB : public BaseIndex::DB
{
public:
    explicit DB(size_t n_cache_size, bool f_memory = false, bool f_wipe = false);

    /// Read the disk location of the transaction data with the given hash. Returns false if the
    /// transaction hash is not indexed.
    bool ReadUnspentIndex(uint160 addressHash, int addressType,
                                           std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &ounspentOutputs);

    /// Write a batch of transaction positions to the DB.
    bool WriteUnspentIndexs(const std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue > >&vect) ;

    /// Delete a batch of transaction positions from the DB.
    bool DeleteUnspentIndexs(const std::vector<CAddressUnspentKey>&vect) ;
    

    /// Read the disk location of the transaction data with the given hash. Returns false if the
    /// transaction hash is not indexed.
    bool ReadIndex(uint160 addressHash, int addressType,
                                           std::vector<std::pair<CAddressIndexKey, CAmount> > &utputs);

    /// Write a batch of indexes to the DB.
    bool WriteIndexs(const std::vector<std::pair<CAddressIndexKey, CAmount > >&vect) ;
    
    
    /// write two batches of transaction and delete one
    bool WriteWriteDelete(
        const std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue > >&wuout,
        const std::vector<std::pair<CAddressIndexKey, CAmount > >&wout,
        const std::vector<CAddressUnspentKey>&duout
    );
};

AddressIndex::DB::DB(size_t n_cache_size, bool f_memory, bool f_wipe) :
    BaseIndex::DB(GetDataDir() / "indexes" / "addressindex", n_cache_size, f_memory, f_wipe)
{}

bool AddressIndex::DB::ReadUnspentIndex(uint160 addressHash, int addressType,
                                           std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs)
{
    std::unique_ptr<CDBIterator> pcursor(NewIterator());

    pcursor->Seek(std::make_pair(DB_ADDRESSUNSPENTINDEX, CAddressIndexIteratorKey(addressType, addressHash)));

    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char,CAddressUnspentKey> key;
        if (pcursor->GetKey(key) && key.first == DB_ADDRESSUNSPENTINDEX && key.second.hashBytes == addressHash) {
            CAddressUnspentValue nValue;
            if (pcursor->GetValue(nValue)) {
                unspentOutputs.push_back(std::make_pair(key.second, nValue));
                pcursor->Next();
            } else {
                return error("failed to get address unspent value");
            }
        } else {
            break;
        }
    }

    return true;
}

bool AddressIndex::DB::WriteUnspentIndexs(const std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue > >&vect) {
    CDBBatch batch(*this);
    for (auto it=vect.begin(); it!=vect.end(); it++) {
        if (it->second.IsNull()) {
            batch.Erase(std::make_pair(DB_ADDRESSUNSPENTINDEX, it->first));
        } else {
            batch.Write(std::make_pair(DB_ADDRESSUNSPENTINDEX, it->first), it->second);
        }
    }
    
    return WriteBatch(batch);
}

bool AddressIndex::DB::DeleteUnspentIndexs(const std::vector<CAddressUnspentKey>&vect)
{
    CDBBatch batch(*this);
    for (auto it=vect.begin(); it!=vect.end(); it++) {
           batch.Erase(std::make_pair(DB_ADDRESSUNSPENTINDEX, *it));
    }
    
    return WriteBatch(batch);
}

bool AddressIndex::DB::ReadIndex(uint160 addressHash, int addressType, std::vector<std::pair<CAddressIndexKey, CAmount> > &outputs)
{
    std::unique_ptr<CDBIterator> pcursor(NewIterator());

    pcursor->Seek(std::make_pair(DB_ADDRESSINDEX, CAddressIndexIteratorKey(addressType, addressHash)));

    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char,CAddressIndexKey> key;
        if (pcursor->GetKey(key) && key.first == DB_ADDRESSINDEX && key.second.hashBytes == addressHash) {
            CAmount nValue;
            if (pcursor->GetValue(nValue)) {
                outputs.push_back(std::make_pair(key.second, nValue));
                pcursor->Next();
            } else {
                return error("failed to get address unspent value");
            }
        } else {
            break;
        }
    }

    return true;
}

bool AddressIndex::DB::WriteIndexs(const std::vector<std::pair<CAddressIndexKey, CAmount > >&vect)
{
    CDBBatch batch(*this);
    for (auto it=vect.begin(); it!=vect.end(); it++) {
        batch.Write(std::make_pair(DB_ADDRESSINDEX, it->first), it->second);
    }
    
    return WriteBatch(batch);
}

bool AddressIndex::DB::WriteWriteDelete(
        const std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue > >&wuout,
        const std::vector<std::pair<CAddressIndexKey, CAmount > >&wout,
        const std::vector<CAddressUnspentKey>&duout
        )
{
    CDBBatch batch(*this);
    for (auto it=wuout.begin(); it!=wuout.end(); it++) {
        if (it->second.IsNull()) {
            batch.Erase(std::make_pair(DB_ADDRESSUNSPENTINDEX, it->first));
        } else {
            batch.Write(std::make_pair(DB_ADDRESSUNSPENTINDEX, it->first), it->second);
        }
    }

    for (auto it=wout.begin(); it!=wout.end(); it++) {
        batch.Write(std::make_pair(DB_ADDRESSINDEX, it->first), it->second);
    }

    for (auto it=duout.begin(); it!=duout.end(); it++) {
           batch.Erase(std::make_pair(DB_ADDRESSUNSPENTINDEX, *it));
    }
    
    return WriteBatch(batch);
}

bool AddressIndex::Init()
{
    return BaseIndex::Init();
}

typedef std::vector<unsigned char> valtype;

bool AddressIndex::WriteBlock(const CBlock& block, const CBlockIndex* pindex) 
{
    // Exclude genesis block transaction because outputs are not spendable.
    if (pindex->nHeight == 0) return true;
    
    std::vector<std::pair<CAddressIndexKey, CAmount>> list_to_add;
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue>> list_unspent_to_add;
    std::vector<CAddressUnspentKey> list_to_remove;

    CDiskTxPos pos(pindex->GetBlockPos(), GetSizeOfCompactSize(block.vtx.size()));
    std::vector<std::pair<uint256, CDiskTxPos>> vPos;
    vPos.reserve(block.vtx.size());
    int txindex = 0;
    for (const auto& tx : block.vtx) {
        CAddressUnspentKey   addrunspentkey;
        CAddressUnspentValue addrval;
        CAddressIndexKey     addrkey;
        CAmount              amount;
        
        addrunspentkey.txhash = tx -> GetHash();
        addrkey.txhash        = tx -> GetHash();
        addrkey.txindex       = txindex++;
        
        addrkey.blockHeight   = pindex->nHeight;
        
        // add each spendable unspent output in this transaction (*tx)
        //  to the index
        int voutindex = 0;
        for(const CTxOut& o : tx->vout)
        {
            if(o.scriptPubKey.IsUnspendable()) continue;
            
            addrkey.index        = voutindex;            // index of this utxo in the txout table
            addrunspentkey.index = voutindex++;          // index of this utxo in the txout table
            
            addrval.satoshis     = o.nValue;
            addrval.blockHeight  = pindex->nHeight;
            addrval.script       = o.scriptPubKey;
            
            amount               = o.nValue;
            
            std::vector<std::vector<unsigned char>> vSolutions;
            
            txnouttype whichType = Solver(o.scriptPubKey, vSolutions);
            
            if (whichType == TX_PUBKEYHASH || whichType == TX_SCRIPTHASH || whichType == TX_WITNESS_V0_KEYHASH)
            {
                addrunspentkey.hashBytes = uint160(vSolutions[0]);
                addrunspentkey.type      = whichType;
                
                addrkey.hashBytes = uint160(vSolutions[0]);
                addrkey.type         = whichType;
                
                list_to_add.push_back(std::make_pair(addrkey, amount));

                list_unspent_to_add.push_back(std::make_pair(addrunspentkey, addrval));
            }
        }
         
        // then
        // for each input
        //   remove from the index, the old output consumed by this input
        
        for(const CTxIn& xin : tx->vin )
        {
            addrunspentkey.txhash = xin.prevout.hash;
            addrunspentkey.index  = xin.prevout.n;
            
            //  get the txout by txhash and index (using txindex)
            //  then use it for address hshbytes and type extraction
            
            uint256 block_hash;
            CTransactionRef tx_in;
            if(!g_txindex->FindTx(addrunspentkey.txhash, block_hash, tx_in))
            {
                //  TODO LogPrintf("ERROR Getting txin's txout\n");
                continue;
            }
            
            std::vector<std::vector<unsigned char>> vSolutions;
            
            txnouttype whichType = Solver(tx_in->vout[addrunspentkey.index].scriptPubKey, vSolutions);
            if (whichType == TX_PUBKEYHASH || whichType == TX_SCRIPTHASH || whichType == TX_WITNESS_V0_KEYHASH)
            {
                addrunspentkey.hashBytes = uint160(vSolutions[0]);
                addrunspentkey.type   = whichType;
                
            }
            else
            {
                continue;
            }
            
            list_to_remove.push_back(addrunspentkey);
        }
    }
    
    return m_db->WriteWriteDelete(list_unspent_to_add, list_to_add, list_to_remove);
    
    /*
    if(! m_db->WriteUnspentIndexs(list_unspent_to_add))
        return false;
    
    if(! m_db->WriteIndexs(list_to_add))
        return false;
    
    return m_db->DeleteUnspentIndexs(list_to_remove);
    */
}

BaseIndex::DB& AddressIndex::GetDB() const { return *m_db; }


AddressIndex::AddressIndex(size_t n_cache_size, bool f_memory, bool f_wipe)
    : m_db(MakeUnique<AddressIndex::DB>(n_cache_size, f_memory, f_wipe))
{}

AddressIndex::~AddressIndex() {}

bool AddressIndex::GetAddressUnspent(uint160 addressHash, int type, std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs )
{

    if (!m_db->ReadUnspentIndex(addressHash, type, unspentOutputs))
        return error("unable to get txids for address");

    return true;
}

bool AddressIndex::GetAddressIndex(uint160 addressHash, int type, std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndex)
{
    if (!m_db->ReadIndex(addressHash, type, addressIndex))
        return error("unable to get txids for address");

    return true;
}

struct CAddressIndexIteratorHeightKey {
    unsigned int type;
    uint160 hashBytes;
    int blockHeight;

    size_t GetSerializeSize() const {
        return 25;
    }
    template<typename Stream>
    void Serialize(Stream& s) const {
        ser_writedata8(s, type);
        hashBytes.Serialize(s);
        ser_writedata32be(s, blockHeight);
    }
    template<typename Stream>
    void Unserialize(Stream& s) {
        type = ser_readdata8(s);
        hashBytes.Unserialize(s);
        blockHeight = ser_readdata32be(s);
    }

    CAddressIndexIteratorHeightKey(unsigned int addressType, uint160 addressHash, int height) {
        type = addressType;
        hashBytes = addressHash;
        blockHeight = height;
    }


    CAddressIndexIteratorHeightKey() {
        SetNull();
    }

    void SetNull() {
        type = 0;
        hashBytes.SetNull();
        blockHeight = 0;
    }
};

// RPC interface

bool GetAddressUnspent(uint160 addressHash, int type,
                       std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs)
{
    if (!g_addressindex)
        return error("address index not enabled");
    
    if (!g_addressindex->GetAddressUnspent(addressHash, type, unspentOutputs))
        return error("unable to get txids for address");
    
    return true;
}

bool GetAddressIndex(uint160 addressHash, int type,
                     std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndex)
{
    if (!g_addressindex)
        return error("address index not enabled");

    if (!g_addressindex->GetAddressIndex(addressHash, type, addressIndex))
        return error("unable to get txids for address");

    return true;
}

bool AddressToHashType(std::string addr_str, uint160& hashBytes, int& type)
{
    CTxDestination dest = DecodeDestination(addr_str);
    //typedef boost::variant<CNoDestination, CKeyID, CScriptID, WitnessV0ScriptHash, WitnessV0KeyHash, WitnessUnknown> CTxDestination;
    switch(dest.which())
    {
        case 1: // CKeyID
        {
            CKeyID k = boost::get<CKeyID> (dest);
            memcpy(&hashBytes, k.begin(), 20);
            type = TX_PUBKEYHASH;
            break;
        }
        
        case 2: // CScriptID
        {
            CScriptID k = boost::get<CScriptID> (dest);
            memcpy(&hashBytes, k.begin(), 20);
            type = TX_SCRIPTHASH;
            break;
        }
        
        case 4: // WitnessV0KeyHash
        {
            WitnessV0KeyHash w = boost::get<WitnessV0KeyHash>(dest);
            memcpy(&hashBytes, w.begin(), 20);
            type = TX_WITNESS_V0_KEYHASH;
           break;
        }
        
        default :
            return false;
    }
    
    return true;
}


bool HashTypeToAddress(const uint160 &hash, const int &type, std::string &address)
{
    // (whichType == TX_PUBKEYHASH || whichType == TX_SCRIPTHASH || whichType == TX_WITNESS_V0_KEYHASH)
 
    if (type == TX_SCRIPTHASH) {
        address = EncodeDestination(CScriptID(hash));
    } else if (type == TX_PUBKEYHASH) {
        address = EncodeDestination(CKeyID(hash));
    } else if (type == TX_WITNESS_V0_KEYHASH) {
        address = EncodeDestination(WitnessV0KeyHash(hash));
    } else {
        return false;
    }
    return true;
}

bool heightSort(std::pair<CAddressUnspentKey, CAddressUnspentValue> a,
                std::pair<CAddressUnspentKey, CAddressUnspentValue> b) {
    return a.second.blockHeight < b.second.blockHeight;
}

std::vector<uint256> GetAddressesTxs(std::vector<std::pair<uint160, int>> &addresses)
{

    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;

    for (auto it = addresses.begin(); it != addresses.end(); it++) {

        if (!GetAddressIndex((*it).first, (*it).second, addressIndex)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");

        }
    }

    std::set<std::pair<int, uint256> > txids;
    std::vector<uint256> result;

    for (auto it = addressIndex.begin(); it != addressIndex.end(); it++) {
        int height = it->first.blockHeight;

        txids.insert(std::make_pair(height, it->first.txhash));
    }

    for (auto it=txids.begin(); it!=txids.end(); it++) {
        result.push_back(it->second);
    }

    return result;
}

bool IsAddressesHasTxs(std::vector<std::pair<uint160, int>> &addresses)
{

    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;

    for (auto it = addresses.begin(); it != addresses.end(); it++) {

        if (!GetAddressIndex((*it).first, (*it).second, addressIndex)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");

        }
        
        if(addressIndex.size() > 0) return true;
    }
    
    return addressIndex.size() > 0;

}

UniValue GetAddressesUtxos(std::vector<std::pair<uint160, int>> &addresses)
{
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {

            if (!GetAddressUnspent((*it).first, (*it).second, unspentOutputs)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
    }

    std::sort(unspentOutputs.begin(), unspentOutputs.end(), heightSort);

    UniValue utxos(UniValue::VARR);

    for (auto it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++) {
        UniValue output(UniValue::VOBJ);
        
        std::string address;
        
        if (!HashTypeToAddress(it->first.hashBytes, it->first.type, address)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown address type");
        }

        output.pushKV("address", address);
        output.pushKV("txid", it->first.txhash.GetHex());
        output.pushKV("outputIndex", (int)it->first.index);
        output.pushKV("script", HexStr(it->second.script.begin(), it->second.script.end()));
        output.pushKV("satoshis", it->second.satoshis);
        output.pushKV("height", it->second.blockHeight);
        utxos.push_back(output);
    }

    return utxos;
}

bool GetAddressesUtxos(std::vector<std::pair<uint160, int>> &addresses, CDataStream& ss, uint32_t& count)
{
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {

            if (!GetAddressUnspent((*it).first, (*it).second, unspentOutputs)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
    }

    std::sort(unspentOutputs.begin(), unspentOutputs.end(), heightSort);

    count += unspentOutputs.size();

    for (auto it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++) {
        
        ss << *it;
    }

    return unspentOutputs.size() > 0;
}

int GetLastUsedIndex(std::vector<std::pair<uint160, int>> &addresses)
{
    int r = -1;
    int index = 0;
    std::vector<std::pair<CAddressIndexKey, CAmount> > txOutputs;

    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {

        if (!GetAddressIndex((*it).first, (*it).second, txOutputs)) {
             throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
        }
        if(txOutputs.size() > 0)
        {
            r = index;
            txOutputs.clear();
        }

                
        index++;
    }
    
    return r;
}
