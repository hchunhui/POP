#ifndef _PACKET_PARSER_H_
#define _PACKET_PARSER_H_

#include <inttypes.h>
#include "value.h"

struct header;
struct header *header(const char *name);
void header_add_field(struct header *h, const char *name, int offset, int length);
void header_set_length(struct header *h, int length);
int header_get_length(struct header *h);
void header_set_sel(struct header *h, const char *name);
const char *header_get_sel(struct header *h);
int header_get_sel_length(struct header *h);
void header_add_next(struct header *h, value_t v, struct header *nh);
const char *header_get_name(struct header *h);
struct flow_table *header_make_flow_table(struct header *h, int tid);
void header_free(struct header *h);

struct packet_parser;
struct packet_parser *packet_parser(struct header *spec, const uint8_t *data, int length);
void packet_parser_free(struct packet_parser *pp);
void packet_parser_reset(struct packet_parser *pp);
void packet_parser_pull(struct packet_parser *pp,
			struct header **old_spec, value_t *sel_val, struct header **new_spec);
value_t packet_parser_read(struct packet_parser *pp, const char *field);
const char *packet_parser_read_type(struct packet_parser *pp);

#endif /* _PACKET_PARSER_H_ */
