#include <stdlib.h>
#include <assert.h>
#include "packet_parser.h"

struct header {
	char name[32];
	int num_fields;
	struct {
		char name[32];
		int offset;
		int length;
	} fields[100];
	int length;
	int sel_idx;
	int num_next;
	struct {
		value_t v;
		struct header *h;
	} next[100];
};

struct header *header(const char *name)
{
	struct header *h = malloc(sizeof(struct header));
	strncpy(h->name, name, 32);
	h->name[31] = 0;
	h->num_fields = 0;
	h->length = 0;
	h->sel_idx = -1;
	h->num_next = 0;
	return h;
}

void header_add_field(struct header *h, const char *name, int offset, int length)
{
	int i = h->num_fields;
	assert(i < 100);
	strncpy(h->fields[i].name, name, 32);
	h->fields[i].name[31] = 0;
	h->fields[i].offset = offset;
	h->fields[i].length = length;
	h->num_fields++;
}

void header_set_length(struct header *h, int length)
{
	h->length = length;
}

void header_set_sel(struct header *h, const char *name)
{
	int i;
	for(i = 0; i < h->num_fields; i++)
		if(strcmp(name, h->fields[i].name) == 0) {
			h->sel_idx = i;
			return;
		}
	assert(0);
}

int header_get_sel_length(struct header *h)
{
	assert(h->sel_idx >= 0);
	return h->fields[h->sel_idx].length;
}

void header_add_next(struct header *h, value_t v, struct header *nh)
{
	int i = h->num_next;
	assert(i < 100);
	h->next[i].v = v;
	h->next[i].h = nh;
	h->num_next++;
}

const char *header_get_name(struct header *h)
{
	return h->name;
}

void header_free(struct header *h)
{
	free(h);
}

static struct header *build_header()
{
	struct header *eth = header("ethernet");
	struct header *ipv4 = header("ipv4");
	header_add_field(eth, "dl_dst", 0, 48);
	header_add_field(eth, "dl_src", 48, 48);
	header_add_field(eth, "dl_type", 96, 16);
	header_set_length(eth, 14);
	header_set_sel(eth, "dl_type");
	header_add_next(eth, value_from_16(0x0800), ipv4);
	header_add_field(ipv4, "nw_src", 96, 32);
	header_add_field(ipv4, "nw_dst", 128, 32);
	header_set_length(ipv4, 20);
	return eth;
}

struct packet_parser
{
	struct header *start;
	struct header *current;
	const uint8_t *head;
	const uint8_t *data;
	int length;
};

struct packet_parser *packet_parser(const uint8_t *data, int length)
{
	struct packet_parser *pp = malloc(sizeof(struct packet_parser));
	pp->head = data;
	pp->data = data;
	pp->length = length;
	pp->start = build_header();
	pp->current = pp->start;
	return pp;
}

void packet_parser_free(struct packet_parser *pp)
{
	free(pp);
}

void packet_parser_reset(struct packet_parser *pp)
{
	pp->head = pp->data;
	pp->current = pp->start;
}

void packet_parser_pull(struct packet_parser *pp)
{
	int i = pp->current->sel_idx;
	assert(i >= 0);
	int j;
	value_t v = value_extract(pp->head,
				  pp->current->fields[i].offset,
				  pp->current->fields[i].length);
	for(j = 0; j < pp->current->num_next; j++) {
		if(value_equ(pp->current->next[j].v, v)) {
			pp->head += pp->current->length;
			pp->current = pp->current->next[j].h;
			assert(pp->head <= pp->data + pp->length);
			return;
		}
	}
	assert(0);
}

value_t packet_parser_read(struct packet_parser *pp, const char *field)
{
	int i;
	for(i = 0; i < pp->current->num_fields; i++) {
		if(strcmp(field, pp->current->fields[i].name) == 0) {
			int offset = pp->current->fields[i].offset;
			int length = pp->current->fields[i].length;
			assert(pp->head + (offset + length + 7) / 8 <= pp->data + pp->length);
			return value_extract(pp->head, offset, length);
		}
	}
	assert(0);
}

const char *packet_parser_read_type(struct packet_parser *pp)
{
	return pp->current->name;
}
