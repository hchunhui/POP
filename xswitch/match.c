#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

void match_dump(struct match *m, char *buf, int n)
{
	int i;
	int offset = 0;
	offset += snprintf(buf + offset, n - offset, "MATCH: ");
	for(i = 0; i < m->fields_num; i++)
		offset += snprintf(buf + offset,
				   n - offset,
				   "%s = (%02x %02x %02x %02x %02x %02x) ",
				   m->m[i].name,
				   m->m[i].value.v[0], m->m[i].value.v[1],
				   m->m[i].value.v[2], m->m[i].value.v[3],
				   m->m[i].value.v[4], m->m[i].value.v[5]);
}

#ifdef ENABLE_WEB
#include "core/packet_parser.h"
int match_dump_json(struct match *m, struct header *h, char *buf)
{
	int i, k;
	int offset, length;
	int pos = 0;

	for(i = 0; i < m->fields_num; i++) {
		if(strcmp(m->m[i].name, "in_port"))
			header_get_field(h, m->m[i].name, &offset, &length);
		else
			length = 8;
		length = (length + 7) / 8;
		pos += sprintf(buf+pos, "\"%s\":\"", m->m[i].name);
		for(k = 0; k < length; k++) {
			pos += sprintf(buf+pos,
				       "%02x",
				       m->m[i].value.v[k]);
		}
		pos += sprintf(buf+pos,
			       "\"%s",
			       i == m->fields_num - 1 ? "" : ",");
	}
	return pos;
}
#endif
