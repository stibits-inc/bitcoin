#include <key.h>
#include <key_io.h>
#include <base58.h>

#include <script/standard.h>
#include <bech32.h>
#include <script/script.h>
#include <util/strencodings.h>
#include <stib/index/addressindex.h>

struct HD_XPub
{
    HD_XPub(const std::string xpub) {SetXPub(xpub);}
    HD_XPub()   {}

    void                        SetXPub         (const std::string xpub_);

    std::vector<std::string>    Derive          (uint32_t from, uint32_t count, bool internal = false);
    std::vector<std::string>    DeriveWitness   (uint32_t from, uint32_t count, bool internal = false);

    std::vector<std::string>    Derive          (uint32_t from, uint32_t count, bool internal, bool segwit)
    {
        return
            segwit ?
                DeriveWitness( from, count, internal)
            :
                Derive       ( from, count, internal);
    }
    
    bool IsValid() {return accountKey.pubkey.IsValid();}

private:
    CExtPubKey accountKey;
};

void HD_XPub::SetXPub(const std::string xpub)
{
    std::vector<unsigned char> ve ;
    if(DecodeBase58Check(xpub, ve))
    {
        ve.erase(ve.begin(), ve.begin() + 4);
        accountKey.Decode(ve.data());
    }

}

static std::string GetAddress(CPubKey& key)
{
    CTxDestination d = CTxDestination(PKHash(key));
    return EncodeDestination(d);
}

static std::string GetBech32Address(CPubKey& key)
{
    CKeyID id = key.GetID();

    std::vector<unsigned char> data = {0};
    data.reserve(33);
    ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, id.begin(), id.end());
    return bech32::Encode(Params().Bech32HRP(), data);
}

std::vector<std::string> HD_XPub::Derive(uint32_t from, uint32_t count, bool internal)
{
    std::vector<std::string> ret(count);

    CExtPubKey chainKey;
    CExtPubKey childKey;

    // derive M/change
    accountKey.Derive(chainKey, internal ? 1 : 0);

    for(uint32_t i = 0; i < count; i++)
    {
        // derive M/change/index
        chainKey.Derive(childKey, from );
        std::string addr = GetAddress(childKey.pubkey );

        from++;
        ret[i] = addr;
    }

    return ret;
}

std::vector<std::string> HD_XPub::DeriveWitness(uint32_t from, uint32_t count, bool internal)
{
    std::vector<std::string> ret(count);

    CExtPubKey chainKey;
    CExtPubKey childKey;

    // derive M/change
    accountKey.Derive(chainKey, internal ? 1 : 0);

    for(uint32_t i = 0; i < count; i++)
    {
        // derive M/change/index
        chainKey.Derive(childKey, from );
        std::string addr = GetBech32Address(childKey.pubkey );

        from++;
        ret[i] = addr;
    }

    return ret;
}

static UniValue& operator <<(UniValue& arr, const UniValue& a) {
    for(size_t i = 0; i < a.size(); i++)
    {
        arr.push_back(a[i]);
    }

    return arr;
}

static std::vector<std::string>& operator <<(std::vector<std::string>& arr, const UniValue& a) {
    for(size_t i = 0; i < a.size(); i++)
    {
        arr.push_back(a[i].write());
    }

    return arr;
}

static std::vector<uint256>& operator <<(std::vector<uint256>& arr, const std::vector<uint256>& a) {
    for(size_t i = 0; i < a.size(); i++)
    {
        arr.push_back(a[i]);
    }

    return arr;
}

