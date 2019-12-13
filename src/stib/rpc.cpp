#include <rpc/blockchain.h>
#include <rpc/server.h>
#include <rpc/util.h>

void RecoverFromXPUB(std::string xpubkey, UniValue& out);
void GenerateFromXPUB(std::string xpubkey, int from, int count, UniValue& out);
int GetLastUsedExternalSegWitIndex(std::string& xpubkey);

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
    int  from = 0;
    int  count = 100;

    if (request.params[0].isObject()) {
        UniValue val = find_value(request.params[0].get_obj(), "xpubkey");
        if (val.isStr()) {
            xpubkey = val.get_str();
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

    UniValue addrs(UniValue::VARR);
    GenerateFromXPUB(xpubkey, from, count, addrs);

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

    int r = GetLastUsedExternalSegWitIndex(xpubkey);

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("lastindex", r);

    return obj;
}

