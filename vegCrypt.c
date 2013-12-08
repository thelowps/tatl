// The functions for the header file vegCrypt.h which encrypt and decrypt messages
// Ryan Boccabella 11/10/13

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <string.h>
#include "gcrypt.h"


#define GCRY_CIPHER GCRY_CIPHER_AES128        // chooses the cipher
#define GCRY_MODE GCRY_CIPHER_MODE_CFB        // required to pick one as an argument but the mode won't matter because I process the blocks one at a time

#define GCRY_MD GCRY_MD_SHA256			 // chooses the hash function

void aesKeyGen(unsigned char *key) {  //modifies the contents of the pointer passed to it to have a key
  int	keyLength = gcry_cipher_get_algo_keylen(GCRY_CIPHER);
  gcry_randomize(key, keyLength, GCRY_STRONG_RANDOM); //stores a key in the array key,
}

void macKeyGen(unsigned char *key) {
  int keyLength = 32; //That's what it is for SHA256
  //if you change keyLength, also change it in computeMAC()
  gcry_randomize(key, keyLength, GCRY_STRONG_RANDOM);
}

void charncpy(char *cpyTo, char *cpyFrom, int x){
  int iC;
  for(iC = 0; iC < x; iC++){
    cpyTo[iC] = cpyFrom[iC];
  }
}

void aesEnc(char *aesKey, const char *ptext, long *blocks, char *ctext){
	gcry_cipher_hd_t	cipherHandle;
	gcry_error_t		gcryError;
	
	int keyLength = gcry_cipher_get_algo_keylen(GCRY_CIPHER);
	int blkLength = gcry_cipher_get_algo_blklen(GCRY_CIPHER);
	int ptextLength = strlen(ptext) + 1; //gets the number of characters including null terminator in the plaintext
	int paddedLength;
	if( (ptextLength % blkLength) == 0){
		paddedLength = ptextLength;
	}
	else{
		paddedLength = ptextLength - (ptextLength % blkLength) + blkLength;
	}
	
	char *padded_ptext = malloc(paddedLength);
	memset(padded_ptext, 0, paddedLength);    //initialize to null terminators
	memset(ctext, 0, paddedLength + blkLength);  //initialize ctext to null terminators
	
	memcpy(padded_ptext, ptext, strlen(ptext)); //copy the ptext into the padded ptext, the rest of the padding is null terminators
	
	char	* plainTextBlock = malloc(blkLength);  // allocate space for the block of the plaintext being encrypted
	char	* encBufferBlock = malloc(blkLength);  // allocate space for the new block of ciphertext
	char * encCtrBlock = malloc(blkLength);     // allocate space for the encrypted ctr string

	long 	iC;		// a counter for the for loops, long for allowing a very large number of blocks to be processed in "manual" ctr mode using AES
	
	long 	numBlocks = paddedLength / blkLength;		// the number of blocks for the padded plaintext, important for the for loop	
	*blocks = numBlocks; //let createMessage know how many blocks were encrypted
	char *ctr = malloc(blkLength); //random string 16 bytes for the ctr
	gcry_randomize(ctr, blkLength, GCRY_STRONG_RANDOM); 

	memcpy(ctext, ctr, blkLength);  // set the first block of encBuffer to ctr
	
	gcryError = gcry_cipher_open(&cipherHandle, GCRY_CIPHER, GCRY_MODE, 0);  //mode/type is irrelevant because this is encrypting an individual block
	if (gcryError){
		printf("Failure in gcry_cipher_open.\n");
		return;
	}

	gcryError = gcry_cipher_setkey(cipherHandle, aesKey, keyLength);  //this matters, it needs the key
	if (gcryError){
		printf("Failure in gcry_cipher_setkey.\n");
		return;
	}

	gcryError = gcry_cipher_setiv(cipherHandle, ctr, blkLength); //this doesn't, i'm implementing ctr mode on my own
	if(gcryError){
		printf("Failure in gcry_cipher_setiv.\n");
		return;
	}
	
	int jC, ctrIndex;
	for(iC = 1; iC <= numBlocks; iC++) {  //creates a single block (iC is the block number for that iteration) of the plaintext
		gcryError = gcry_cipher_encrypt(cipherHandle, encCtrBlock, blkLength, ctr, blkLength);
		if(gcryError){
			printf("Failure in gcry_cipher_encrpyt.\n");
			return;
		}

		for(jC = 0; jC < blkLength; jC++) {//0 to 15 for AES128
			//this character of this block of plaintext is the jCth character of plainTextBlock
			plainTextBlock[jC] = padded_ptext[blkLength * (iC - 1) + jC];  
			//XOR the jCth character of the plainTextBlock with the jCth character of the encCtrBlock
			encBufferBlock[jC] = (encCtrBlock[jC] ^ plainTextBlock[jC]);			
			ctext[ (blkLength * iC) + jC ] = encBufferBlock[jC];  //put that result in the appropriate spot in the ciphertext
		}
		
		// increment ctr for the next iteration
		for(ctrIndex = 15; ctrIndex >= 0; ctrIndex--){
			if (ctr[ctrIndex] == 127)   // if the char examined (starting with the last char in ctr) is at 127, set it to zero and continue on to increment the next char (essentially carrying)
			  {	
			    //gcry_cipher_hd_t  cipherHandle;
			    //gcry_error_t gcryError;
			    ctr[ctrIndex] = 0;
				
			}
			else	// otherwise, increment this character and move on
			{
				ctr[ctrIndex] += 1;
				ctrIndex = -1; // exit the loop
			}
		}
	}
	free(plainTextBlock);
	free(encBufferBlock);
	free(encCtrBlock);
	free(ctr);
	free(padded_ptext);
	gcry_cipher_close(cipherHandle);
}
	
	


