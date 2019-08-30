// Copyright (c) 2017-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <index/addressindex.h>
#include <shutdown.h>
#include <ui_interface.h>
#include <util/system.h>
#include <validation.h>

#include <key_io.h>
#include <rpc/blockchain.h>
#include <rpc/server.h>
#include <rpc/util.h>

#include <script/standard.h>
#include <pubkey.h>

#include <boost/thread.hpp>

#include <util/strencodings.h>



//constexpr char DB_BEST_BLOCK = 'B';
//constexpr char DB_ADDRESSINDEX = 'a';
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
        //std::string r = ss;
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
                                           std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs);

    /// Write a batch of transaction positions to the DB.
    bool WriteUnspentIndexs(const std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue > >&vect) ;

    /// Delete a batch of transaction positions from the DB.
    bool DeleteUnspentIndexs(const std::vector<CAddressUnspentKey>&vect) ;

};

AddressIndex::DB::DB(size_t n_cache_size, bool f_memory, bool f_wipe) :
    BaseIndex::DB(GetDataDir() / "indexes" / "addressindex", n_cache_size, f_memory, f_wipe)
{}

bool AddressIndex::DB::ReadUnspentIndex(uint160 addressHash, int addressType,
                                           std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs)
{
	
    std::unique_ptr<CDBIterator> pcursor(NewIterator());
    
    LogPrintf("Reading key = %s\n", CAddressIndexIteratorKey(addressType, addressHash).ToString().c_str() );


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
    for (std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> >::const_iterator it=vect.begin(); it!=vect.end(); it++) {
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


bool AddressIndex::Init()
{
    return BaseIndex::Init();
}

typedef std::vector<unsigned char> valtype;

bool AddressIndex::WriteBlock(const CBlock& block, const CBlockIndex* pindex) 
{
    // Exclude genesis block transaction because outputs are not spendable.
    if (pindex->nHeight == 0) return true;
    
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue>> list_to_add;
    std::vector<CAddressUnspentKey> list_to_remove;

    CDiskTxPos pos(pindex->GetBlockPos(), GetSizeOfCompactSize(block.vtx.size()));
    std::vector<std::pair<uint256, CDiskTxPos>> vPos;
    vPos.reserve(block.vtx.size());
    for (const auto& tx : block.vtx) {
        CAddressUnspentKey addrkey;
        CAddressUnspentValue addrval;
        addrkey.txhash = tx -> GetHash();
        
        // add each spendable unspent output in this transaction (*tx)
        //  to the index
        int ndex = 0;
        for(const CTxOut& o : tx->vout)
        {
            if(o.scriptPubKey.IsUnspendable()) continue;
            
            addrkey.index = ndex++;      // index of this utxo in the txout table
            
            addrval.satoshis = o.nValue;
            addrval.blockHeight = pindex->nHeight;
            addrval.script = o.scriptPubKey;// = ??; TODO
            //pindex->
            
		    std::vector<std::vector<unsigned char>> vSolutions;
		    
		    txnouttype whichType = Solver(o.scriptPubKey, vSolutions);
		    
		    if (whichType == TX_PUBKEYHASH || whichType == TX_SCRIPTHASH || whichType == TX_WITNESS_V0_KEYHASH)
		    {
		        addrkey.hashBytes = uint160(vSolutions[0]);
		        addrkey.type	  = whichType;
		    }
		    else
		    {
		        continue;
		    }
		    
		    // LOG
		    CTxDestination addressRet;
		    ExtractDestination(o.scriptPubKey, addressRet);
		    std::string d = EncodeDestination(addressRet);
		    
		    LogPrintf("type = %d, addr hex = %, addr = %s\n", addrkey.type, addrkey.hashBytes.GetHex().c_str(), d.c_str() );
		    LogPrintf("key = %s\n", addrkey.ToString().c_str() );
            // END LOG
         
            list_to_add.push_back(std::make_pair(addrkey, addrval));
        }
         
        // then
        // for each input
        //   remove from the index, the old output consumed by this input
       
        ndex = 0;
        
        for(const CTxIn& xin : tx->vin )
        {
            addrkey.txhash = xin.prevout.hash;
            addrkey.index  = xin.prevout.n;
            
            
            std::vector<std::vector<unsigned char>> vSolutions;
		    
		    txnouttype whichType = Solver(xin.scriptSig, vSolutions); // TODO check xin.scriptSig
		    if (whichType == TX_PUBKEYHASH || whichType == TX_SCRIPTHASH || whichType == TX_WITNESS_V0_KEYHASH)
		    {
		        addrkey.hashBytes = uint160(vSolutions[0]);
		        addrkey.type	  = whichType;
		        
		    }
		    else
		    {
		        continue;
		    }
		    
            list_to_remove.push_back(addrkey);
        }
    }
    
    if(! m_db->WriteUnspentIndexs(list_to_add))
        return false;
    
    return m_db->DeleteUnspentIndexs(list_to_remove);
    
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


// RPC interface


bool GetAddressUnspent(uint160 addressHash, int type,
                       std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs)
{
    if (!g_addressindex)
        return error("address index not enabled");
    
    // LOG
 /*   CTxDestination addressRet;
    ExtractDestination(o.scriptPubKey, addressRet);
    std::string d = EncodeDestination(addressRet);
 */
 std::string d;
    LogPrintf("type = %d, addr hex = %, addr = %s\n", type, addressHash.GetHex().c_str(), d.c_str() );
    // END LOG
                
    
    if (!g_addressindex->GetAddressUnspent(addressHash, type, unspentOutputs))
        return error("unable to get txids for address");
    
    return true;
}

bool GetIndexKey(std::string addr_str, uint160& hashBytes, int& type)
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
            CKeyID k = boost::get<CKeyID> (dest);
            memcpy(&hashBytes, k.begin(), 20);
            type = TX_SCRIPTHASH;
            break;
        }
        
        case 4:  // WitnessV0KeyHash
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


bool getAddressFromIndex(const int &type, const uint160 &hash, std::string &address)
{
	// (whichType == TX_PUBKEYHASH || whichType == TX_SCRIPTHASH || whichType == TX_WITNESS_V0_KEYHASH)
 
    if (type == TX_SCRIPTHASH) {
        address = (CScriptID(hash)).ToString();
    } else if (type == TX_PUBKEYHASH) {
        address = (CKeyID(hash)).ToString();
    } else if (type == TX_WITNESS_V0_KEYHASH) {
        address = (WitnessV0KeyHash(hash)).ToString();
    } else {
        return false;
    }
    return true;
}

bool getAddressesFromParams(const UniValue& params, std::vector<std::pair<uint160, int> > &addresses)
{
    if (params[0].isStr()) {
        uint160 hashBytes;
        int type = 0;
        if (!GetIndexKey(params[0].get_str(), hashBytes, type)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY,  params[0].get_str() + "Invalid address 1");
        }
        addresses.push_back(std::make_pair(hashBytes, type));
    } else if (params[0].isObject()) {

        UniValue addressValues = find_value(params[0].get_obj(), "addresses");
        if (!addressValues.isArray()) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Addresses is expected to be an array");
        }

        std::vector<UniValue> values = addressValues.getValues();

        for (std::vector<UniValue>::iterator it = values.begin(); it != values.end(); ++it) {

            uint160 hashBytes;
            int type = 0;
            if (!GetIndexKey(it->get_str(), hashBytes, type)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address 2");
            }
            addresses.push_back(std::make_pair(hashBytes, type));
        }
    } else {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address 3");
    }

    return true;
}

