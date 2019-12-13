###stib command description :

##command name : "stib"

##description
 this command hold all custom messages developped for stibits.
 for now : 
    - hd addresses generation from an extended public key
    - get utxos from an extended public key
    - get transactions from an extended public key
 this list is open and it can grow.


##payload

  - the first byte of the payload define the function.

  the functions list for now contain:
    - 'G' (0x47) : generate address
    - 'R' (0x52) : Recover wallete : get UTXOs
    - 'T' (0x54) : Recover wallete : get Txs

## G function detail:

Request structure:
_____________________________________________________________
    position    :   parameter   :   size    :
-------------------------------------------------------------
    0           :   function    :   1       : equal to 'G' (0x47)
    1           :   from        :   4       : uint32 define the index from witch the generation must starts.
    5           :   count       :   4       : uint define the number of address to generate
    9           :   xpub        :   111     : the account xpubkey
-------------------------------------------------------------
 Total                              120
-------------------------------------------------------------

example :
std::vector<unsigned char> Gpayload(uint32_t from, uint32_t size, std::string xpub) {
    std::vector<unsigned char> v(120);
    v[0] = 'G';
    *((uint32_t*)(v.data() + 1)) = from;
    *((uint32_t*)(v.data() + 5)) = size;
    memcpy((unsigned char*)(v.data() + 9), xpub.data(), 111);
    
    return v;
}

## R function detail:

Request structure:
_____________________________________________________________
    position    :   parameter   :   size    :
-------------------------------------------------------------
    0           :   function    :   1       : equal to 'R'(0x52)
    1           :   xpub        :   111     : the account xpubkey
-------------------------------------------------------------
 Total                              112
-------------------------------------------------------------

example:
std::vector<unsigned char> Rpayload(std::string xpub) {
    std::vector<unsigned char> v(112);
    v[0] = 'R';
    memcpy((unsigned char*)(v.data() + 1), xpub.data(), 111);
    
    return v;
}

## T function detail:

Request structure:
_____________________________________________________________
    position    :   parameter   :   size    :
-------------------------------------------------------------
    0           :   function    :   1       : equal to 'R'(0x52)
    1           :   xpub        :   111     : the account xpubkey
-------------------------------------------------------------
 Total                              112
-------------------------------------------------------------

example:
std::vector<unsigned char> Tpayload(std::string xpub) {
    std::vector<unsigned char> v(112);
    v[0] = 'T';
    memcpy((unsigned char*)(v.data() + 1), xpub.data(), 111);
    
    return v;
}

##Responses:

#Response for G message:

struct G_Response
{
   compactInt size;                // 	the size of the rest of the reponse (byte count)
   compactInt addrs_count;         //   the number of addresses in this response
   address    addrs[addrs_count];  //   addresses array 
};
each address has the structure:
struct address:
{
   compactInt length;
   char       address[length];
}

#Response for R message:

struct R_Response
{
   compactInt size;                // 	the size of the rest of the reponse (byte count)
   compactInt utxos_count;         //   the number of utxos in this response
   Utxo    utxos[utxos_count];     //   utxos array 
};

each Utxo has the structure:

struct Utxo
{
    compactInt addr_len;
    char addr[addr_len];  // the address

    uint256 txhash;
    int32_t index;  // output index

    int64_t satoshis;
    compactInt script_size;
    char script[script_size]; 
    int blockHeight;
};


#Response for T message:

struct T_Response
{
   compactInt size;               //   the size of the rest of the reponse (byte count)
   compactInt txs_count;          //   the number of txs in this response
   Tx    txs[txs_count];          //   Txs array 
};

Tx structure is defined here :
  https://en.bitcoin.it/wiki/Protocol_documentation#tx



Example tx Message:
--------------------------------------------------------------------------------------------------------------------------------------------------------
02000000  : version
0001      : segwit flag
02        : 2 inputs
  the first:
    1b3fcd61aaf80bde335b34e0ace6bc935058bc2cae64a15965146691cdcd1072 01000000
    17 160014b6e3d013244d114133c1555f8ecd10452c7b6411 feffffff

  the second:
    3fa98db8fc823c38289d14a5ce925be960ae1b1ce6c8f71f2ec7c9d93be3928d 01000000
    17 1600141b4b8ddd50a55d6072f4786ea44c8c06a269f388 feffffff

02        : 2 outputs
  the first:
    40420f0000000000
    16 0014c5e8742a958417ad35507c908b45f4870d3deaec

  the second:
    aede160000000000
    16 0014d89798292df6b1ae728351f46ae924565c717e98 

02        : 2 segwit for the first input
  the first:
    47 304402204d8d59d0d271ae306d2be4bfb36930b084f71cc35e5216279028a785ff37052802206d19ade2014e7f2a191487256c72889961d0a757f9d5284bfbc42c47cbb0584a01

  the second:
    21 02451352296cb8b1b1bee4141e23494aaa993901db5a3534b5b5281ff7804a472e

02        : 2 segwit for the second input
  the first:
    47 30440220438da3e5e4d39d471129cc92e384611f45a10f2b2e402894b3b2cb75a15b318602207cb66f822d650a78af6cb35998845749059684bec929ad6d3a96a420129aa0e101

  the second:
    21 03f378b0b1c55890ba2161910132383eeeb5df641b8d79e10874cd923f817d669d

e4e81700  : locktime

--------------------------------------------------------------------------------------------------------------------------------------------------------

