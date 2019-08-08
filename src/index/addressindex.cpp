// Copyright (c) 2017-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <index/addressindex.h>
#include <shutdown.h>
#include <ui_interface.h>
#include <util/system.h>
#include <validation.h>

#include <boost/thread.hpp>

constexpr char DB_BEST_BLOCK = 'B';
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

    size_t GetSerializeSize() const {
        return 21;
    }
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
	
}

bool AddressIndex::WriteBlock(const CBlock& block, const CBlockIndex* pindex) 
{
    // Exclude genesis block transaction because outputs are not spendable.
    if (pindex->nHeight == 0) return true;
    
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue>> vect;

    CDiskTxPos pos(pindex->GetBlockPos(), GetSizeOfCompactSize(block.vtx.size()));
    std::vector<std::pair<uint256, CDiskTxPos>> vPos;
    vPos.reserve(block.vtx.size());
    for (const auto& tx : block.vtx) {
        CAddressUnspentKey addrkey;
        CAddressUnspentValue addrval;
        addrkey.txhash = tx -> GetHash();
        
        // TODO for each unspent output in this transaction (*tx)
        //        add an element in the index 'vect'
        for(const auto o : tx->vout)
        {
            addrkey.type;
            addrkey.index;
            addrkey.hashBytes;
            
            o.nValue;
            o.scriptPubKey;
            
            addrval.satoshis = o.nValue;
            addrval.blockHeight = pindex->nHeight;
            //pindex->
        }
        
        // and
        // for each input
        //   remove from the index, the old output consumed by this input
        
        vPos.emplace_back(tx->GetHash(), pos);
        pos.nTxOffset += ::GetSerializeSize(*tx, CLIENT_VERSION);
    }
    
    return m_db->WriteUnspentIndexs(vect);
	
}

BaseIndex::DB& AddressIndex::GetDB() const 
{
	
}

AddressIndex::AddressIndex(size_t n_cache_size, bool f_memory, bool f_wipe)
{
	
}

AddressIndex::~AddressIndex()
{
	
}

bool AddressIndex::FindAddress(const uint160& addressHash) const
{
	
}