bool heightSort(std::pair<CAddressUnspentKey, CAddressUnspentValue> a,
                std::pair<CAddressUnspentKey, CAddressUnspentValue> b) {
    return a.second.blockHeight < b.second.blockHeight;
}


UniValue getaddressutxos(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1)
        throw std::runtime_error(
            "getaddressutxos\n"
            "\nReturns all unspent outputs for an address (requires addressindex to be enabled).\n"
            "\nArguments:\n"
            "{\n"
            "  \"addresses\"\n"
            "    [\n"
            "      \"address\"  (string) The base58check encoded address\n"
            "      ,...\n"
            "    ],\n"
            "  \"chainInfo\",  (boolean, optional, default false) Include chain info with results\n"
            "}\n"
            "\nResult\n"
            "[\n"
            "  {\n"
            "    \"address\"  (string) The address base58check encoded\n"
            "    \"txid\"  (string) The output txid\n"
            "    \"height\"  (number) The block height\n"
            "    \"outputIndex\"  (number) The output index\n"
            "    \"script\"  (strin) The script hex encoded\n"
            "    \"satoshis\"  (number) The number of satoshis of the output\n"
            "  }\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("getaddressutxos", "'{\"addresses\": [\"12c6DSiU4Rq3P4ZxziKxzrL5LmMBrzjrJX\"]}'")
            + HelpExampleRpc("getaddressutxos", "{\"addresses\": [\"12c6DSiU4Rq3P4ZxziKxzrL5LmMBrzjrJX\"]}")
            );

    bool includeChainInfo = false;
    if (request.params[0].isObject()) {
        UniValue chainInfo = find_value(request.params[0].get_obj(), "chainInfo");
        if (chainInfo.isBool()) {
            includeChainInfo = chainInfo.get_bool();
        }
    }
    
    LogPrintf("A....................\n");

    std::vector<std::pair<uint160, int> > addresses;

    if (!getAddressesFromParams(request.params, addresses)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address 4");
    }
   LogPrintf("B....................\n");

    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {

            if (!GetAddressUnspent((*it).first, (*it).second, unspentOutputs)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
    }
   LogPrintf("C.........size = %d .......\n", unspentOutputs.size());

    std::sort(unspentOutputs.begin(), unspentOutputs.end(), heightSort);

    UniValue utxos(UniValue::VARR);

    for (auto it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++) {
        UniValue output(UniValue::VOBJ);
        std::string address;
        if (!getAddressFromIndex(it->first.type, it->first.hashBytes, address)) {
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

    if (includeChainInfo) {
        UniValue result(UniValue::VOBJ);
        result.pushKV("utxos", utxos);

        LOCK(cs_main);
        result.pushKV("hash", chainActive.Tip()->GetBlockHash().GetHex());
        result.pushKV("height", (int)chainActive.Height());
        return result;
    } else {
        return utxos;
    }
}

