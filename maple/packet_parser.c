#include <stdlib.h>
#include <assert.h>
#include "xswitch/xswitch.h"
#include "packet_parser.h"


struct expr {
	enum expr_type type;
	union {
		struct expr *sub_expr;
		struct {
			struct expr *left;
			struct expr *right;
		} binop;
		uint32_t value;
		char field[32];
	} u;
};

struct expr *expr_field(const char *name)
{
	struct expr *e = malloc(sizeof(struct expr));
	e->type = EXPR_FIELD;
	strncpy(e->u.field, name, 32);
	e->u.field[31] = 0;
	return e;
}

struct expr *expr_value(uint32_t value)
{
	struct expr *e = malloc(sizeof(struct expr));
	e->type = EXPR_VALUE;
	e->u.value = value;
	return e;
}

struct expr *expr_op1(enum expr_type type, struct expr *sub_expr)
{
	struct expr *e = malloc(sizeof(struct expr));
	e->type = type;
	e->u.sub_expr = sub_expr;
	return e;
}

struct expr *expr_op2(enum expr_type type, struct expr *left, struct expr *right)
{
	struct expr *e = malloc(sizeof(struct expr));
	e->type = type;
	e->u.binop.left = left;
	e->u.binop.right = right;
	return e;
}

void expr_free(struct expr *e)
{
	if(!e)
		return;
	switch(e->type) {
	case EXPR_FIELD:
	case EXPR_VALUE:
		free(e);
		break;
	case EXPR_NOT:
		expr_free(e->u.sub_expr);
		free(e);
		break;
	default:
		expr_free(e->u.binop.left);
		expr_free(e->u.binop.right);
		free(e);
		break;
	}
}

static uint32_t expr_interp(struct expr *e, struct packet_parser *pp)
{
	switch(e->type) {
	case EXPR_FIELD:
		return packet_parser_read_to_32(pp, e->u.field);
	case EXPR_VALUE:
		return e->u.value;
	case EXPR_NOT:
		return ~ expr_interp(e->u.sub_expr, pp);
	case EXPR_ADD:
		return expr_interp(e->u.binop.left, pp) + expr_interp(e->u.binop.right, pp);
	case EXPR_SUB:
		return expr_interp(e->u.binop.left, pp) - expr_interp(e->u.binop.right, pp);
	case EXPR_SHL:
		return expr_interp(e->u.binop.left, pp) << expr_interp(e->u.binop.right, pp);
	case EXPR_SHR:
		return expr_interp(e->u.binop.left, pp) >> expr_interp(e->u.binop.right, pp);
	case EXPR_AND:
		return expr_interp(e->u.binop.left, pp) & expr_interp(e->u.binop.right, pp);
	case EXPR_OR:
		return expr_interp(e->u.binop.left, pp) | expr_interp(e->u.binop.right, pp);
	case EXPR_XOR:
		return expr_interp(e->u.binop.left, pp) ^ expr_interp(e->u.binop.right, pp);
	}
	return 0;
}

/*
 * Metadata configuration:
 * offset |01234567|01234567|01234567|01234567|
 *     0  |      length     | inport |reserved|
 *    32  |compRes |          not uesd        |
 *    64  |                r0                 |
 *    96  |                r1                 |
 *   128  |                r2                 |
 *   160  |               ....                |
 *   ...
 */
#define R_offset(x) (64 + 32*(x))
#define R_length    32

/*
 * expr_gen() performs a stack-based translation.
 * r{0, 1, 2, ...} are viewed as a "register stack",
 * "stage" points to the top of the stack.
 */
static void expr_gen(struct expr *e, struct flow_table *ft, struct action *a, int stage)
{
	enum action_oper_type op_type;
	int offset, length;
	switch(e->type) {
	case EXPR_FIELD:
		/* load packet data to stage:
		 *   xor  r(stage), r(stage)
		 *   add  r(stage), packet(offset, length)
		 */
		flow_table_get_offset_length(ft,
					     flow_table_get_field_index(ft, e->u.field),
					     &offset,
					     &length);
		action_add_calc_r(a, AC_OP_XOR,
				  MATCH_FIELD_METADATA, R_offset(stage), R_length,
				  MATCH_FIELD_METADATA, R_offset(stage), R_length);
		action_add_calc_r(a, AC_OP_ADD,
				  MATCH_FIELD_METADATA, R_offset(stage), R_length,
				  MATCH_FIELD_PACKET, offset, length);
		break;
	case EXPR_VALUE:
		/* load imm to stage:
		 *   write_metadata r(stage), imm
		 */
		action_add_write_metadata(a, R_offset(stage), R_length,
					  value_from_32(e->u.value));
		break;
	case EXPR_NOT:
		/* recursively generate sub expr to stage,
		 * then do:
		 *   nori r(stage), 0
		 */
		expr_gen(e->u.sub_expr, ft, a, stage);
		action_add_calc_i(a, AC_OP_NOR,
				  MATCH_FIELD_METADATA, R_offset(stage), R_length,
				  0);
		break;
	default:
		/* recursively generate sub exprs to stage and (stage+1)
		 * then do:
		 *   op r(stage), r(stage+1)
		 */
		switch(e->type) {
		case EXPR_ADD: op_type = AC_OP_ADD; break;
		case EXPR_SUB: op_type = AC_OP_SUB; break;
		case EXPR_SHL: op_type = AC_OP_SHL; break;
		case EXPR_SHR: op_type = AC_OP_SHR; break;
		case EXPR_AND: op_type = AC_OP_AND; break;
		case EXPR_OR:  op_type = AC_OP_OR;  break;
		case EXPR_XOR: op_type = AC_OP_XOR; break;
		default: assert(0);
		}
		expr_gen(e->u.binop.left, ft, a, stage);

		/* before generate the right expr, check if we can do it better */
		if(e->u.binop.right->type == EXPR_VALUE) {
			action_add_calc_i(a, op_type,
					  MATCH_FIELD_METADATA, R_offset(stage), R_length,
					  e->u.binop.right->u.value);
		} else {
			/* fall back to generic procedure */
			expr_gen(e->u.binop.right, ft, a, stage + 1);
			action_add_calc_r(a, op_type,
					  MATCH_FIELD_METADATA, R_offset(stage), R_length,
					  MATCH_FIELD_METADATA, R_offset(stage+1), R_length);
		}
		break;
	}
}

