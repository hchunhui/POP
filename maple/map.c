#include <stdlib.h>
#include <assert.h>
#include "trace.h"
#include "map.h"
#include "types.h"

#define HASH_TABLE_SIZE 256

struct entry {
	int_or_ptr_t key;
	int_or_ptr_t val;
	eq_func_t eq_val;
	free_func_t free_val;
	struct entry *next;
};

struct map {
	eq_func_t eq_key;
	hash_func_t hash_key;
	dup_func_t dup_key;
	free_func_t free_key;
	struct entry *table[HASH_TABLE_SIZE];
};

struct map *map(eq_func_t eq_key, hash_func_t hash_key,
		dup_func_t dup_key, free_func_t free_key)
{
	unsigned int i;
	struct map *m = malloc(sizeof(struct map));
	m->eq_key = eq_key;
	m->hash_key = hash_key;
	m->dup_key = dup_key;
	m->free_key = free_key;
	for(i = 0; i < HASH_TABLE_SIZE; i++) {
		m->table[i] = NULL;
	}
	return m;
}

void map_free(struct map *m)
{
	unsigned int i;
	for(i = 0; i < HASH_TABLE_SIZE; i++) {
		struct entry *e = m->table[i];
		while(e) {
			struct entry *p = e;
			e = e->next;
			m->free_key(p->key);
			p->free_val(p->val);
			free(p);
			trace_IE("map", p);
		}
	}
	free(m);
}

void map_add_key(struct map *m, int_or_ptr_t key,
		 int_or_ptr_t init_val, eq_func_t eq_val, free_func_t free_val)
{
	unsigned int idx = m->hash_key(key) % HASH_TABLE_SIZE;
	struct entry *e = m->table[idx];

	while(e) {
		if(m->eq_key(key, e->key))
			abort();
		e = e->next;
	}

	e = malloc(sizeof(struct entry));
	e->key = m->dup_key(key);
	e->val = init_val;
	e->eq_val = eq_val;
	e->free_val = free_val;
	e->next = m->table[idx];
	m->table[idx] = e;
	trace_IE("map", NULL);
}

void map_del_key(struct map *m, int_or_ptr_t key)
{
	unsigned int idx = m->hash_key(key) % HASH_TABLE_SIZE;
	struct entry *pe = m->table[idx];
	struct entry *e = m->table[idx];

	while(e) {
		if(m->eq_key(key, e->key)) {
			m->free_key(e->key);
			e->free_val(e->val);
			pe->next = e->next;
			free(e);
			trace_IE("map", e);
			return;
		}
		pe = e;
		e = e->next;
	}
	abort();
}

void map_mod(struct map *m, int_or_ptr_t key, int_or_ptr_t val)
{
	unsigned int idx = m->hash_key(key) % HASH_TABLE_SIZE;
	struct entry *e = m->table[idx];

	while(e) {
		if(m->eq_key(key, e->key)) {
			if(e->eq_val(e->val, val)) {
				e->free_val(val);
			} else {
				e->free_val(e->val);
				e->val = val;
				trace_IE("map", e);
			}
			return;
		}
		e = e->next;
	}
	abort();
}

void map_mod2(struct map *m, int_or_ptr_t key,
	      int_or_ptr_t val, eq_func_t eq_val, free_func_t free_val)
{
	unsigned int idx = m->hash_key(key) % HASH_TABLE_SIZE;
	struct entry *e = m->table[idx];

	while(e) {
		if(m->eq_key(key, e->key)) {
			assert(eq_val == e->eq_val);
			assert(free_val == e->free_val);
			if(e->eq_val(e->val, val)) {
				e->free_val(val);
			} else {
				e->free_val(e->val);
				e->val = val;
				trace_IE("map", e);
			}
			return;
		}
		e = e->next;
	}

	e = malloc(sizeof(struct entry));
	e->key = m->dup_key(key);
	e->val = val;
	e->eq_val = eq_val;
	e->free_val = free_val;
	e->next = m->table[idx];
	m->table[idx] = e;
	trace_IE("map", NULL);
}

int_or_ptr_t map_read(struct map *m, int_or_ptr_t key)
{
	unsigned int idx = m->hash_key(key) % HASH_TABLE_SIZE;
	struct entry *e = m->table[idx];

	while(e) {
		if(m->eq_key(key, e->key)) {
			trace_RE("map", e);
			return e->val;
		}
		e = e->next;
	}

	trace_RE("map", NULL);
	return PTR(NULL);
}
