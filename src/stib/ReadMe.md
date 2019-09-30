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
	position	:	parameter	:	size	:
	0			:	function	:	1		: equal to 'G' (0x47)
	1			:	from		:	4		: uint32 define the index from witch the generation must starts.
	5			:	count		:	4		: uint define the number of address to generate
	9			:	xpub		:	111		: the account xpubkey
-------------------------------------------------------------
 Total								120
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
	position	:	parameter	:	size	:
	0			:	function	:	1		: equal to 'R'(0x52)
	1			:	xpub		:	111		: the account xpubkey
-------------------------------------------------------------
 Total								112
-------------------------------------------------------------

std::vector<unsigned char> Gpayload(std::string xpub) {
	std::vector<unsigned char> v(112);
	v[0] = 'R';
	memcpy((unsigned char*)(v.data() + 1), xpub.data(), 111);
	
	return v;
}

##Response:

  - first character (x):
     - if it is lesser than 0xfd
           it is the size of the rest of the message, that is a json object. and that is start at index 1.
	 - if it is equal to 0xfd
			the size of the json message is defined in the next tow byte as unsigned short
			in this case the json message start at the index 3.
  - the json object starts with '{'

# response example for a 'G' function:
f{"result":["tb1qyplyx6wfnrtqt7jjxscr9cqme0cf8kh645j6u4","tb1qgtsqak0d3s29d7t433899kc0399z9r4h4sru9a"]}

 'f' is 0x66 = 102 = the size starting from the '{'


# response example for a 'R' function:
ù{"result":[{"address":"tb1qkg547xcwcl487m3547uq40st4jtyz38twd6sq2","txid":"5271e8feb842a45fb5ab3c2e5602f2c6eab96fb15b045d72c918c310f00587f4","outputIndex":1,"script":"0014b2295f1b0ec7ea7f6e34afb80abe0bac964144eb","satoshis":10184,"height":1571089}]}

 'ù' is 0xf9 = 249 = the size starting from the '{'


# response example for an unknown function ('K' as example):
E{"result":{"error":"stib custom command, command id (75) not found"}}

 'E' is 0x45 = 69 = the size starting from the '{'

