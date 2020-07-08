// Copyright (c) 2017-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <stib/index/addressindex.h>
#include <index/txindex.h>
#include <validation.h>
#include <key_io.h>
#include <rpc/server.h>

constexpr char DB_ADDRESSINDEX = 'a';
constexpr char DB_ADDRESSUNSPENTINDEX = 'u';

std::unique_ptr<CAddressIndex> g_addressindex;


CAddressIndex::CAddressIndex(size_t nCacheSize, bool fMemory, bool fWipe)
 : CDBWrapper( GetDataDir() / "indexes" / "addressindex", nCacheSize, fMemory, fWipe), batch(*this) {
}

bool CAddressIndex::ReadAddressUnspentIndex(uint160 addressHash, int type, std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &vect)
{

    std::unique_ptr<CDBIterator> pcursor(NewIterator());

    pcursor->Seek(std::make_pair(DB_ADDRESSUNSPENTINDEX, CAddressIndexIteratorKey(type, addressHash)));

    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char,CAddressUnspentKey> key;
        if (pcursor->GetKey(key) && key.first == DB_ADDRESSUNSPENTINDEX && key.second.hashBytes == addressHash) {
            CAddressUnspentValue nValue;
            if (pcursor->GetValue(nValue)) {
                vect.push_back(std::make_pair(key.second, nValue));
                pcursor->Next();
            } else {
                return error("failed to get address unspent value");
            }
        } else {
            break;
        }
    }

    return true;
}


bool CAddressIndex::ReadAddressIndex(uint160 addressHash, int type, std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndex, unsigned int start, unsigned int end)
{

    std::unique_ptr<CDBIterator> pcursor(NewIterator());

    pcursor->Seek(std::make_pair(DB_ADDRESSINDEX, CAddressIndexIteratorKey(type, addressHash)));
    
    while (pcursor->Valid()) {
        boost::this_thread::interruption_point();
        std::pair<char,CAddressIndexKey> key;
        if (pcursor->GetKey(key) && key.first == DB_ADDRESSINDEX && key.second.hashBytes == addressHash) {
            if (end > 0 && ((unsigned int)key.second.blockHeight) > end) {
                break;
            }
            CAmount nValue;
            if (pcursor->GetValue(nValue)) {
                addressIndex.push_back(std::make_pair(key.second, nValue));
                pcursor->Next();
            } else {
                return error("failed to get address index value");
            }
        } else {
            break;
        }
    }

    return true;
}

void CAddressIndex::Clear()
{
    batch.Clear();
}

void CAddressIndex::Begin()
{
    batch.Clear();
    //Clear();
}

bool CAddressIndex::Commit()
{
    bool r = WriteBatch(batch);
    if(r)
        batch.Clear();
    
    return r;
}

void CAddressIndex::Write(CAddressIndexKey k, CAmount v)
{
    batch.Write(std::make_pair(DB_ADDRESSINDEX, k), v);
}

void CAddressIndex::Erase(CAddressIndexKey k)
{
    batch.Erase(std::make_pair(DB_ADDRESSINDEX, k));
}

void CAddressIndex::Write(CAddressUnspentKey k, CAddressUnspentValue v)
{
    batch.Write(std::make_pair(DB_ADDRESSUNSPENTINDEX, k), v);
}

void CAddressIndex::Erase(CAddressUnspentKey k)
{
    batch.Erase(std::make_pair(DB_ADDRESSUNSPENTINDEX, k));
}

// RPC interface

bool GetAddressUnspent(uint160 addressHash, int type,
                       std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > &unspentOutputs)
{
    if (!g_addressindex)
        return error("address index not enabled");
    
    if (!g_addressindex->ReadAddressUnspentIndex(addressHash, type, unspentOutputs))
        return error("unable to get txids for address");
    
    return true;
}

bool GetAddressIndex(uint160 addressHash, int type,
                     std::vector<std::pair<CAddressIndexKey, CAmount> > &addressIndex)
{
    if (!g_addressindex)
        return error("address index not enabled");

    if (!g_addressindex->ReadAddressIndex(addressHash, type, addressIndex))
        return error("unable to get txids for address");

    return true;
}

bool HashTypeToAddress(const uint160 &hash, const int &type, std::string &address)
{
    // (whichType == TX_PUBKEYHASH || whichType == TX_SCRIPTHASH || whichType == TX_WITNESS_V0_KEYHASH)
 
    if (type == TX_SCRIPTHASH) {
        address = EncodeDestination(ScriptHash(hash));
    } else if (type == TX_PUBKEYHASH) {
        address = EncodeDestination(PKHash(hash));
    } else if (type == TX_WITNESS_V0_KEYHASH) {
        address = EncodeDestination(WitnessV0KeyHash(hash));
    } else {
        return false;
    }
    return true;
}

