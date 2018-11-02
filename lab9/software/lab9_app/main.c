/************************************************************************
Lab 9 Nios Software

Dong Kai Wang, Fall 2017
Christine Chen, Fall 2013

For use with ECE 385 Experiment 9
University of Illinois ECE Department
************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "aes.h"

// Pointer to base address of AES module, make sure it matches Qsys
volatile unsigned int * AES_PTR = (unsigned int *) 0x00000100;

// Execution mode: 0 for testing, 1 for benchmarking
int run_mode = 0;

/** charToHex
 *  Convert a single character to the 4-bit value it represents.
 *  
 *  Input: a character c (e.g. 'A')
 *  Output: converted 4-bit value (e.g. 0xA)
 */
char charToHex(char c)
{
	char hex = c;

	if (hex >= '0' && hex <= '9')
		hex -= '0';
	else if (hex >= 'A' && hex <= 'F')
	{
		hex -= 'A';
		hex += 10;
	}
	else if (hex >= 'a' && hex <= 'f')
	{
		hex -= 'a';
		hex += 10;
	}
	return hex;
}

/** charsToHex
 *  Convert two characters to byte value it represents.
 *  Inputs must be 0-9, A-F, or a-f.
 *  
 *  Input: two characters c1 and c2 (e.g. 'A' and '7')
 *  Output: converted byte value (e.g. 0xA7)
 */
char charsToHex(char c1, char c2)
{
	char hex1 = charToHex(c1);
	char hex2 = charToHex(c2);
	return (hex1 << 4) + hex2;
}

unsigned int RotWord(unsigned int word) {
	unsigned int afterRotation = 0;
	unsigned int bitmask = 0x000000FF;
    
    // Rotate right
	unsigned int digit0 = ((bitmask << 24) & word) >> 24;
	
	// Rotate left
	unsigned int digit1 = (bitmask & word) << 8;
	unsigned int digit2 = ((bitmask << 8) & word) << 8;
	unsigned int digit3 = ((bitmask << 16) & word) << 8;
    
    
	return digit3 + digit2 + digit1 + digit0;
}

unsigned int SubWord(unsigned int word) {
    
	unsigned int bitmask = 0x000000FF;
	unsigned int output = 0;
	unsigned int aes_index = 0;
	for (unsigned int i = 0; i < 4; i++){
	    if (word == 0)
	        aes_index = 0;
	    else
		    aes_index = ((bitmask << (i*8)) & word) >> (i*8);
		output = output + (aes_sbox[aes_index] << (i*8));
	}

	return output;
}

void KeyExpansion(unsigned char * hexKey, unsigned int * expandedKey) {
	int i = 0;
	unsigned int temp = 0;
    
	while (i < 4) {
		expandedKey[i] = (hexKey[4*i] << 24) + (hexKey[4*i+1] << 16) + (hexKey[4*i+2] << 8) + (hexKey[4*i+3]);
		i = i+1;
	}

	i = 4;
	while (i < 44) {
		temp = expandedKey[i-1];

		if (i % 4 == 0){
			temp = SubWord(RotWord(temp)) ^ Rcon[i/4];
		}
			
		expandedKey[i] = expandedKey[i-4] ^ temp;
		i = i + 1;
	}
}

void AddRoundKey(unsigned char * state, unsigned int * expandedKey, unsigned int offset) {
	for (unsigned int i = 0; i < 4; i++){
		state[i*4] = state[i*4] ^ (expandedKey[i+offset] >> 24);
        state[i*4 + 1] = state[i*4 + 1] ^ (expandedKey[i+offset] >> 16);
        state[i*4 + 2] = state[i*4 + 2] ^ (expandedKey[i+offset] >> 8);
        state[i*4 + 3] = state[i*4 + 3] ^ expandedKey[i+offset];
	}
}

void SubBytes(unsigned char * state) {
	for (int i = 0; i < 16; i++) {
		state[i] = aes_sbox[state[i]];
	}
}

