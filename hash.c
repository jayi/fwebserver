/*
 * =====================================================================================
 *
 *       Filename:  hash.c
 *
 *    Description:  
 *
 *        Version:  1.0
 *        Created:  08/01/2012 11:41:13 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  jayi (), hjy322@gmail.com
 *        Company:  
 *
 * =====================================================================================
 */
#include "hash.h"

elem_t elf_hash(char *key)
{
	elem_t h = 0, g;
	while (*key) {
		h = (h << 4) + (*key++);
		g = h & 0xf0000000L;
		if (g) h ^= g >> 24;
		h &= ~g;
	}
	return h;
}


void init_hash_table(struct hash_node *hash_table)
{
	int i;
	for (i = 0; i < HASH_SIZE; ++i) {
		hash_table[i].value = -1;
		hash_table[i].key = 0;
	}
}

void hash_insert(elem_t key, int value, struct hash_node *hash_table)
{
	int i = (int)(key & HASH_SIZE);
	while (hash_table[i].value != -1 && hash_table[i].key != key) {
		++i;
		i &= (HASH_SIZE - 1);
	}
	if (hash_table[i].value == -1) {
		hash_table[i].value = value;
		hash_table[i].key = key;
	}
}

int hash_get(elem_t key, struct hash_node *hash_table)
{
	int i = (int)(key & HASH_SIZE);
	while (hash_table[i].value != -1 && hash_table[i].key != key) {
		++i;
		i &= (HASH_SIZE - 1);
	}
	return hash_table[i].value;
}
