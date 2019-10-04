#include <key.h>
#include <key_io.h>
#include <base58.h>
#include <thread>
#include <script/standard.h>
#include <bech32.h>
#include <script/script.h>
#include <util/strencodings.h>
#include <rpc/blockchain.h>
#include <rpc/server.h>
#include <rpc/util.h>
#include <index/addressindex.h>

#include <logging.h>


struct HD_XPub
{
	HD_XPub(const std::string xpub)	{SetXPub(xpub);}
	HD_XPub()	{}
	
	
	void						SetXPub         (const std::string xpub_);
	
	std::vector<std::string>	Derive          (uint32_t from, uint32_t count, bool internal = false);
	std::vector<std::string>	DeriveWitness   (uint32_t from, uint32_t count, bool internal = false);

	std::vector<std::string>	Derive          (uint32_t from, uint32_t count, bool internal, bool segwit)
	{
		return
			segwit ?
				DeriveWitness( from, count, internal)
			:
				Derive       ( from, count, internal);
	}
	
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
		CKeyID id = key.GetID();
		CTxDestination d = id;
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

#define BLOCK_SIZE 100

int GetLastUsedExternalSegWitIndex(HD_XPub& hd)
{
	 int ret = -1;
	 uint32_t last =  0;
	 
	 do
	 {
		 std::vector<std::string> addrs = hd.Derive(last, BLOCK_SIZE, false, true);
		 std::vector<std::pair<uint160, int> > addresses;
		 
		 for(auto a : addrs)
		 {
		     LogPrintf("%s\n", a.data());
		    uint160 hashBytes;
	        int type = 0;
	        if (AddressToHashType(a, hashBytes, type)) {
	            addresses.push_back(std::make_pair(hashBytes, type));
	        }
		 }
		 
		 int r = GetLastUsedIndex(addresses);
		 
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
		     UniValue txs = GetAddressesTxs(addresses);
			 found = txs.size() > 0;
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

void RecoverFromXPUB(std::string xpubkey, UniValue& out)
{
    HD_XPub xpub(xpubkey);
        
    out   << Recover_(xpub, false, true)
          << Recover_(xpub, false, false)
          << Recover_(xpub, true, false)
          << Recover_(xpub, true, true);
}

void RecoverFromXPUB(std::string xpubkey, std::vector<std::string>& out)
{
    HD_XPub xpub(xpubkey);
        
    out   << Recover_(xpub, false, true)
          << Recover_(xpub, false, false)
          << Recover_(xpub, true, false)
          << Recover_(xpub, true, true);
}

// RPC
UniValue stibgenxpubaddresses(const JSONRPCRequest& request)
{
   if (request.fHelp || request.params.size() < 1  || request.params.size() > 1)
        throw std::runtime_error(
            "stibgenxpubaddresses\n"
            "\nReturns 'count' HD generated address for an 'xpub', starting  from 'start' index.\n"
            "\nArguments:\n"
            "{\n"
            "  \"xpubkey\",  account extended public key ExtPubKey\n"
            "  \"start\", (optional default to 0) index of the first address to generate\n"
            "  \"count\", (optional default to 100) numbre of addresses to generate\n"
            "  \"for_change\", (boolean optional default to false ) generate addresses for internal use(true), or for external use(false)?\n"
            "  \"use_bech32\", (boolean optional default to true ) generate witness addresses(true) or legacy addresses(false)?\n"
            "}\n"
            "\nResult\n"
            "[\n"
            "      \"address\"  (string) The base58check encoded address\n"
            "      ,...\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("stibgenxpubaddresses", "'{\"xpubkey\": \"xpub6Bgu572Y3EWgEq8gkVxmznPkb8hWkgYR9E6KTZN3pyM3hhC7WvwgHNchSCrC19a7nZ3ddyjwB26rbePuyATc55snUwWKkszRnvVwfmBshdS\"}'")
            + HelpExampleRpc("stibgenxpubaddresses", "{\"xpubkey\": \"xpub6Bgu572Y3EWgEq8gkVxmznPkb8hWkgYR9E6KTZN3pyM3hhC7WvwgHNchSCrC19a7nZ3ddyjwB26rbePuyATc55snUwWKkszRnvVwfmBshdS\"}")
            );
   
	std::string xpubkey;
    bool for_change = false;
    bool segwit = true;
    int  from = 0;
    int  count = 100;

    if (request.params[0].isObject()) {
        UniValue val = find_value(request.params[0].get_obj(), "xpubkey");
        if (val.isStr()) {
            xpubkey = val.get_str();
        }
        
        val = find_value(request.params[0].get_obj(), "for_change");
        if (val.isBool()) {
            for_change = val.get_bool();
        }
        
        
        val = find_value(request.params[0].get_obj(), "use_bech32");
        if (val.isBool()) {
            segwit = val.get_bool();
        }
        
        val = find_value(request.params[0].get_obj(), "from");
        if (val.isNum()) {
            from = val.get_int();
        }
                
        val = find_value(request.params[0].get_obj(), "count");
        if (val.isNum()) {
            count = val.get_int();
        }
        
    }
   
    HD_XPub xpub(xpubkey);
    
    std::vector<std::string> v = xpub.Derive(from, count, for_change, segwit);
    
	UniValue addrs(UniValue::VARR);
	
    for(auto addr : v)
    {
        addrs.push_back(addr);
    }
    
	return addrs;
}

UniValue stibgetxpubutxos(const JSONRPCRequest& request)
{
   if (request.fHelp || request.params.size() < 1  || request.params.size() > 1)
        throw std::runtime_error(
            "stibgetxpubutxos\n"
            "\nReturns 'count' HD generated address for an 'xpub', starting  from 'start' index.\n"
            "\nArguments:\n"
            "{\n"
            "  \"xpubkey\",  account extended public key ExtPubKey\n"
            "}\n"
            "\nResult\n"
            "[\n"
            "  {\n"
            "  \"addresses\"\n"
            "    [\n"
            "      \"address\"  (string) The base58check encoded address\n"
            "      ,...\n"
            "    ]\n"
            "  }\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("stibgetxpubutxos", "'{\"xpubkey\": \"xpub6Bgu572Y3EWgEq8gkVxmznPkb8hWkgYR9E6KTZN3pyM3hhC7WvwgHNchSCrC19a7nZ3ddyjwB26rbePuyATc55snUwWKkszRnvVwfmBshdS\"}'")
            + HelpExampleRpc("stibgetxpubutxos", "{\"xpubkey\": \"xpub6Bgu572Y3EWgEq8gkVxmznPkb8hWkgYR9E6KTZN3pyM3hhC7WvwgHNchSCrC19a7nZ3ddyjwB26rbePuyATc55snUwWKkszRnvVwfmBshdS\"}")
            );
   
	std::string xpubkey;

    if (request.params[0].isObject()) {
        UniValue val = find_value(request.params[0].get_obj(), "xpubkey");
        if (val.isStr()) {
            xpubkey = val.get_str();
        }
    }
    
    UniValue utxos(UniValue::VARR);
	RecoverFromXPUB(xpubkey, utxos);
    return utxos;

}

UniValue stibgetlastusedhdindex(const JSONRPCRequest& request)
{
   if (request.fHelp || request.params.size() < 1  || request.params.size() > 1)
        throw std::runtime_error(
            "stibgetlastusedhdindex\n"
            "\nReturns the last used index, the index of the last used address.\n"
            "\nReturns -1 if no address is used.\n"
            "\nArguments:\n"
            "{\n"
            "  \"xpubkey\",  account extended public key ExtPubKey\n"
            "}\n"
            "\nResult\n"
            "[\n"
            "  {lastindex:val}\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("stibgetlastusedhdindex", "'{\"xpubkey\": \"xpub6Bgu572Y3EWgEq8gkVxmznPkb8hWkgYR9E6KTZN3pyM3hhC7WvwgHNchSCrC19a7nZ3ddyjwB26rbePuyATc55snUwWKkszRnvVwfmBshdS\"}'")
            + HelpExampleRpc("stibgetlastusedhdindex", "{\"xpubkey\": \"xpub6Bgu572Y3EWgEq8gkVxmznPkb8hWkgYR9E6KTZN3pyM3hhC7WvwgHNchSCrC19a7nZ3ddyjwB26rbePuyATc55snUwWKkszRnvVwfmBshdS\"}")
            );
   
	std::string xpubkey;

    if (request.params[0].isObject()) {
        UniValue val = find_value(request.params[0].get_obj(), "xpubkey");
        if (val.isStr()) {
            xpubkey = val.get_str();
        }
    }

    HD_XPub x(xpubkey);
    int r = GetLastUsedExternalSegWitIndex(x);
    
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("lastindex", r);
    
    return obj;
}

