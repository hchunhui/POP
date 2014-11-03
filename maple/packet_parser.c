#include <stdlib.h>
#include <assert.h>
#include "packet_parser.h"

struct packet_parser
{
	const uint8_t *data;
	int length;
};

struct packet_parser *packet_parser(const uint8_t *data, int length)
{
	struct packet_parser *pp = malloc(sizeof(struct packet_parser));
	pp->data = data;
	pp->length = length;
	return pp;
}

void packet_parser_free(struct packet_parser *pp)
{
	free(pp);
}

void packet_parser_reset(struct packet_parser *pp)
{
}

void packet_parser_pull(struct packet_parser *pp)
{
}

value_t packet_parser_read(struct packet_parser *pp, const char *field)
{
	value_t v = {{0}};
	const uint8_t *data = pp->data;
	assert(pp->length >= 38);

	if(strcmp(field, "dl_dst") == 0)
		v = value_extract(data, 0, 48);
	else if(strcmp(field, "dl_src") == 0)
		v = value_extract(data, 48, 48);
	else if(strcmp(field, "dl_type") == 0)
		v = value_extract(data, 96, 16);
	else if(strcmp(field, "nw_proto") == 0)
		v = value_extract(data, 112+64+8, 8);
	else if(strcmp(field, "nw_src") == 0)
		v = value_extract(data, 112+96, 32);
	else if(strcmp(field, "nw_dst") == 0)
		v = value_extract(data, 112+128, 32);
	else if(strcmp(field, "tp_src") == 0)
		v = value_extract(data, 112+160, 16);
	else if(strcmp(field, "tp_dst") == 0)
		v = value_extract(data, 112+176, 16);
	return v;
}

void packet_parser_read_type(struct packet_parser *pp, char *buf, int len)
{
}