void ShiftRows(unsigned char * state) {
	unsigned char afterRotation[16];

	// First row stays same
	afterRotation[0] = state[0];
	afterRotation[4] = state[4];
	afterRotation[8] = state[8];
	afterRotation[12] = state[12];

	// Second row shifts by 1
	afterRotation[1] = state[5];
	afterRotation[5] = state[9];
	afterRotation[9] = state[13];
	afterRotation[13] = state[1];

	// Third row shifts by 2
	afterRotation[2] = state[10];
	afterRotation[6] = state[14];
	afterRotation[10] = state[2];
	afterRotation[14] = state[6];

	// Fourth row shifts by 3
	afterRotation[3] = state[15];
	afterRotation[7] = state[3];
	afterRotation[11] = state[7];
	afterRotation[15] = state[11];

	for (unsigned int i = 0 ; i < 16; i++){
		state[i] = afterRotation[i];
	}
}

unsigned char xtime(unsigned char x) {
    int flag = 0;
    
    if ((x>>7) & 1)
        flag = 1;
    
	x = x << 1;
	
	if (flag) {
		x = x ^ 0x11b;
	}

    return x;
}

unsigned char gf_mul2(unsigned char stateByte) {
	return xtime(stateByte);
}

unsigned char gf_mul3(unsigned char stateByte) {
	return xtime(stateByte) ^ stateByte;
}

void MixColumns(unsigned char * state) {
	unsigned char gf_multiplied[16];

	gf_multiplied[0] = (gf_mul2(state[0]) ^ gf_mul3(state[1]) ^ state[2] ^ state[3]);
	gf_multiplied[1] = (state[0] ^ gf_mul2(state[1]) ^ gf_mul3(state[2]) ^ state[3]);
	gf_multiplied[2] = (state[0] ^ state[1] ^ gf_mul2(state[2]) ^ gf_mul3(state[3]));
	gf_multiplied[3] = (gf_mul3(state[0]) ^ state[1] ^ state[2] ^ gf_mul2(state[3]));

	gf_multiplied[4] = (gf_mul2(state[4]) ^ gf_mul3(state[5]) ^ state[6] ^ state[7]);
	gf_multiplied[5] = (state[4] ^ gf_mul2(state[5]) ^ gf_mul3(state[6]) ^ state[7]);
	gf_multiplied[6] = (state[4] ^ state[5] ^ gf_mul2(state[6]) ^ gf_mul3(state[7]));
	gf_multiplied[7] = (gf_mul3(state[4]) ^ state[5] ^ state[6] ^ gf_mul2(state[7]));

	gf_multiplied[8] = (gf_mul2(state[8]) ^ gf_mul3(state[9]) ^ state[10] ^ state[11]);
	gf_multiplied[9] = (state[8] ^ gf_mul2(state[9]) ^ gf_mul3(state[10]) ^ state[11]);
	gf_multiplied[10] = (state[8] ^ state[9] ^ gf_mul2(state[10]) ^ gf_mul3(state[11]));
	gf_multiplied[11] = (gf_mul3(state[8]) ^ state[9] ^ state[10] ^ gf_mul2(state[11]));

	gf_multiplied[12] = (gf_mul2(state[12]) ^ gf_mul3(state[13]) ^ state[14] ^ state[15]);
	gf_multiplied[13] = (state[12] ^ gf_mul2(state[13]) ^ gf_mul3(state[14]) ^ state[15]);
	gf_multiplied[14] = (state[12] ^ state[13] ^ gf_mul2(state[14]) ^ gf_mul3(state[15]));
	gf_multiplied[15] = (gf_mul3(state[12]) ^ state[13] ^ state[14] ^ gf_mul2(state[15]));

	for (unsigned int i = 0 ; i < 16; i++){
		state[i] = gf_multiplied[i];
	}
}

/** encrypt
 *  Perform AES encryption in software.
 *
 *  Input: msg_ascii - Pointer to 32x 8-bit char array that contains the input message in ASCII format
 *         key_ascii - Pointer to 32x 8-bit char array that contains the input key in ASCII format
 *  Output:  msg_enc - Pointer to 4x 32-bit int array that contains the encrypted message
 *               key - Pointer to 4x 32-bit int array that contains the input key
 */