bool heightSort(std::pair<CAddressUnspentKey, CAddressUnspentValue> a,
                std::pair<CAddressUnspentKey, CAddressUnspentValue> b) {
    return a.second.blockHeight < b.second.blockHeight;
}

std::vector<uint256> GetAddressesTxs(std::vector<std::pair<uint160, int>> &addresses)
{

    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;

    for (auto it = addresses.begin(); it != addresses.end(); it++) {

        if (!GetAddressIndex((*it).first, (*it).second, addressIndex)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");

        }
    }

    std::set<std::pair<int, uint256> > txids;
    std::vector<uint256> result;

    for (auto it = addressIndex.begin(); it != addressIndex.end(); it++) {
        int height = it->first.blockHeight;

        txids.insert(std::make_pair(height, it->first.txhash));
    }

    for (auto it=txids.begin(); it!=txids.end(); it++) {
        result.push_back(it->second);
    }

    return result;
}


int  GetFirstBlockHeightForAddresses(std::vector<std::pair<uint160, int>> &addresses)
{
    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;
    
    for (auto& a: addresses) {
        if (!GetAddressIndex(a.first, a.second, addressIndex)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
        }
    }
    
    if (addressIndex.size() == 0 ) return -1;
    
    int blockHeight = addressIndex[0].first.blockHeight;
    
    for (auto& a: addressIndex) {
        int height = a.first.blockHeight;
		if (height < blockHeight)
            blockHeight = height;
    }

    return blockHeight;
}

bool IsAddressesHasTxs(std::vector<std::pair<uint160, int>> &addresses)
{

    std::vector<std::pair<CAddressIndexKey, CAmount> > addressIndex;

    for (auto it = addresses.begin(); it != addresses.end(); it++) {

        if (!GetAddressIndex((*it).first, (*it).second, addressIndex)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");

        }
        
        if(addressIndex.size() > 0) return true;
    }
    
    return addressIndex.size() > 0;
}

UniValue GetAddressesUtxos(std::vector<std::pair<uint160, int>> &addresses)
{
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {

            if (!GetAddressUnspent((*it).first, (*it).second, unspentOutputs)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
    }

    std::sort(unspentOutputs.begin(), unspentOutputs.end(), heightSort);

    UniValue utxos(UniValue::VARR);

    for (auto it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++) {
        UniValue output(UniValue::VOBJ);
        
        std::string address;
        
        if (!HashTypeToAddress(it->first.hashBytes, it->first.type, address)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown address type");
        }

        output.pushKV("address", address);
        output.pushKV("txid", it->first.txhash.GetHex());
        output.pushKV("outputIndex", (int)it->first.index);
        output.pushKV("script", HexStr(it->second.script.begin(), it->second.script.end()));
        output.pushKV("satoshis", it->second.satoshis);
        output.pushKV("height", it->second.blockHeight);
        utxos.push_back(output);
    }

    return utxos;
}

bool GetAddressesUtxos(std::vector<std::pair<uint160, int>> &addresses, CDataStream& ss, uint32_t& count)
{
    std::vector<std::pair<CAddressUnspentKey, CAddressUnspentValue> > unspentOutputs;

    for (std::vector<std::pair<uint160, int> >::iterator it = addresses.begin(); it != addresses.end(); it++) {

            if (!GetAddressUnspent((*it).first, (*it).second, unspentOutputs)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
            }
    }

    std::sort(unspentOutputs.begin(), unspentOutputs.end(), heightSort);

    count += unspentOutputs.size();

    for (auto it=unspentOutputs.begin(); it!=unspentOutputs.end(); it++) {

        std::string address;
        
        if (!HashTypeToAddress(it->first.hashBytes, it->first.type, address)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Unknown address type");
        }
        ss << address;
                
        it->first.txhash.Serialize(ss);
        ser_writedata32(ss, it->first.index);
        
        ss << it->second;
    }

    return unspentOutputs.size() > 0;
}

int GetLastUsedIndex(std::vector<std::pair<uint160, int>> &addresses)
{
    int r = -1;
    int index = 0;
    std::vector<std::pair<CAddressIndexKey , CAmount> > txOutputs;

    for (auto it = addresses.begin(); it != addresses.end(); it++) {
        if (!GetAddressIndex((*it).first, (*it).second, txOutputs)) {
             throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "No information available for address");
        }
        if(txOutputs.size() > 0)
        {
            r = index;
            txOutputs.clear();
        }
        index++;
    }
    
    return r;
}
    