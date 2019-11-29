#include <net.h>
#include <string>
#include <vector>

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
    vRecv.read((char*)&cmd, 1);

    switch(cmd)
    {
        case 'G' :
            {
                uint32_t from, count;
                vRecv.read((char*)&from, 4);
                vRecv.read((char*)&count, 4);

                std::string req = vRecv.str();

                std::vector<std::string> out;
                GenerateFromXPUB(req, (int)from, (int)count, out);

                LogPrint(BCLog::NET, "Stib Custom message : Gen from = %d, count = %d, k = %s \n", from, count, req.c_str());

                return "{\"result\":[\"" + Join(out, "\",\"") + "\"]}";
                break;
            }

        case 'R' :
            {
                std::string req = vRecv.str();
                std::vector<std::string> out;
                RecoverFromXPUB(req, out);

                LogPrint(BCLog::NET, "Stib Custom message : Recover Utxos k = %s \n",  req.c_str());

                return "{\"result\":[" + Join(out) + "]}";
                break;
            }

        case 'T' :
            {
                std::string req = vRecv.str();
                std::vector<std::string> out;
                RecoverTxsFromXPUB(req, out);

                LogPrint(BCLog::NET, "Stib Custom message : Recover Txs k = %s \n",  req.c_str());

                return "{\"result\":[" + Join(out) + "]}";
                break;
            }

            default:
                break;
    }

    std::string ret = tinyformat::format(R"({"result":{"error":"stib custom command, command id (%d) not found"}})", cmd);
    return ret;
}