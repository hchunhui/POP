#include <stdlib.h>
#include <assert.h>
#include "xswitch-private.h"

struct match *match(void)
{
	struct match *m = malloc(sizeof(struct match));
	m->fields_num = 0;
	return m;
}

void match_add(struct match *m, const char *name, value_t value, value_t mask)
{
	int n = m->fields_num;
	assert(n < FLOW_TABLE_NUM_FIELDS);
	strncpy(m->m[n].name, name, MATCH_FIELD_NAME_LEN);
	m->m[n].name[MATCH_FIELD_NAME_LEN - 1] = 0;
	m->m[n].value = value;
	m->m[n].mask = mask;
	m->fields_num++;
}

void match_free(struct match *m)
{
	free(m);
}

struct match *match_copy(struct match *m)
{
	struct match *mm = match();
	memcpy(mm, m, sizeof(struct match));
	return mm;
}