// need to decrypt the message
void aesDec(unsigned char *aesKey, char *ctext, long numBlocks, char *ptext){ //given the ciphertext with the authentication block already gone
  gcry_cipher_hd_t cipherHandle;
  gcry_error_t gcryError;
	
  int keyLength = gcry_cipher_get_algo_keylen(GCRY_CIPHER);
  int blkLength = gcry_cipher_get_algo_blklen(GCRY_CIPHER);
  char* encCtrBlock = malloc(blkLength);
  char* encBufferBlock = malloc(blkLength);
  char* outBufferBlock = malloc(blkLength);  // allocate space for the new block of plaintext

  int iC; // a loop counter
  gcryError = gcry_cipher_open(&cipherHandle, GCRY_CIPHER, GCRY_MODE, 0);  // again, mode/type is irrelevant because this is encrypting an individual block
  if (gcryError){
    printf("Failure in gcry_cipher_open.\n");
    return;
  }

  gcryError = gcry_cipher_setkey(cipherHandle, aesKey, keyLength);
  if (gcryError){
    printf("Failure in gcry_cipher_setkey.\n");
    return;
  }
  // get the ctr from the ciphertext
  char *ctr = malloc(blkLength);
  memcpy(ctr, ctext, blkLength);
  gcryError = gcry_cipher_setiv(cipherHandle, ctr, blkLength);  // do this manually so the definition shouldn't matter
  if(gcryError){
    printf("Failure in gcry_cipher_setiv.\n");
    return;
  }
	
  //actually decrypt now
  for(iC = 1; iC <= (numBlocks - 1); iC++) { // runs through all ciphertext blocks (indexing starting at 0, skip ctr block)
    gcryError = gcry_cipher_encrypt(cipherHandle, encCtrBlock, blkLength, ctr, blkLength); //AES the ctr, put this in encCtrBlock
    if(gcryError){ //make sure encryption was successful
      printf("Failure in gcry_cipher_encrypt.\n");
      return;
    }
		
    int jC;  //loop coutner
    for(jC = 0; jC < blkLength; jC++) { // runs through each byte of the ciphertext block
      encBufferBlock[jC] = ctext[blkLength * (iC) + jC];  // finds the part of the block of ciphertext we're looking for
      //printf("Index of ctext: %d\n", (blkLength * iC) + jC);
      outBufferBlock[jC] = (encCtrBlock[jC] ^ encBufferBlock[jC]);  //XOR the block we have with
      ptext[(iC - 1) * blkLength + jC] = outBufferBlock[jC];   // iC - 1 to remove ctr from the front of the message
      //printf("Index of ptext: %d\n", (iC - 1) * blkLength + jC);
    }
		
    int ctrIndex;
    for(ctrIndex = 15; ctrIndex >= 0; ctrIndex--){
      if (ctr[ctrIndex] == 127)   // if the last char is at 127, set it to zero and continue on to increment the next char
	{
	  ctr[ctrIndex] = 0;
				
	}
      else	// otherwise, increment this character and move on
	{
	  ctr[ctrIndex] += 1;
	  ctrIndex = -1;
	}
    }
  }

  free(encCtrBlock);
  free(encBufferBlock);
  free(outBufferBlock);
  free(ctr);
  gcry_cipher_close(cipherHandle);
}


