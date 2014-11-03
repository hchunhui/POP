#ifndef _PACKET_PARSER_H_
#define _PACKET_PARSER_H_

#include <inttypes.h>
#include "value.h"

struct packet_parser;
struct packet_parser *packet_parser(const uint8_t *data, int length);
void packet_parser_free(struct packet_parser *pp);
void packet_parser_reset(struct packet_parser *pp);
void packet_parser_pull(struct packet_parser *pp);
value_t packet_parser_read(struct packet_parser *pp, const char *field);
const char *packet_parser_read_type(struct packet_parser *pp);

#endif /* _PACKET_PARSER_H_ */
