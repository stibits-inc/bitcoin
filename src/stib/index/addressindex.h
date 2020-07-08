#ifndef _bitcoin2_addressindex_h_
#define _bitcoin2_addressindex_h_

#include <chain.h>
#include <index/base.h>
#include <rpc/util.h>


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


/** Access to the addresses index database (index/addressindex) */
class CAddressIndex : public CDBWrapper
{
public:
    explicit CAddressIndex(size_t nCacheSize, bool fMemory = false, bool fWipe = false);
    
    CAddressIndex(const CAddressIndex&) = delete;
    CAddressIndex& operator=(const CAddressIndex&) = delete;

    bool ReadAddressUnspentIndex(uint160 addressHash, int type,
                                 std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &vect);
                                 
    bool ReadAddressIndex(uint160 addressHash, int type,
                          std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndex,
                          unsigned int start = 0, unsigned int end = 0);
                          
    // bool UpdateAddressUnspentIndex(const std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue > >&vect);

    // bool WriteAddressIndex(const std::vector<std::pair<CAddressIndexKey, CAmount> > &vect);
    // bool EraseAddressIndex(const std::vector<std::pair<CAddressIndexKey, CAmount> > &vect);


private:
    //std::vector<std::pair<CAddressIndexKey, CAmount>> aToWrite;
    //std::vector<CAddressIndexKey> aToErase;
    //std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue>> uToWrite;
    //std::vector<CAddressUnspentKey> uToErase;
    
    CDBBatch  batch;

     void Clear();
public:
    void Begin();
    bool Commit();

    void Write(CAddressIndexKey, CAmount);
    void Erase(CAddressIndexKey);
    
    void Write(CAddressUnspentKey, CAddressUnspentValue);
    void Erase(CAddressUnspentKey);
};


/// The global addresses index. May be null.
extern std::unique_ptr<CAddressIndex> g_addressindex;


bool IsAddressesHasTxs(std::vector<std::pair<uint160, int>> &addresses);
UniValue GetAddressesUtxos(std::vector<std::pair<uint160, int>> &addresses);
bool GetAddressesUtxos(std::vector<std::pair<uint160, int>> &addresses, CDataStream& ss, uint32_t& count);
std::vector<uint256>  GetAddressesTxs(std::vector<std::pair<uint160, int>> &addresses);
int GetLastUsedIndex(std::vector<std::pair<uint160, int>> &addresses);
int  GetFirstBlockHeightForAddresses(std::vector<std::pair<uint160, int>> &addresses);
#endif