//Hashing function
void computeMAC(char *input, char *macKey, long numBlocks, char *MAC){
	gcry_md_hd_t		hashHandle;
	gcry_error_t		gcryError;
	int keyLength = 32;  //that's what it is for SHA256 (if you change this, also change in keyGen)
	gcryError = gcry_md_open(&hashHandle, GCRY_MD, GCRY_MD_FLAG_HMAC);  //last argument makes it do HMAC
	if (gcryError){
		printf("Failure in gcry_cipher_open.\n");
		return;
	}

	gcryError = gcry_md_setkey(hashHandle, macKey, keyLength);  //set the key for the hash function here
	if (gcryError){
		printf("Failure in gcry_md_setkey.\n");
		return;
	}
	
	char *tempDig;
	gcry_md_write(hashHandle, input, (numBlocks * 16));
	tempDig = (char*)gcry_md_read(hashHandle, GCRY_MD);
	charncpy((char*)MAC, (char*)tempDig, 32);
	gcry_md_close(hashHandle); //close the context	

	/*
	printf("MAC: ");
	int i;
	for(i = 0; i < 32; ++i) {
	  printf("%02x ", (unsigned char)MAC[i]);
	}
	printf("\n");
	*/
}//end computeMAC	



long power(long x, int y){ //returns x^y
	int iC; //loop counter
	long accum = 1;
	for(iC = 1; iC <= y; iC++){
		accum *= x; //multiply the accumulator by x
	}
	return accum;
}//end power



void setBlockStr(char *str, long blocks){
	int iC;
	long base = 10;
	int quotient;
	int ASCIIoffset = 48;
	for(iC = 15; iC >= 0; iC--){
		quotient = blocks / power(base, iC);
		str[15 - iC] = quotient + ASCIIoffset;
		blocks = blocks % power(base, iC); //remove the leading digit
	}
}//end setBlockStr

long getBlockStr(char *str){
	int iC;
	long base = 10;
	int ASCIIoffset = 48;
	long number = 0;
	for(iC = 15; iC >= 0; iC--){
		number += (str[15 - iC] - ASCIIoffset) * power(base, iC);				
	}
	return number;
}//end getBlockStr

