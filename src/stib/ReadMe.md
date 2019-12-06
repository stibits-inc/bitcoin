###stib command description :

##command name : "stib"

##description
 this command hold all custom messages developped for stibits.
 for now : 
    - hd addresses generation from an extended public key
    - get utxos from an extended public key
 this list is open and it can grow.


##payload

  - the first byte of the payload define the function.

  the functions list for now contain:
    - 'G' (0x47) : generate address
    - 'R' (0x52) : Recover wallete

## G function detail:
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


std::vector<unsigned char> Gpayload(uint32_t from, uint32_t size, std::string xpub) {
    std::vector<unsigned char> v(120);
    v[0] = 'G';
    *((uint32_t*)(v.data() + 1)) = from;
    *((uint32_t*)(v.data() + 5)) = size;
    memcpy((unsigned char*)(v.data() + 9), xpub.data(), 111);
    
    return v;
}

## R function detail:
_____________________________________________________________
    position    :   parameter   :   size    :
-------------------------------------------------------------
    0           :   function    :   1       : equal to 'R'(0x52)
    1           :   xpub        :   111     : the account xpubkey
-------------------------------------------------------------
 Total                              112
-------------------------------------------------------------

std::vector<unsigned char> Rpayload(std::string xpub) {
    std::vector<unsigned char> v(112);
    v[0] = 'R';
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

    position    :   type        :   size    :
-------------------------------------------------------------
    0           :   compactInt  :   1..9    : the size of the rest of the reponse (byte count)
    1..9        :   compactInt  :   1..9    : the number of utxos in this response
    2..18       :   Utxos[]     :           : utxos array 
-------------------------------------------------------------- 

struct R_Response
{
   compactInt size;                // 	the size of the rest of the reponse (byte count)
   compactInt utxos_count;         //   the number of utxos in this response
   Utxo    utxos[utxos_count];  //   utxos array 
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
   compactInt size;               // 	the size of the rest of the reponse (byte count)
   compactInt txs_count;          //   the number of txs in this response
   Tx    txs[txs_count];          //   Txs array 
};

Tx structure is defined here :
  https://en.bitcoin.it/wiki/Protocol_documentation#tx


