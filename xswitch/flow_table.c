#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "xswitch-private.h"

struct flow_table *flow_table(int tid, enum flow_table_type type, int size)
{
	struct flow_table *ft;
	ft = malloc(sizeof(struct flow_table));
	ft->tid = tid;
	ft->type = type;
	ft->size = size;
	ft->fields_num = 0;
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
