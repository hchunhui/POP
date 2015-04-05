#ifndef _PACKET_PARSER_H_
#define _PACKET_PARSER_H_

#include <inttypes.h>
#include "value.h"

struct action;
struct flow_table;
struct header;
struct expr;

enum expr_type {
	EXPR_FIELD,
	EXPR_VALUE,
	EXPR_NOT,
	EXPR_ADD,
	EXPR_SUB,
	EXPR_SHL,
	EXPR_SHR,
	EXPR_AND,
	EXPR_OR,
	EXPR_XOR,
};

struct expr *expr_field(const char *name);
struct expr *expr_value(uint32_t value);
struct expr *expr_op1(enum expr_type type, struct expr *sub_expr);
struct expr *expr_op2(enum expr_type type, struct expr *left, struct expr *right);
void expr_free(struct expr *e);
void expr_generate_action(struct expr *e,
			  struct header *spec, struct flow_table *ft, int base,
			  struct action *a);
void expr_generate_action_backward(struct expr *e, int base, struct action *a);

struct header *header(const char *name);
void header_add_field(struct header *h, const char *name, int offset, int length);
void header_get_field(struct header *h, const char *name, int *offset, int *length);
void header_set_length(struct header *h, struct expr *e);
struct expr *header_get_length(struct header *h);
void header_set_sel(struct header *h, const char *name);
const char *header_get_sel(struct header *h);
int header_get_sel_length(struct header *h);
void header_add_next(struct header *h, value_t v, struct header *nh);
struct header *header_lookup(struct header *start, const char *name);
const char *header_get_name(struct header *h);
struct flow_table *header_make_flow_table(struct header *h, int tid);
void header_free(struct header *h);

struct packet_parser;
struct packet_parser *packet_parser(struct header *spec, uint8_t *data, int length);
void packet_parser_free(struct packet_parser *pp);
void packet_parser_reset(struct packet_parser *pp);
void packet_parser_pull(struct packet_parser *pp,
			struct header **old_spec, value_t *sel_val, struct header **new_spec,
			int *next_stack_top);
void packet_parser_push(struct packet_parser *pp, struct header **new_spec,
			int *prev_stack_top);
value_t packet_parser_read(struct packet_parser *pp, const char *field);
uint32_t packet_parser_read_to_32(struct packet_parser *pp, const char *field);
const char *packet_parser_read_type(struct packet_parser *pp);
const uint8_t *packet_parser_get_payload(struct packet_parser *pp, int *length);
void packet_parser_mod(struct packet_parser *pp, const char *field, value_t value,
		       struct header **spec);
void packet_parser_add_header(struct packet_parser *pp, struct header *add_spec, int *phlen);

#endif /* _PACKET_PARSER_H_ */
