#ifndef _MAP_H_
#define _MAP_H_

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "types.h"

typedef union { unsigned long v; void *p; } int_or_ptr_t;
typedef unsigned int (*hash_func_t)(int_or_ptr_t key);
typedef bool (*eq_func_t)(int_or_ptr_t k1, int_or_ptr_t k2);
typedef int_or_ptr_t (*dup_func_t)(int_or_ptr_t x);
typedef void (*free_func_t)(int_or_ptr_t x);

struct map *map(eq_func_t eq_key, hash_func_t hash_key,
		dup_func_t dup_key, free_func_t free_key);
void map_free(struct map *m);

void map_add_key(struct map *m, int_or_ptr_t key,
		 int_or_ptr_t init_val, eq_func_t eq_val, free_func_t free_val);
void map_del_key(struct map *m, int_or_ptr_t key);

void map_mod(struct map *m, int_or_ptr_t key, int_or_ptr_t val);
void map_mod2(struct map *m, int_or_ptr_t key,
	      int_or_ptr_t val, eq_func_t eq_val, free_func_t free_val);

int_or_ptr_t map_read(struct map *m, int_or_ptr_t key);


static inline int_or_ptr_t INT(unsigned long v)
{
	int_or_ptr_t x;
	x.v = v;
	return x;
}

static inline int_or_ptr_t PTR(void *p)
{
	int_or_ptr_t x;
	x.p = p;
	return x;
}


static inline bool mapf_eq_str(int_or_ptr_t s1, int_or_ptr_t s2)
{
	return strcmp(s1.p, s2.p) == 0;
}

static inline unsigned int mapf_hash_str(int_or_ptr_t str)
{
	const char *s = str.p;
	unsigned int seed = 131;
	unsigned int hash = 0;

	while(*s)
		hash = hash * seed + (*s++);

	return hash;
}

static inline int_or_ptr_t mapf_dup_str(int_or_ptr_t str)
{
	return PTR(strdup(str.p));
}

static inline void mapf_free_str(int_or_ptr_t str)
{
	free(str.p);
}

static inline bool mapf_eq_int(int_or_ptr_t k1, int_or_ptr_t k2)
{
	return k1.v == k2.v;
}

static inline unsigned int mapf_hash_int(int_or_ptr_t key)
{
	return key.v;
}

static inline int_or_ptr_t mapf_dup_int(int_or_ptr_t x)
{
	return x;
}

static inline void mapf_free_int(int_or_ptr_t x)
{
}

static inline bool mapf_eq_map(int_or_ptr_t m1, int_or_ptr_t m2)
{
	return false;
}

static inline void mapf_free_map(int_or_ptr_t m)
{
	map_free(m.p);
}

#endif /* _MAP_H_ */
