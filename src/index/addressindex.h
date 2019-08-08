#ifndef _bitcoin2_addressindex_h_
#define _bitcoin2_addressindex_h_


#include <chain.h>
#include <index/base.h>
#include <txdb.h>

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

    /// Look up a transaction by hash.
    ///
    /// @param[in]   tx_hash  The hash of the transaction to be returned.
    /// @param[out]  block_hash  The hash of the block the transaction is found in.
    /// @param[out]  tx  The transaction itself.
    /// @return  true if transaction is found, false otherwise
    bool FindAddress(const uint160& addressHash) const;
};

/// The global addresses index, used in GetTransaction. May be null.
extern std::unique_ptr<AddressIndex> g_addressindex;



#endif
