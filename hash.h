/*
 * =====================================================================================
 *
 *       Filename:  hash.h
 *
 *    Description:
 *
 *        Version:  1.0
 *        Created:  08/01/2012 12:58:37 PM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  jayi (), hjy322@gmail.com
 *        Company:
 *
 * =====================================================================================
 */
#ifndef _H_HASH
#define _H_HASH

#define HASH_SIZE 1048576	// (1 << 20)
typedef unsigned long elem_t;

struct hash_node {
	elem_t key;
	int value;
};

elem_t elf_hash(char *key);

void init_hash_table(struct hash_node *hash_table);

void hash_insert(elem_t key, int value, struct hash_node *hash_table);

int hash_get(elem_t key, struct hash_node *hash_table);
#endif
