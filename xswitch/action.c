#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "xswitch-private.h"

struct action *action(void)
{
	struct action *a = malloc(sizeof(struct action));
	a->num_actions = 0;
	return a;
}

void action_add_goto_table(struct action *a, int tid, int offset)
{
	int n = a->num_actions;
	assert(n < ACTION_NUM_ACTIONS);
	a->a[n].type = AC_GOTO_TABLE;
	a->a[n].u.goto_table.tid = tid;
	a->a[n].u.goto_table.offset = offset;
	a->num_actions++;
}

void action_add(struct action *a, enum action_type type, int arg)
{
	int n = a->num_actions;
	assert(n < ACTION_NUM_ACTIONS);
	a->a[n].type = type;
	a->a[n].u.arg = arg;
	a->num_actions++;
}

void action_add_calc_r(struct action *a, enum action_oper_type op_type,
		       enum match_field_type dst_type,
		       int dst_offset, int dst_length,
		       enum match_field_type src_type,
		       int src_offset, int src_length)
{
	int n = a->num_actions;
	assert(n < ACTION_NUM_ACTIONS);
	a->a[n].type = AC_CALC_R;
	a->a[n].u.op_r.op_type = op_type;
	a->a[n].u.op_r.dst_type = dst_type;
	a->a[n].u.op_r.dst_offset = dst_offset;
	a->a[n].u.op_r.dst_length = dst_length;
	a->a[n].u.op_r.src_type = src_type;
	a->a[n].u.op_r.src_offset = src_offset;
	a->a[n].u.op_r.src_length = src_length;
	a->num_actions++;
}

void action_add_calc_i(struct action *a, enum action_oper_type op_type,
		       enum match_field_type dst_type,
		       int dst_offset, int dst_length,
		       uint32_t src_value)
{
	int n = a->num_actions;
	assert(n < ACTION_NUM_ACTIONS);
	a->a[n].type = AC_CALC_I;
	a->a[n].u.op_i.op_type = op_type;
	a->a[n].u.op_i.dst_type = dst_type;
	a->a[n].u.op_i.dst_offset = dst_offset;
	a->a[n].u.op_i.dst_length = dst_length;
	a->a[n].u.op_i.src_value = src_value;
	a->num_actions++;
}

void action_add_write_metadata(struct action *a, int dst_offset, int dst_length, value_t val)
{
	int n = a->num_actions;
	assert(n < ACTION_NUM_ACTIONS);
	a->a[n].type = AC_WRITE_METADATA;
	a->a[n].u.write_metadata.dst_offset = dst_offset;
	a->a[n].u.write_metadata.dst_length = dst_length;
	a->a[n].u.write_metadata.val = val;
	a->num_actions++;
}

void action_add_set_field(struct action *a, int dst_offset, int dst_length, value_t val)
{
	int n = a->num_actions;
	assert(n < ACTION_NUM_ACTIONS);
	a->a[n].type = AC_SET_FIELD;
	a->a[n].u.set_field.dst_offset = dst_offset;
	a->a[n].u.set_field.dst_length = dst_length;
	a->a[n].u.set_field.val = val;
	a->num_actions++;
}

void action_add_move_packet(struct action *a, enum move_direction dir,
			    enum match_field_type type, int offset, int length)
{
	int n = a->num_actions;
	assert(n < ACTION_NUM_ACTIONS);
	a->a[n].type = AC_MOVE_PACKET;
	a->a[n].u.move_packet.dir = dir;
	a->a[n].u.move_packet.type = type;
	a->a[n].u.move_packet.offset = offset;
	a->a[n].u.move_packet.length = length;
	a->num_actions++;
}

void action_add_move_packet_imm(struct action *a, enum move_direction dir, int value)
{
	int n = a->num_actions;
	assert(n < ACTION_NUM_ACTIONS);
	a->a[n].type = AC_MOVE_PACKET_IMM;
	a->a[n].u.move_packet_imm.dir = dir;
	a->a[n].u.move_packet_imm.value = value;
	a->num_actions++;
}

void action_free(struct action *m)
{
	free(m);
}

struct action *action_copy(struct action *a)
{
	struct action *aa = action();
	memcpy(aa, a, sizeof(struct action));
	return aa;
}

int action_num_actions(struct action *a)
{
	return a->num_actions;
}

void action_union(struct action *a1, struct action *a2)
{
	int i, j;
	for(j = 0; j < a2->num_actions; j++) {
		assert(a2->a[j].type == AC_OUTPUT || a2->a[j].type == AC_DROP);
		for(i = 0; i < a1->num_actions; i++) {
			assert(a1->a[i].type == AC_OUTPUT || a1->a[i].type == AC_DROP);
			if(a1->a[i].type == a2->a[j].type &&
			   a1->a[i].u.arg == a2->a[j].u.arg)
				break;
		}
		if(i >= a1->num_actions)
			action_add(a1, a2->a[j].type, a2->a[j].u.arg);
	}
}

void action_dump(struct action *a, char *buf, int n)
{
	int i;
	int offset = 0;
	offset += snprintf(buf + offset, n - offset, "ACTION: ");
	for(i = 0; i < a->num_actions; i++)
		switch(a->a[i].type) {
		case AC_DROP:
			offset += snprintf(buf + offset, n - offset, "DROP ");
			break;
		case AC_PACKET_IN:
			offset += snprintf(buf + offset, n - offset, "PACKET_IN ");
			break;
		case AC_OUTPUT:
			offset += snprintf(buf + offset, n - offset, "OUTPUT %d ",
					   a->a[i].u.arg);
			break;
		case AC_GOTO_TABLE:
			offset += snprintf(buf + offset, n - offset, "GOTO_TABLE %d off %d ",
					   a->a[i].u.goto_table.tid,
					   a->a[i].u.goto_table.offset);
			break;
		default:
			offset += snprintf(buf + offset, n - offset, "unknown ");
		}
}
