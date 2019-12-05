#ifndef _bitcoin2_addressindex_h_
#define _bitcoin2_addressindex_h_

#include <chain.h>
#include <index/base.h>
#include <rpc/util.h>

struct CAddressUnspentKey;
struct CAddressUnspentValue;
struct CAddressIndexKey;

/**
 * AddressIndex is used to look up addresses information included in the blockchain.
 * The index is written to a LevelDB database and records the filesystem
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

/// The global addresses index. May be null.
extern std::unique_ptr<AddressIndex> g_addressindex;

bool AddressToHashType(std::string addr_str, uint160& hashBytes, int& type);
bool IsAddressesHasTxs(std::vector<std::pair<uint160, int>> &addresses);
UniValue GetAddressesUtxos(std::vector<std::pair<uint160, int>> &addresses);
bool GetAddressesUtxos(std::vector<std::pair<uint160, int>> &addresses, CDataStream& ss, uint32_t& count);
std::vector<uint256>  GetAddressesTxs(std::vector<std::pair<uint160, int>> &addresses);
int GetLastUsedIndex(std::vector<std::pair<uint160, int>> &addresses);

#endif