static bool AddressToHashType(std::string str, uint160& hashBytes, int& type)
{
    std::vector<unsigned char> data;
    const CChainParams &params = Params();
    uint160 hash;
    if (DecodeBase58Check(str, data)) {
        // base58-encoded Bitcoin addresses.
        // Public-key-hash-addresses have version 0 (or 111 testnet).
        // The data vector contains RIPEMD160(SHA256(pubkey)), where pubkey is the serialized public key.
        const std::vector<unsigned char>& pubkey_prefix = params.Base58Prefix(CChainParams::PUBKEY_ADDRESS);
        if (data.size() == hash.size() + pubkey_prefix.size() && std::equal(pubkey_prefix.begin(), pubkey_prefix.end(), data.begin())) {
            std::copy(data.begin() + pubkey_prefix.size(), data.end(), hash.begin());
            hashBytes = PKHash(hash);
            type = TX_PUBKEYHASH;
            return true;
        }
        // Script-hash-addresses have version 5 (or 196 testnet).
        // The data vector contains RIPEMD160(SHA256(cscript)), where cscript is the serialized redemption script.
        const std::vector<unsigned char>& script_prefix = params.Base58Prefix(CChainParams::SCRIPT_ADDRESS);
        if (data.size() == hash.size() + script_prefix.size() && std::equal(script_prefix.begin(), script_prefix.end(), data.begin())) {
            std::copy(data.begin() + script_prefix.size(), data.end(), hash.begin());
            hashBytes = ScriptHash(hash);
            type = TX_SCRIPTHASH;
            return true;
        }
    }
    data.clear();
    auto bech = bech32::Decode(str);
    if (bech.second.size() > 0 && bech.first == params.Bech32HRP()) {
        // Bech32 decoding
        int version = bech.second[0]; // The first 5 bit symbol is the witness version (0-16)
        // The rest of the symbols are converted witness program bytes.
        data.reserve(((bech.second.size() - 1) * 5) / 8);
        if (ConvertBits<5, 8, false>([&](unsigned char c) { data.push_back(c); }, bech.second.begin() + 1, bech.second.end())) {
            if (version == 0) {
                {
                    WitnessV0KeyHash keyid;
                    if (data.size() == keyid.size()) {
                        std::copy(data.begin(), data.end(), keyid.begin());
                        
                        type = TX_WITNESS_V0_KEYHASH;
                        hashBytes =  keyid;
                        return true;
                    }
                }
                /*
                {
                    WitnessV0ScriptHash scriptid;
                    if (data.size() == scriptid.size()) {
                        std::copy(data.begin(), data.end(), scriptid.begin());
                        type = TX_WITNESS_V0_SCRIPTHASH;
                        hashBytes =  scriptid;
                        return true;
                    }
                }
                */
                return false;
            }
            
            return false;
        }
    }
    return false;
}

#define BLOCK_SIZE 100

int GetLastUsedExternalSegWitIndex(std::string& xpubkey)
{
     int ret = -1;
     uint32_t last =  0;
     HD_XPub hd(xpubkey);
     
     if(!hd.IsValid()) return ret;

     do
     {
         std::vector<std::string> addrs = hd.Derive(last, BLOCK_SIZE, false, true);
         std::vector<std::pair<uint160, int> > addresses;

         for(auto a : addrs)
         {
            uint160 hashBytes;
            int type = 0;
            if (AddressToHashType(a, hashBytes, type)) {
                addresses.push_back(std::make_pair(hashBytes, type));
            }
         }
         LogPrintf("calling GetLastUsedIndex\n");

         int r = GetLastUsedIndex(addresses);
         
         LogPrintf("called GetLastUsedIndex, that return %d\n", r);
    
         if(r < 0) return ret+1;
         ret = last + r;

         last += BLOCK_SIZE;

     } while(true);

     return ret;
}

UniValue Recover_(HD_XPub& hd, bool internal, bool segwit)
{
    /*
     * repeat
     *    derive 100 next address
     *    get their utxos
     *    if no utxo found
     *       get their txs
     * while there is at least ( one utxo or one tx)
     *
     */

     UniValue ret(UniValue::VARR);

     uint32_t last =  0;

     int not_found = 0;

     bool found = false;

     do
     {
         std::vector<std::string> addrs = hd.Derive(last, BLOCK_SIZE, internal, segwit);
         std::vector<std::pair<uint160, int> > addresses;

         for(auto a : addrs)
         {
            uint160 hashBytes;
            int type = 0;
            if (AddressToHashType(a, hashBytes, type)) {
                addresses.push_back(std::make_pair(hashBytes, type));
            }
         }

         UniValue utxos = GetAddressesUtxos(addresses);

         if(utxos.size() == 0)
         {
             found = IsAddressesHasTxs(addresses);

         }
         else
         {
             ret << utxos;
             found = true;
         }

         last += BLOCK_SIZE;

         not_found = found ? 0 : not_found + BLOCK_SIZE;


     } while(not_found < 100);

     return ret;
}