//creates message given the two keys and the text
long createMessage(char *aesKey, char *macKey, char *cipherText, char *text){
	int blkLength = 16; //this is for AES128
	long numBlocks = 0;
	long *numPtr = &numBlocks;
	
	char *cText = malloc(strlen(text) + 1 + (16 * 7));
	aesEnc(aesKey, text, numPtr, cText);
	charncpy(&cipherText[blkLength], cText, (numBlocks + 1) * blkLength); //put the encrypted stuff starting in the second block
	numBlocks += 4;  //numBlocks returned is length of padded plaintext, need another for the ctr and another for the length and two for the mac 
	char * blockStr = malloc(blkLength);
	setBlockStr(blockStr, numBlocks);
	charncpy(&cipherText[0], blockStr, blkLength);
	
	char * sendMAC = malloc(32);  //32 bytes for the MAC we're sending
	computeMAC(cipherText, macKey, (numBlocks - 2), sendMAC); // calculates the MAC
	charncpy(&cipherText[(numBlocks - 2) * blkLength], sendMAC, blkLength * 2);
	
	free(cText);
	free(blockStr);
	free(sendMAC);
	return numBlocks * 16; //the number of bytes, because each block is 16 bytes
}//end createMessage


//verify that the mac on the message is the mac from the first bit of text
int vrfyMAC(void *input, void *macKey, int blocks, char *sentMAC){
  input = (char*)input;
  macKey = (char*)macKey;
  int vrfy, digLength = 32;  //32 bytes of input 
  char *computedMAC = malloc(digLength); 
  computeMAC(input, macKey, blocks, computedMAC);
  if (strncmp(computedMAC, sentMAC, digLength) == 0) {  //if the first digLenth bytes match (no null term, so strncmp not strcmp)
    vrfy = 1;
  } else {
    vrfy = 0;
  }

  free(computedMAC);
  return vrfy;
}//end vrfyMAC 

			
int deconstructMessage(unsigned char *aesKey, unsigned char *macKey, char **plainTextPtr, unsigned char *input){
  int blkLength = 16; //for AES128
  long numBlocks;
  char *blockString = malloc(blkLength);
  charncpy((char*)blockString, (char*)input, blkLength);
  numBlocks = getBlockStr(blockString);
  //printf("deconstructMessage: numBlocks is: %ld\n", numBlocks);
  int success; //tells whether or not the vrfy function was successful
	
  char *MACdStuff = malloc((numBlocks - 2) * blkLength);  //we mac everything but the mac on the end
  charncpy((char*)MACdStuff, (char*)input, (numBlocks - 2) * blkLength);
  char *ctext = malloc((numBlocks - 3) * blkLength);
  //printf("ctext size is: %d\n", (numBlocks - 3) * blkLength);
  charncpy((char*)ctext, (char*)(input+blkLength), (numBlocks - 3) * blkLength);

  char *receivedMAC = malloc(2*blkLength);
  charncpy((char*)receivedMAC, (char*)(input + (numBlocks - 2)*blkLength), 2*blkLength);
  success = vrfyMAC(MACdStuff, macKey, (numBlocks - 2), receivedMAC);
  //printf("numBlocks - 3 is: %d\n", numBlocks - 3);
  char *pText = malloc((numBlocks-3)*blkLength);
  aesDec(aesKey, ctext, (numBlocks - 3), pText);
  //printf("Plain text in deconstructMessage is: %s\n", pText);
  *plainTextPtr = pText; //set the location
	
  //don't free pText
  free(MACdStuff);
  free(ctext);
  free(receivedMAC);
  free(blockString);
  return success;
}

void hash_pass(char *password, char *digest){
  //pad the password to the next full block, deterministic padding, so
  //will modify the password in predictable way
  int length = strlen(password);
  int blkLen = 16;
  int inputLen;
  inputLen = blkLen * ceil((double)length / blkLen);  //space of input
  //is the next highest multiple of blkLen if not currently a multiple of
  //blkLen
  
  char input[inputLen];
  memset(input, 0, inputLen);
  memcpy(input, password, length);  //copy the password into part of the input
  //printf("input to hash is: %16s \n\n", input);
  long numBlocks = inputLen / blkLen;
  
  computeMAC(input, "000000000000000000000000000042", numBlocks, digest);
}//end hash_pass
