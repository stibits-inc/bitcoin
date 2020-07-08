#include <rpc/blockchain.h>
#include <rpc/server.h>
#include <rpc/util.h>

int GetLastUsedExternalSegWitIndex(std::string& xpubkey);
int GetFirstUsedBlock(std::string xpub);

// RPC

UniValue stbtsgetlastusedhdindex(const JSONRPCRequest& request)
{
   if (request.fHelp || request.params.size() < 1  || request.params.size() > 1)
        throw std::runtime_error(
            "stbtsgetlastusedhdindex\n"
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
            + HelpExampleCli("stbtsgetlastusedhdindex", "\"xpub6Bgu572Y3EWgEq8gkVxmznPkb8hWkgYR9E6KTZN3pyM3hhC7WvwgHNchSCrC19a7nZ3ddyjwB26rbePuyATc55snUwWKkszRnvVwfmBshdS\"")
            + HelpExampleRpc("stbtsgetlastusedhdindex", "\"xpub6Bgu572Y3EWgEq8gkVxmznPkb8hWkgYR9E6KTZN3pyM3hhC7WvwgHNchSCrC19a7nZ3ddyjwB26rbePuyATc55snUwWKkszRnvVwfmBshdS\"")
            );

    std::string xpubkey;

    if (request.params[0].isObject()) {
        UniValue val = find_value(request.params[0].get_obj(), "xpubkey");
        if (val.isStr()) {
            xpubkey = val.get_str();
        }
    }
    else
    if(request.params[0].isStr())
    {
        xpubkey = request.params[0].get_str();
    }
    else
    {
        throw JSONRPCError(-1, "xpub is missing or invalid!!!");
    }

    if(xpubkey.size() == 0 || xpubkey[0] != 'x')
    {
        throw JSONRPCError(-1, "xpub is missing or invalid!!!");
    }
    
    if(xpubkey.size() == 0 || xpubkey[0] != 'x')
    {
        throw JSONRPCError(-1, "xpub is missing or invalid!!!");
    }

    int r = GetLastUsedExternalSegWitIndex(xpubkey);
    
    if(r == -1)
    {
        throw JSONRPCError(-1, "xpub is missing or invalid!!!");
    }
    
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("lastindex", r);

    return obj;
}


UniValue stbtsgetfirstusedblock(const JSONRPCRequest& request)
{
   if (request.fHelp || request.params.size() < 1  || request.params.size() > 1)
        throw std::runtime_error(
            "stbtsgetfirstusedblock\n"
            "\nReturns the last used index, the index of the last used address.\n"
            "\nReturns -1 if no address is used.\n"
            "\nArguments:\n"
            "{\n"
            "  \"xpubkey\",  account extended public key ExtPubKey\n"
            "}\n"
            "\nResult\n"
            "[\n"
            "  {firstusedblock:val}\n"
            "]\n"
            "\nExamples:\n"
            + HelpExampleCli("stbtsgetfirstusedblock", "'{\"xpubkey\": \"xpub6Bgu572Y3EWgEq8gkVxmznPkb8hWkgYR9E6KTZN3pyM3hhC7WvwgHNchSCrC19a7nZ3ddyjwB26rbePuyATc55snUwWKkszRnvVwfmBshdS\"}'")
            + HelpExampleRpc("stbtsgetfirstusedblock", "{\"xpubkey\": \"xpub6Bgu572Y3EWgEq8gkVxmznPkb8hWkgYR9E6KTZN3pyM3hhC7WvwgHNchSCrC19a7nZ3ddyjwB26rbePuyATc55snUwWKkszRnvVwfmBshdS\"}")
            );
   
    std::string xpubkey;

    if (request.params[0].isObject()) {
        UniValue val = find_value(request.params[0].get_obj(), "xpubkey");
        if (val.isStr()) {
            xpubkey = val.get_str();
        }
    }
    else
    if(request.params[0].isStr())
    {
        xpubkey = request.params[0].get_str();
    }
    else
    {
        throw JSONRPCError(-1, "xpub is missing!!");
    }

    if(xpubkey.size() == 0 || xpubkey[0] != 'x')
    {
        throw JSONRPCError(-1, "xpub is missing or invalid!!!");
    }

    int r = GetFirstUsedBlock(xpubkey);
    
    if(r == -2)
    {
        throw JSONRPCError(-1, "xpub is invalid!!!");
    }
    
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("firstusedblock", r);
    
    return obj;
}