uint32_t Recover_(HD_XPub& hd, bool internal, bool segwit, CDataStream& ss)
{
    /*
     * repeat
     *    derive 100 next address
     *    get their utxos
     *    if no utxo found
     *       get their txs
     * while there is at least ( one utxo or one tx)
     *
     */

     uint32_t last =  0;

     int not_found = 0;
     
     uint32_t count = 0;

     bool found = false;

     do
     {
         std::vector<std::string> addrs = hd.Derive(last, BLOCK_SIZE, internal, segwit);
         std::vector<std::pair<uint160, int> > addresses;

         for(auto a : addrs)
         {
            uint160 hashBytes;
            int type = 0;
            if (AddressToHashType(a, hashBytes, type)) {
                addresses.push_back(std::make_pair(hashBytes, type));
            }
            else
            {
                error("Error when convert from address %s to hash", a);
            }
         }

         if(!GetAddressesUtxos(addresses, ss, count))
         {
             found = IsAddressesHasTxs(addresses);

         }
         else
         {
             found = true;
         }

         last += BLOCK_SIZE;

         not_found = found ? 0 : not_found + BLOCK_SIZE;


     } while(not_found < 100);
     
     return count;
}

std::vector<uint256> RecoverTxs_(HD_XPub& hd, bool internal, bool segwit)
{
    /*
     * repeat
     *    derive 100 next address
     *    gget their txs
     * while there is at least ( one tx)
     *
     */

     std::vector<uint256>  ret;

     uint32_t last =  0;

     int not_found = 0;

     bool found = false;

     do
     {
         std::vector<std::string> addrs = hd.Derive(last, BLOCK_SIZE, internal, segwit);
         std::vector<std::pair<uint160, int> > addresses;

         for(auto a : addrs)
         {
            uint160 hashBytes;
            int type = 0;
            if (AddressToHashType(a, hashBytes, type)) {
                addresses.push_back(std::make_pair(hashBytes, type));
            }
         }

         std::vector<uint256>  txs = GetAddressesTxs(addresses);

         if(txs.size() > 0)
         {
             ret << txs;
             found = true;
         }
         else
         {
            found = false;
         }

         last += BLOCK_SIZE;

         not_found = found ? 0 : not_found + BLOCK_SIZE;


     } while(not_found < 100);

     return ret;
}

void GenerateFromXPUB(std::string xpubkey, int from, int count, UniValue& out)
{
    HD_XPub xpub(xpubkey);

    std::vector<std::string> v = xpub.Derive(from, count, false, true);

    for(auto addr : v)
    {
        out.push_back(addr);
    }
}

void GenerateFromXPUB(std::string xpubkey, int from, int count, std::vector<std::string>& out)
{
    HD_XPub xpub(xpubkey);

    std::vector<std::string> v = xpub.Derive(from, count, false, true);

    for(auto addr : v)
    {
        out.push_back(addr);
    }
}


void GenerateFromXPUB(std::string xpubkey, int from, int count, CDataStream& ss)
{
    HD_XPub xpub(xpubkey);

    std::vector<std::string> v = xpub.Derive(from, count, false, true);
    
    ss << v;
}

void RecoverFromXPUB(std::string xpubkey, UniValue& out)
{
    HD_XPub xpub(xpubkey);

    out   << Recover_(xpub, false, true)
          << Recover_(xpub, false, false)
          << Recover_(xpub, true, false)
          << Recover_(xpub, true, true);
}

uint32_t RecoverFromXPUB(std::string xpubkey, CDataStream& ss)
{
    HD_XPub xpub(xpubkey);
    
    uint32_t count =
            Recover_(xpub, false, true, ss)
          + Recover_(xpub, false, false, ss)
          + Recover_(xpub, true, false, ss)
          + Recover_(xpub, true, true, ss)
          ;
    return count;
}


void RecoverFromXPUB(std::string xpubkey, std::vector<std::string>& out)
{
    HD_XPub xpub(xpubkey);

    out   << Recover_(xpub, false, true)
          << Recover_(xpub, false, false)
          << Recover_(xpub, true, false)
          << Recover_(xpub, true, true);
}

void RecoverTxsFromXPUB(std::string xpubkey, std::vector<uint256>& out)
{
    HD_XPub xpub(xpubkey);

    out   << RecoverTxs_(xpub, false, true)
          << RecoverTxs_(xpub, false, false)
          << RecoverTxs_(xpub, true, false)
          << RecoverTxs_(xpub, true, true);
}
