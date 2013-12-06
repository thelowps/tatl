//takes in a pointer for where to put a key (16 bytes)
void aesKeyGen(unsigned char *key);
//modifies the memory it points to to contain the key

//takes in the key, the plaintext, and the number of blocks
void aesEnc(char *aesKey, const char *ptext, long *blocks, char *ctext);
//returns a pointer to the ciphertext (and we know how many blocks it has)

//takes in a key and the already modified message from the network, and the number of blocks of the already modified message
void aesDec(char *aesKey, char *ctext, long numBlocks, char *ptext);
// returns a pointer to the plaintext which will have a null term in it

//takes in a pointer for where to put the key (32 bytes)
void macKeyGen(unsigned char *key);
// modifies the memory it points to to contain the key

//takes in a buffer, a mac key, and the number of blocks that the input has (a block is two bytes)
void computeMAC(char * input, char * macKey, long numBlocks, char *MAC);
//returns a pointer to the result of MACing it

//Takes in the two keys, a space large enough to put the ciphertext in, and the text the user entered
long createMessage(unsigned char *aesKey, unsigned char *macKey, unsigned char *cipherText, char *text);
//Returns the number of bytes that the message is long

//Takes in the two keys, a space large enough to put the recovered plain text is, and the message from the network
int deconstructMessage(unsigned char *aesKey, unsigned char *macKey, char **plainTextPtr, unsigned char *input);
//returns whether or not vrfy worked on the MAC, if it did and the keys match, encryption is successful

//takes in the number of blocks and a str 16 bytes (a block) long
void setBlockStr(char *str, long blocks);
//changes str to have a string with the number of blocks (i.e. 0000000000000015)

//takes in a string of character integers that is 16 bytes (a block) long
long getBlockStr(char *str);
//returns as a long the number it had in it

//takes in a base x and an exponent y
long power(long x, int y);
//returns x^y

//given a pointer to start copying from and space to copy to
void charncpy(char *cpyTo, char *cpyFrom, int x);
//modifies cpyTo by copying the first x chars from cpyFrom to it 

//given an input, the mac key, the number of blocks of the input, and the mac it was sent
int vrfyMAC(char *input, char *macKey, int blocks, char *sentMAC);
// returns 1 if the mac of the input using the macKey matches the sentMAC, 0 if not

//hash a given text
void hash_pass(char *password, unsigned char *digest);