void expr_generate_action(struct expr *e, struct flow_table *ft, struct action *a)
{
	int offset, length;
	switch(e->type) {
	case EXPR_VALUE:
		/* Fixed value is the most common case */
		action_add_goto_table(a, flow_table_get_tid(ft), e->u.value);
		break;
	case EXPR_FIELD:
		/* Handle a single field name is simple */
		flow_table_get_offset_length(ft,
					     flow_table_get_field_index(ft, e->u.field),
					     &offset,
					     &length);
		action_add_move_packet(a, MATCH_FIELD_PACKET, offset, length);
		action_add_goto_table(a, flow_table_get_tid(ft), 0);
		break;
	default:
		/* The expr is complex, do generic procedure */
		expr_gen(e, ft, a, 0);
		action_add_move_packet(a, MATCH_FIELD_METADATA, R_offset(0), R_length);
		action_add_goto_table(a, flow_table_get_tid(ft), 0);
		break;
	}
}

struct header {
	char name[32];
	int num_fields;
	struct {
		char name[32];
		int offset;
		int length;
	} fields[100];
	struct expr *length;
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
	h->length = NULL;
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

void header_set_length(struct header *h, struct expr *e)
{
	expr_free(h->length);
	h->length = e;
}

struct expr *header_get_length(struct header *h)
{
	return h->length;
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

const char *header_get_sel(struct header *h)
{
	assert(h->sel_idx >= 0);
	return h->fields[h->sel_idx].name;
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

struct flow_table *header_make_flow_table(struct header *h, int tid)
{
	int i;
	struct flow_table *ft = flow_table(tid, FLOW_TABLE_TYPE_MM, 1024);
	flow_table_add_field(ft, "in_port", MATCH_FIELD_METADATA, 16, 8);
	for(i = 0; i < h->num_fields; i++)
		if(h->fields[i].length != 0)
			flow_table_add_field(ft, h->fields[i].name, MATCH_FIELD_PACKET,
					     h->fields[i].offset, h->fields[i].length);
	return ft;
}

void header_free(struct header *h)
{
	expr_free(h->length);
	free(h);
}

struct packet_parser
{
	struct header *start;
	struct header *current;
	const uint8_t *head;
	const uint8_t *data;
	int length;
};

struct packet_parser *packet_parser(struct header *spec, const uint8_t *data, int length)
{
	struct packet_parser *pp = malloc(sizeof(struct packet_parser));
	pp->head = data;
	pp->data = data;
	pp->length = length;
	pp->start = spec;
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

void packet_parser_pull(struct packet_parser *pp,
			struct header **old_spec,
			value_t *sel_value,
			struct header **new_spec)
{
	int i = pp->current->sel_idx;
	assert(i >= 0);
	int j;
	value_t v = value_extract(pp->head,
				  pp->current->fields[i].offset,
				  pp->current->fields[i].length);
	*old_spec = pp->current;
	*sel_value = v;
	for(j = 0; j < pp->current->num_next; j++) {
		if(value_equal(pp->current->next[j].v, v)) {
			pp->head += expr_interp(pp->current->length, pp);
			pp->current = pp->current->next[j].h;
			assert(pp->head <= pp->data + pp->length);
			*new_spec = pp->current;
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

uint32_t packet_parser_read_to_32(struct packet_parser *pp, const char *field)
{
	int i;
	for(i = 0; i < pp->current->num_fields; i++) {
		if(strcmp(field, pp->current->fields[i].name) == 0) {
			int offset = pp->current->fields[i].offset;
			int length = pp->current->fields[i].length;
			value_t v;
			assert(pp->head + (offset + length + 7) / 8 <= pp->data + pp->length);
			v = value_extract(pp->head, offset, length);
			if(length < 8)
				return value_bits_to_8(length, v);
			else if (length == 8)
				return value_to_8(v);
			else if(length == 16)
				return value_to_16(v);
			else if(length == 32)
				return value_to_32(v);
			else {
				assert(0);
			}
		}
	}
	assert(0);
}

const char *packet_parser_read_type(struct packet_parser *pp)
{
	return pp->current->name;
}

const uint8_t *packet_parser_get_payload(struct packet_parser *pp, int *length)
{
	int header_length = expr_interp(pp->current->length, pp);
	const uint8_t *head = pp->head + header_length;
	if(length)
		*length = pp->length - (head - pp->data);
	return head;
}