void encrypt(unsigned char * msg_ascii, unsigned char * key_ascii, unsigned int * msg_enc, unsigned int * key)
{   
    
	// Implement this function
	unsigned char state[16];
	unsigned char hexKey[16];
	for (unsigned int i = 0; i < 16; i++) {
		state[i] = charsToHex(msg_ascii[i*2], msg_ascii[i*2 + 1]);
		hexKey[i] = charsToHex(key_ascii[i*2], key_ascii[i*2 + 1]);
	}

	int rounds = 9;
	int keyOffset = 0;

	unsigned int expandedKey[44];
	KeyExpansion(hexKey, expandedKey);
	
	AddRoundKey(state, expandedKey, 0);
    
	for (unsigned int i = 0; i < rounds ; i++) {
		SubBytes(state);
		ShiftRows(state);
		MixColumns(state);
		AddRoundKey(state, expandedKey, (i+1)*4);
	}

	SubBytes(state);
	ShiftRows(state);
	AddRoundKey(state, expandedKey, 40);

	for (unsigned int i = 0; i < 4; i++) {
		msg_enc[i] = (state[i*4]<<24) + (state[i*4+1]<<16) + (state[i*4+2]<<8) + (state[i*4+3]);
		key[i] = (hexKey[i*4]<<24) + (hexKey[i*4+1]<<16) + (hexKey[i*4+2]<<8) + (hexKey[i*4+3]);
	}
	
	printf("\nEncrypted state is: \n");
			for(int i = 0; i < 16; i++){
				printf("%08lx\n", state[i]);
    }
}

/** decrypt
 *  Perform AES decryption in hardware.
 *
 *  Input:  msg_enc - Pointer to 4x 32-bit int array that contains the encrypted message
 *              key - Pointer to 4x 32-bit int array that contains the input key
 *  Output: msg_dec - Pointer to 4x 32-bit int array that contains the decrypted message
 */
void decrypt(unsigned int * msg_enc, unsigned int * msg_dec, unsigned int * key)
{
	// Implement this function
}

int main()
{
	// Input Message and Key as 32x 8-bit ASCII Characters ([33] is for NULL terminator)
	unsigned char msg_ascii[33];
	unsigned char key_ascii[33];
	// Key, Encrypted Message, and Decrypted Message in 4x 32-bit Format to facilitate Read/Write to Hardware
	unsigned int key[4];
	unsigned int msg_enc[4];
	unsigned int msg_dec[4];

	printf("Select execution mode: 0 for testing, 1 for benchmarking: ");
	scanf("%d", &run_mode);

	if (run_mode == 0) {
		// Continuously Perform Encryption and Decryption
		while (1) {
			int i = 0;
			printf("\nEnter Message:\n");
			scanf("%s", msg_ascii);
			printf("\n");
			printf("\nEnter Key:\n");
			scanf("%s", key_ascii);
			printf("\n");
			encrypt(msg_ascii, key_ascii, msg_enc, key);
			printf("\nEncrpted message is: \n");
			for(i = 0; i < 4; i++){
				printf("%08x\n", msg_enc[i]);
			}
			printf("\n");

			AES_PTR[0] = key[0];
			AES_PTR[1] = key[1];
			AES_PTR[2] = key[2];
			AES_PTR[3] = key[3];

			AES_PTR[4] = msg_enc[0];
			AES_PTR[5] = msg_enc[1];
			AES_PTR[6] = msg_enc[2];
			AES_PTR[7] = msg_enc[3];

			AES_PTR[10] = 0xDEADBEEF;
			if (AES_PTR[10] != 0xDEADBEEF)
				printf("Error!");

			decrypt(msg_enc, msg_dec, key);
//			printf("\nDecrypted message is: \n");
//			for(i = 0; i < 4; i++){
//				printf("%08x", msg_dec[i]);
//			}
			printf("\n");
		}
	}
	else {
		// Run the Benchmark
		int i = 0;
		int size_KB = 2;
		// Choose a random Plaintext and Key
		for (i = 0; i < 32; i++) {
			msg_ascii[i] = 'a';
			key_ascii[i] = 'b';
		}
		// Run Encryption
		clock_t begin = clock();
		for (i = 0; i < size_KB * 64; i++)
			encrypt(msg_ascii, key_ascii, msg_enc, key);
		clock_t end = clock();
		double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
		double speed = size_KB / time_spent;
		printf("Software Encryption Speed: %f KB/s \n", speed);
		// Run Decryption
		begin = clock();
		for (i = 0; i < size_KB * 64; i++)
			decrypt(msg_enc, msg_dec, key);
		end = clock();
		time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
		speed = size_KB / time_spent;
		printf("Hardware Encryption Speed: %f KB/s \n", speed);
	}
	return 0;
}