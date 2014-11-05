#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "xswitch-private.h"

struct action *action(void)
{
	struct action *a = malloc(sizeof(struct action));
	a->num_actions = 0;
	return a;
}

void action_add2(struct action *a, enum action_type type, int arg1, int arg2)
{
	int n = a->num_actions;
	assert(n < ACTION_NUM_ACTIONS);
	a->a[n].type = type;
	a->a[n].arg1 = arg1;
	a->a[n].arg2 = arg2;
	a->num_actions++;
}

void action_add(struct action *a, enum action_type type, int arg1)
{
	action_add2(a, type, arg1, 0);
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
		for(i = 0; i < a1->num_actions; i++) {
			if(a1->a[i].type == a2->a[j].type &&
			   a1->a[i].arg1 == a2->a[j].arg1 &&
			   a1->a[i].arg2 == a2->a[j].arg2)
				break;
		}
		if(i >= a1->num_actions)
			action_add2(a1, a2->a[j].type, a2->a[j].arg1, a2->a[j].arg2);
	}
}

void action_dump(struct action *a, char *buf, int n)
{
	int i;
	int offset = 0;
	for(i = 0; i < a->num_actions; i++)
		switch(a->a[i].type) {
		case AC_DROP:
			offset += snprintf(buf + offset, n - offset, "DROP ");
			break;
		case AC_PACKET_IN:
			offset += snprintf(buf + offset, n - offset, "PACKET_IN ");
			break;
		case AC_OUTPUT:
			offset += snprintf(buf + offset, n - offset, "OUTPUT %d ", a->a[i].arg1);
			break;
		case AC_GOTO_TABLE:
			offset += snprintf(buf + offset, n - offset, "GOTO_TABLE %d off %d ",
					   a->a[i].arg1, a->a[i].arg2);
			break;
		default:
			offset += snprintf(buf + offset, n - offset, "unknown ");
		}
}
