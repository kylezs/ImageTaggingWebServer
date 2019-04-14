/* * * * * * *
 * Module providing several hash functions for strings
 *
 * created for COMP20007 Design of Algorithms
 * by Matt Farrugia <matt.farrugia@unimelb.edu.au>
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "strhash.h"

#define MAX_KEY_LEN 128

// hash everything to a random value (mod size)
unsigned int random_hash(const char *key, unsigned int size) {
	return rand() % size;
}

// hash everything to zero
unsigned int zero_hash(const char *key, unsigned int size) {
	return 0;
}

/* * *
 * ADD YOUR OWN HASH FUNCTIONS HERE
 */

// hash everything to the ascii value of its first character (mod size)
unsigned int ascii_hash(const char *key, unsigned int size) {
	return key[0] % size;
}

// hash everything to its length (mod size)
unsigned int length_hash(const char *key, unsigned int size) {
	return strlen(key) % size;
	// OR
	// int len = 0;
	// while (key[len] != '\0') {
	// 	len++;
	// }
	// return len % size;
}

// hash everything to the unsigned integer made up of its first few bytes
// short strings will leave later bytes blank
// (NOTE: careful, order of bytes in an integer may not be what you expect,
// it's system-dependent. look up 'endianness' for details)
unsigned int prefix_hash(const char *key, unsigned int size) {
	unsigned int h = 0;
	char *buffer = (char *)&h;

	int numchars = strlen(key);
	int intchars = sizeof(unsigned int);

	int i;
	for (i = 0; i < intchars && i < numchars; i++) {
		buffer[i] = key[i];
	}

	return h % size;
}

// xor hash from lectures, with seed 73802
unsigned int seed = 73802;
unsigned int xor_hash(const char *key, unsigned int size) {
	unsigned int h = seed;

	int i;
	for (i = 0; key[i] != '\0'; i++) {
		h = h ^ ((h << 5) + key[i] + (h >> 2));
	}

	return h % size;
}

// random-array based universal hash function
// uses a static flag to initialise the random array the first time the function
// is called, and uses the same array afterwards
// assumes strlen(key) <= MAX_KEY_LEN = 128
unsigned int universal_hash(const char *key, unsigned int size) {
	static int *r = NULL;
	if (!r) {
		r = malloc(MAX_KEY_LEN * (sizeof *r));
		assert(r);

		int i;
		for (i = 0; i < MAX_KEY_LEN; i++) {
			r[i] = rand();
		}
	}

	unsigned int h = 0;
	int i;
	for (i = 0; key[i] != '\0'; i++) {
		h = h + r[i] * key[i];
	}

	return h % size;
}




// generate a hash value for key (a string) to a hash table of size entries,
// using hash method 'method'
// guarantees a return value between 0 (inclusive) and size (non inclusive)
unsigned int hash(const char *key, unsigned int size, char method) {
	switch(method) {
		// really bad hash functions:
		case '0':
			return zero_hash(key, size);
		case 'r':
			return random_hash(key, size);

		// still bad hash functions:
		case 'a':
			return ascii_hash(key, size);
		case 'l':
			return length_hash(key, size);
		case 'p':
			return prefix_hash(key, size);

		// better hash functions
		case 'x':
			return xor_hash(key, size);
		case 'u':
			return universal_hash(key, size);

		default:
			fprintf(stderr, "unknown hashing method '%c'\n", method);
			exit(1);
	}
}

// return the string name of a hash function specified by method
char *name(char method) {
	switch(method) {
		case '0':
			return "zero hash";
		case 'r':
			return "random hash";
		case 'a':
			return "ascii hash";
		case 'l':
			return "length hash";
		case 'p':
			return "prefix hash";
		case 'x':
			return "xor hash";
		case 'u':
			return "universal hash";
		default:
			return "unknown";
	}
}
