#include <net.h>
#include <chainparams.h>
#include <validation.h>
#include <rpc/server.h>
#include <index/txindex.h>

#include <core_io.h>

void GenerateFromXPUB(std::string xpubkey, int from, int count, std::vector<std::string>& out);
void RecoverFromXPUB(std::string xpubkey, std::vector<std::string>& out); // defined in src/stib/cmmon.cpp
void RecoverTxsFromXPUB(std::string xpubkey, std::vector<std::string>& out); // defined in src/stib/cmmon.cpp

static std::string Join(std::vector<std::string>& v, std::string sep = ",")
{
    if(v.size() == 0) return "";
    std::string ret = v[0];

    for(unsigned int i = 1; i < v.size(); i++)
        ret += sep + v[i];

    return ret;
}

std::string ProcessStib(CDataStream& vRecv)
{
    unsigned char cmd;
    if(vRecv.size() == 0)
    {
        LogPrint(BCLog::NET, "Stib Custom message Error.\n");
        return tinyformat::format(R"({"result":{"error":"Empty payload not autorized"}})");
    }
    vRecv.read((char*)&cmd, 1);

    switch(cmd)
    {
        case 'G' :
            {
                uint32_t from, count;
                
                if(vRecv.size() != 120)
                {
                    LogPrint(BCLog::NET, "Stib Custom message : G, parameters errors.\n");
                    return tinyformat::format(R"({"result":{"error":"G command size is 120 byte, not %d"}})", vRecv.size() );
                }
                
                vRecv.read((char*)&from, 4);
                vRecv.read((char*)&count, 4);

                std::string req = vRecv.str();

                std::vector<std::string> out;
                GenerateFromXPUB(req, (int)from, (int)count, out);

                LogPrint(BCLog::NET, "Stib Custom message : Gen from = %d, count = %d, k = %s\n", from, count, req.c_str());

                return "{\"result\":[\"" + Join(out, "\",\"") + "\"]}";
                break;
            }

        case 'R' :
            {
                std::string req = vRecv.str();
                
                if(vRecv.size() != 111)
                {
                    LogPrint(BCLog::NET, "Stib Custom message : R, parameters errors.\n");
                    return tinyformat::format(R"({"result":{"error":"R command size is 111 byte, not %d"}})", vRecv.size() );
                }
                
                std::vector<std::string> out;
                RecoverFromXPUB(req, out);

                LogPrint(BCLog::NET, "Stib Custom message : Recover Utxos k = %s\n",  req.c_str());

                return "{\"result\":[" + Join(out) + "]}";
                break;
            }

        case 'T' :
            {
                std::string req = vRecv.str();
                std::vector<std::string> out;
                LogPrint(BCLog::ALL, "Stib Custom message : Recover Txs k = %s\n",  req.c_str());
                LogPrint(BCLog::ALL, "Calling : RecoverTxsFromXPUB ..\n");
                RecoverTxsFromXPUB(req, out);
                LogPrint(BCLog::ALL, "Calling : RecoverTxsFromXPUB .DONE\n");

                for(auto t: out)
                {
                    uint256 h;
                    h.SetHex(t);
                    CTransactionRef tx;
                    uint256 hash_block;
                    if (g_txindex && g_txindex->FindTx(h, hash_block, tx ))
                    {
                        out.push_back("{\"hex\":" + EncodeHexTx(*tx, 0) + "}");
                    }

                }

                LogPrint(BCLog::NET, "Stib Custom message : Recover Txs k = %s\n",  req.c_str());

                LogPrint(BCLog::ALL, "returning result\n");

                return "{\"result\":[" + Join(out) + "]}";
                break;
            }

            default:
                break;
    }
    LogPrint(BCLog::NET, "Stib Custom message, command id (%d) not found.\n", cmd);
    std::string ret = tinyformat::format(R"({"result":{"error":"stib custom command, command id (%d) not found"}})", cmd);
    return ret;
}