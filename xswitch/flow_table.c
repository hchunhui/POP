#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "xswitch-private.h"

struct flow_table *flow_table(int tid, enum flow_table_type type, int size)
{
	struct flow_table *ft;
	int sz = sizeof(unsigned long);
	int msize = (size + sz - 1) / sz;
	ft = malloc(sizeof(struct flow_table) + msize * sz);
	ft->tid = tid;
	ft->type = type;
	ft->size = size;
	ft->fields_num = 0;
	memset(ft->index_map, 0, msize * sz);
	return ft;
}

void flow_table_free(struct flow_table *ft)
{
	free(ft);
}

void flow_table_add_field(struct flow_table *ft,
			  const char *name, enum match_field_type type, int offset, int length)
{
	struct match_field *f = ft->fields + ft->fields_num;
	assert(ft->fields_num < FLOW_TABLE_NUM_FIELDS);
	strncpy(f->name, name, MATCH_FIELD_NAME_LEN);
	f->name[MATCH_FIELD_NAME_LEN - 1] = 0;
	f->type = type;
	f->offset = offset;
	f->length = length;
	ft->fields_num++;
}

int flow_table_get_field_index(struct flow_table *ft, const char *name)
{
	int i;
	for(i = 0; i < ft->fields_num; i++)
		if(strcmp(name, ft->fields[i].name) == 0)
			return i;
	return -1;
}

void flow_table_get_offset_length(struct flow_table *ft, int idx,
				  int *offset, int *length)
{
	assert(idx >= 0 && idx < ft->fields_num);
	*offset = ft->fields[idx].offset;
	*length = ft->fields[idx].length;
}

int flow_table_get_tid(struct flow_table *ft)
{
	return ft->tid;
}

int flow_table_get_entry_index(struct flow_table *ft)
{
	int i, j;
	int sz = sizeof(unsigned long);
	int m = (ft->size + sz - 1) / sz;
	for(i = 0; i < m; i++)
		if(ft->index_map[i] != ~(0ul))
			for(j = 0; j < sz; j++)
				if((ft->index_map[i] & (1 << j)) == 0) {
					ft->index_map[i] |= 1 << j;
					assert(i * sz + j < ft->size);
					return i * sz + j;
				}
	assert(0);
}

void flow_table_put_entry_index(struct flow_table *ft, int index)
{
	int sz = sizeof(unsigned long);
	assert(index >= 0 && index < ft->size);
	ft->index_map[index / sz] &= ~(1 << (index % sz));
}
