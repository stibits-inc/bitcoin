#ifndef _bitcoin2_addressindex_h_
#define _bitcoin2_addressindex_h_


#include <chain.h>
#include <index/base.h>
#include <txdb.h>
#include <rpc/util.h>

struct CAddressUnspentKey;
struct CAddressUnspentValue;
struct CAddressIndexKey;

/**
 * TxIndex is used to look up transactions included in the blockchain by hash.
 * The index is written to a LevelDB database and records the filesystem
 * location of each transaction by transaction hash.
 */
class AddressIndex final : public BaseIndex
{
protected:
    class DB;

private:
    const std::unique_ptr<DB> m_db;

protected:
    /// Override base class init to migrate from old database.
    bool Init() override;

    bool WriteBlock(const CBlock& block, const CBlockIndex* pindex) override;

    BaseIndex::DB& GetDB() const override;

    const char* GetName() const override { return "addressindex"; }

public:
    /// Constructs the index, which becomes available to be queried.
    explicit AddressIndex(size_t n_cache_size, bool f_memory = false, bool f_wipe = false);

    // Destructor is declared because this class contains a unique_ptr to an incomplete type.
    virtual ~AddressIndex() override;
   
    bool GetAddressUnspent(
                       uint160 addressHash,
                       int type,
                       std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs
                       );


    bool GetAddressIndex(uint160 addressHash,
                         int type,
                         std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndex
                        );
                                                           
};

/// The global addresses index, used in GetTransaction. May be null.
extern std::unique_ptr<AddressIndex> g_addressindex;

bool AddressToHashType(std::string addr_str, uint160& hashBytes, int& type);
UniValue GetAddressesUtxos(std::vector<std::pair<uint160, int>> &addresses);
UniValue GetAddressesTxs(std::vector<std::pair<uint160, int>> &addresses);
int GetLastUsedIndex(std::vector<std::pair<uint160, int>> &addresses);

#endif
