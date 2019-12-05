#include <net.h>
#include <chainparams.h>
#include <validation.h>
#include <rpc/server.h>
#include <index/txindex.h>

#include <core_io.h>

void GenerateFromXPUB(std::string xpubkey, int from, int count, std::vector<std::string>& out);  // defined in src/stib/common.cpp
void GenerateFromXPUB(std::string xpubkey, int from, int count, CDataStream& ss);  // defined in src/stib/common.cpp

void RecoverFromXPUB(std::string xpubkey, std::vector<std::string>& out); // defined in src/stib/common.cpp
uint32_t RecoverFromXPUB(std::string xpubkey, CDataStream& out); // defined in src/stib/common.cpp

void RecoverTxsFromXPUB(std::string xpubkey, std::vector<uint256>& out);  // defined in src/stib/common.cpp

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
    
    BCLog::LogFlags logFlag = BCLog::NET;
   
    if(vRecv.size() == 0)
    {
        LogPrintf("Stib Custom message Error.\n");
        return tinyformat::format(R"({"result":{"error":"Empty payload not autorized"}})");
    }
    vRecv.read((char*)&cmd, 1);
    

    switch(cmd)
    {
        case 'G' :
            {
                uint32_t from, count;
                
                if(vRecv.size() != 119)
                {
                    LogPrintf( "Stib Custom message : G, parameters errors.\n");
                    return tinyformat::format(R"({"result":{"error":"G command size is 120 byte, not %d"}})", vRecv.size() );
                }
                
                vRecv.read((char*)&from, 4);
                vRecv.read((char*)&count, 4);

                std::string req = vRecv.str();
                LogPrint(logFlag, "Stib Custom message : Gen from = %d, count = %d, k = %s\n", from, count, req.c_str());

                CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                GenerateFromXPUB(req, (int)from, (int)count, ss);

                return ss.str();
                break;
            }

        case 'R' :
            {
                std::string req = vRecv.str();
                LogPrint(logFlag, "Stib Custom message : Recover Utxos k = %s\n",  req.c_str());
                
                if(vRecv.size() != 111)
                {
                    LogPrintf( "Stib Custom message : R, parameters errors.\n");
                    return tinyformat::format(R"({"result":{"error":"R command size is 111 byte, not %d"}})", vRecv.size() );
                }
                
                CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
                uint32_t count = RecoverFromXPUB(req, ss);
                
                CDataStream tmp(SER_NETWORK, PROTOCOL_VERSION);
                WriteCompactSize(tmp, count);
                ss.insert(ss.begin(), (const char*)tmp.data(), (const char*)tmp.data() + tmp.size());
                
                return ss.str();
                break;
            }

        case 'T' :
            {
                if(!g_txindex)
                {
                    LogPrintf("Stib Custom message : T, Error, bitcoind is not started with -txindex option.\n");
                    return tinyformat::format(R"({"result":{"error":"bitcoind is not started with -txindex option"}})");
                }
                
                std::string req = vRecv.str();
                std::vector<uint256> out;
                std::vector<std::string> outHex;
                RecoverTxsFromXPUB(req, out);
                
                CDataStream ssTx(SER_NETWORK, PROTOCOL_VERSION);
                WriteCompactSize(ssTx, out.size());
      
                if(out.size())
                    LogPrint(logFlag, "Stib Custom message : T, %d, Transactions found.\n", out.size());

                for(auto txhash: out)
                {
                    CTransactionRef tx;
                    uint256 hash_block;
                    if (g_txindex->FindTx(txhash, hash_block, tx ))
                    {
                        ssTx << *tx;
                    }
                }

                LogPrint(logFlag, "Stib Custom message : Recover Txs k = %s\n",  req.c_str());

                return ssTx.str();
                return "{\"result\":[" + Join(outHex) + "]}";
                break;
            }

            default:
                break;
    }
    
    LogPrint(logFlag, "Stib Custom message, command id (%d) not found.\n", cmd);
    std::string ret = tinyformat::format(R"({"result":{"error":"stib custom command, command id (%d) not found"}})", cmd);
    return ret;
}