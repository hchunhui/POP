#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "xswitch-private.h"
#include "maple.h"
#include "maple_api.h"
#include "topo.h"
#include "entity.h"
#include "packet_parser.h"

enum event_type { EV_R, EV_T, EV_RE };

struct event
{
	enum event_type type;
	union {
		struct {
			char name[32];
			value_t value;
		} r;
		struct {
			char name[32];
			value_t value;
			bool test;
		} t;
		struct {
			char name[32];
		} re;
	} u;
};

struct trace
{
	int num_events;
	struct event events[1024];
	int num_inv_events;
	struct {
		char name[32];
	} inv_events[1024];
};

static __thread struct trace trace = { 0 };

static void trace_R(const char *name, value_t value)
{
	int i = trace.num_events;
	trace.events[i].type = EV_R;
	strncpy(trace.events[i].u.r.name, name, 32);
	trace.events[i].u.r.name[31] = 0;
	trace.events[i].u.r.value = value;
	trace.num_events++;
}

static void trace_T(const char *name, value_t value, bool result)
{
	int i = trace.num_events;
	trace.events[i].type = EV_T;
	strncpy(trace.events[i].u.t.name, name, 32);
	trace.events[i].u.t.name[31] = 0;
	trace.events[i].u.t.value = value;
	trace.events[i].u.t.test = result;
	trace.num_events++;
}

static void trace_RE(const char *name)
{
	int i = trace.num_events;
	trace.events[i].type = EV_RE;
	strncpy(trace.events[i].u.re.name, name, 32);
	trace.events[i].u.re.name[31] = 0;
	trace.num_events++;
}

static void trace_IE(const char *name)
{
	int i = trace.num_inv_events;
	strncpy(trace.inv_events[i].name, name, 32);
	trace.inv_events[i].name[31] = 0;
	trace.num_inv_events++;
}

static void trace_clear(void)
{
	trace.num_events = 0;
	trace.num_inv_events = 0;
	trace_R("in_port", value_from_8(0));
}

static void trace_mod_in_port(int in_port)
{
	assert(trace.num_events > 0);
	assert(trace.events[0].type == EV_R);
	assert(strcmp(trace.events[0].u.r.name, "in_port") == 0);
	trace.events[0].u.r.value = value_from_8(in_port);
}

enum trace_tree_type { TT_E, TT_L, TT_V, TT_T, TT_D };
struct trace_tree_header
{
	enum trace_tree_type type;
};

struct trace_tree_E
{
	struct trace_tree_header h;
};

struct trace_tree_L
{
	struct trace_tree_header h;
	struct action *ac;
};

struct trace_tree_Vb {
	value_t value;
	struct trace_tree_header *tree;
};

struct trace_tree_V
{
	struct trace_tree_header h;
	char name[32];
	int num_branches;
	struct trace_tree_Vb branches[0];
};

struct trace_tree_T
{
	struct trace_tree_header h;
	char name[32];
	value_t value;
	struct trace_tree_header *t, *f;
};

struct trace_tree_D
{
	struct trace_tree_header h;
	char name[32];
	struct trace_tree_header *t;
};

static struct trace_tree_header *trace_tree_E(void)
{
	struct trace_tree_E *t = malloc(sizeof *t);
	t->h.type = TT_E;
	return (struct trace_tree_header *)t;
}

static struct trace_tree_header *trace_tree_L(struct action *a)
{
	struct trace_tree_L *t = malloc(sizeof *t);
	t->h.type = TT_L;
	t->ac = action_copy(a);
	return (struct trace_tree_header *)t;
}

static struct trace_tree_header *trace_tree_V(const char *name,
					      value_t v,
					      struct trace_tree_header *p)
{
	struct trace_tree_V *t = malloc(sizeof *t + sizeof (struct trace_tree_Vb));
	t->h.type = TT_V;
	strncpy(t->name, name, 32);
	t->name[31] = 0;
	t->num_branches = 1;
	t->branches[0].value = v;
	t->branches[0].tree = p;
	return (struct trace_tree_header *)t;
}

static struct trace_tree_header *trace_tree_T(const char *name,
					      value_t v,
					      struct trace_tree_header *tt,
					      struct trace_tree_header *tf)
{
	struct trace_tree_T *t = malloc(sizeof *t);
	t->h.type = TT_T;
	strncpy(t->name, name, 32);
	t->name[31] = 0;
	t->value = v;
	t->t = tt;
	t->f = tf;
	return (struct trace_tree_header *)t;
}

static struct trace_tree_header *trace_tree_D(const char *name,
					      struct trace_tree_header *tt)
{
	struct trace_tree_D *t = malloc(sizeof *t);
	t->h.type = TT_D;
	strncpy(t->name, name, 32);
	t->name[31] = 0;
	t->t = tt;
	return (struct trace_tree_header *)t;
}

static void trace_tree_free(struct trace_tree_header *t)
{
	int i;
	struct trace_tree_V *tv;
	struct trace_tree_T *tt;
	struct trace_tree_L *tl;
	struct trace_tree_D *td;
	switch(t->type) {
	case TT_E:
		free(t);
		break;
	case TT_L:
		tl = (struct trace_tree_L *)t;
		action_free(tl->ac);
		free(t);
		break;
	case TT_V:
		tv = (struct trace_tree_V *)t;
		for(i = 0; i < tv->num_branches; i++)
			trace_tree_free(tv->branches[i].tree);
		free(t);
		break;
	case TT_T:
		tt = (struct trace_tree_T *)t;
		trace_tree_free(tt->t);
		trace_tree_free(tt->f);
		free(t);
		break;
	case TT_D:
		td = (struct trace_tree_D *)t;
		trace_tree_free(td->t);
		free(t);
		break;
	}
}

/* -- algorithm  --*/
static struct trace_tree_header *events_to_tree(struct event *events, int num_events,
						struct action *a)
{
	int i;
	struct trace_tree_header *root;
	root = trace_tree_L(a);
	for(i = num_events - 1; i >= 0; i--) {
		struct event *ev = events + i;
		switch(ev->type) {
		case EV_T:
			if(ev->u.t.test)
				root = trace_tree_T(ev->u.t.name,
						    ev->u.t.value,
						    root,
						    trace_tree_E());
			else
				root = trace_tree_T(ev->u.t.name,
						    ev->u.t.value,
						    trace_tree_E(),
						    root);
			break;
		case EV_R:
			root = trace_tree_V(ev->u.r.name, ev->u.r.value, root);
			break;
		case EV_RE:
			root = trace_tree_D(ev->u.re.name, root);
			break;
		}
	}
	return root;
}

static void dump_tt(struct trace_tree_header *tree)
{
	struct trace_tree_V *tv;
	struct trace_tree_T *tt;
	struct trace_tree_D *td;
	int j;
	switch(tree->type) {
	case TT_E:
		fprintf(stderr, "(E)");
		break;
	case TT_V:
		tv = (struct trace_tree_V *) tree;
		fprintf(stderr, "(V %s ", tv->name);
		for(j = 0; j < tv->num_branches; j++) {
			fprintf(stderr, " ");
			dump_tt(tv->branches[j].tree);
		}
		fprintf(stderr, ")");
		break;
	case TT_T:
		tt = (struct trace_tree_T *) tree;
		fprintf(stderr, "(T %s ", tt->name);
		dump_tt(tt->t);
		fprintf(stderr, " ");
		dump_tt(tt->f);
		fprintf(stderr, ")");
		break;
	case TT_D:
		td = (struct trace_tree_D *) tree;
		fprintf(stderr, "(D %s ", td->name);
		dump_tt(td->t);
		fprintf(stderr, ")");
		break;
	case TT_L:
		fprintf(stderr, "(L)");
		break;
	}
}

static bool augment_tt(struct trace_tree_header **tree, struct trace *trace, struct action *a)
{
	int i, j;
	struct trace_tree_header **t = tree;
	int num_events = trace->num_events;
	int n1, n2;
	struct trace_tree_L *t_L;

	if((*t)->type == TT_E) {
		free(*t);
		*t = events_to_tree(trace->events, trace->num_events, a);
		return true;
	}

	for(i = 0; i < num_events; i++) {
		struct event *ev = trace->events + i;
		struct trace_tree_T *t_T;
		struct trace_tree_V *t_V;
		struct trace_tree_D *t_D;
		switch((*t)->type) {
		case TT_T:
			t_T = *(struct trace_tree_T **) t;
			/* ev->type == EV_T */
			assert(ev->type == EV_T);
			if(ev->u.t.test) {
				if(t_T->t->type == TT_E) {
					free(t_T->t);
					t_T->t = events_to_tree(ev + 1, num_events - i - 1, a);
					return true;
				} else {
					t = &(t_T->t);
				}
			} else {
				if(t_T->f->type == TT_E) {
					free(t_T->f);
					t_T->f = events_to_tree(ev + 1, num_events - i - 1, a);
					return true;
				} else {
					t = &(t_T->f);
				}
			}
			break;
		case TT_V:
			t_V = *(struct trace_tree_V **) t;
			/* ev->type == EV_R */
			assert(ev->type == EV_R);
			for(j = 0; j < t_V->num_branches; j++) {
				if(value_equ(t_V->branches[j].value, ev->u.r.value)) {
					t = &(t_V->branches[j].tree);
					break;
				}
			}
			if(j == t_V->num_branches) {
				t_V->num_branches++;
				*t = realloc(*t,
					     sizeof(struct trace_tree_V) +
					     t_V->num_branches * sizeof(struct trace_tree_Vb));
				t_V = *(struct trace_tree_V **) t;
				t_V->branches[j].value = ev->u.r.value;
				t_V->branches[j].tree = events_to_tree(ev + 1, num_events - i - 1, a);
				return true;
			}
			break;
		case TT_D:
			/* ev->type == EV_RE */
			assert(ev->type == EV_RE);
			t_D = *(struct trace_tree_D **) t;
			if(t_D->t->type == TT_E) {
				free(t_D->t);
				t_D->t = events_to_tree(ev + 1, num_events - i - 1, a);
				return true;
			} else {
				t = &(t_D->t);
			}
			break;
		case TT_L:
		case TT_E:
			break;
		}
	}
	assert((*t)->type == TT_L);
	t_L = *(struct trace_tree_L **) t;
	n1 = action_num_actions(t_L->ac);
	action_union(t_L->ac, a);
	n2 = action_num_actions(t_L->ac);
	return n1 != n2;
}

static bool invalidate_tt(struct trace_tree_header **tree, const char *name)
{
	int i;
	bool b, b1;
	struct trace_tree_header *t = *tree;
	struct trace_tree_V *tv;
	struct trace_tree_T *tt;
	struct trace_tree_D *td;

	switch(t->type) {
	case TT_E:
	case TT_L:
		return false;
	case TT_V:
		tv = (struct trace_tree_V *)t;
		b = false;
		for(i = 0; i < tv->num_branches; i++) {
			b1 = invalidate_tt(&(tv->branches[i].tree), name);
			b = b || b1;
		}
		return b;
	case TT_T:
		tt = (struct trace_tree_T *)t;
		b = invalidate_tt(&(tt->t), name);
		b1 = invalidate_tt(&(tt->f), name);
		return b || b1;
	case TT_D:
		td = (struct trace_tree_D *)t;
		if(strcmp(td->name, name) == 0) {
			trace_tree_free(td->t);
			td->t = trace_tree_E();
			return true;
		}
		return invalidate_tt(&(td->t), name);
	}
	abort();
}

static int __build_flow_table(struct xswitch *sw,
			      struct trace_tree_header *tree, struct match *ma, int priority,
			      struct action *ac_pi)
{
	int i;
	struct trace_tree_L *tl;
	struct trace_tree_V *tv;
	struct trace_tree_T *tt;
	struct msgbuf *msg;
	struct match *maa;
	char buf[128];
	switch(tree->type) {
	case TT_L:
		tl = (struct trace_tree_L *)tree;
		action_dump(tl->ac, buf, 128);
		fprintf(stderr, "%2d : _MATCH_, %s\n", priority, buf);
		msg = msg_flow_entry_add(sw->table0, priority, ma, tl->ac);
		xswitch_send(sw, msg);
		return priority + 1;
	case TT_V:
		tv = (struct trace_tree_V *)tree;
		for(i = 0; i < tv->num_branches; i++) {
			maa = match_copy(ma);
			match_add(maa,
				  flow_table_get_field_index(sw->table0, tv->name),
				  tv->branches[i].value,
				  value_from_64(0xffffffffffffffffull));
			priority = __build_flow_table(sw, tv->branches[i].tree, maa, priority, ac_pi);
			match_free(maa);
		}
		return priority;
	case TT_T:
		tt = (struct trace_tree_T *)tree;
		priority = __build_flow_table(sw, tt->f, ma, priority, ac_pi);
		maa = match_copy(ma);
		match_add(maa,
			  flow_table_get_field_index(sw->table0, tt->name),
			  tt->value,
			  value_from_64(0xffffffffffffffffull));
		action_dump(ac_pi, buf, 128);
		fprintf(stderr, "%2d : BARRIER, %s\n", priority, buf);
		msg = msg_flow_entry_add(sw->table0, priority, maa, ac_pi);
		xswitch_send(sw, msg);
		priority = __build_flow_table(sw, tt->t, maa, priority + 1, ac_pi);
		match_free(maa);
		return priority;
	case TT_E:
	case TT_D:
		return priority;
	}
}

static int build_flow_table(struct xswitch *sw,
			    struct trace_tree_header *tree, struct match *ma, int priority)
{
	int ret;
	struct action *ac_pi = action();
	action_add(ac_pi, AC_PACKET_IN, 0);
	ret = __build_flow_table(sw, tree, ma, priority, ac_pi);
	action_free(ac_pi);
	return ret;
}

struct route
{
	int num_edges;
	struct {
		dpid_t dpid1;
		int out_port;
		dpid_t dpid2;
		int in_port;
	} edges[32];
};

struct route *route(void)
{
	struct route *r = malloc(sizeof(struct route));
	r->num_edges = 0;
	return r;
}

void route_free(struct route *r)
{
	free(r);
}

void route_add_edge(struct route *r, dpid_t dpid1, int out_port, dpid_t dpid2, int in_port)
{
	int i = r->num_edges;
	if(i >= 32) {
		fprintf(stderr, "route: too many hops!\n");
		return;
	}
	r->edges[i].dpid1 = dpid1;
	r->edges[i].out_port = out_port;
	r->edges[i].dpid2 = dpid2;
	r->edges[i].in_port = in_port;
	r->num_edges++;
}

void route_union(struct route *r1, struct route *r2)
{
	int i, j;
	for(j = 0; j < r2->num_edges; j++) {
		for(i = 0; i < r1->num_edges; i++) {
			if(r1->edges[i].dpid1 == r2->edges[j].dpid1 &&
			   r1->edges[i].out_port == r2->edges[j].out_port &&
			   r1->edges[i].dpid2 == r2->edges[j].dpid2 &&
			   r1->edges[i].in_port == r2->edges[j].in_port)
				break;
		}
		if(i >= r1->num_edges)
			route_add_edge(r1,
				       r2->edges[j].dpid1,
				       r2->edges[j].out_port,
				       r2->edges[j].dpid2,
				       r2->edges[j].in_port);
	}
}

struct packet {
	struct packet_parser *pp;
};

void pull_header(struct packet *pkt)
{
	packet_parser_pull(pkt->pp);
}

const char *read_header_type(struct packet *pkt)
{
	return packet_parser_read_type(pkt->pp);
}

value_t read_packet(struct packet *pkt, const char *field)
{
	value_t v = packet_parser_read(pkt->pp, field);
	trace_R(field, v);
	return v;
}

bool test_equal(struct packet *pkt, const char *field, value_t value)
{
	value_t v = packet_parser_read(pkt->pp, field);
	bool result = value_equ(v, value);
	trace_T(field, value, result);
	return result;
}

void record(const char *name)
{
	trace_RE(name);
}

void invalidate(const char *name)
{
	trace_IE(name);
}

static int init_entry(struct xswitch *sw, int prio)
{
	struct match *ma;
	struct msgbuf *msg;
	struct action *ac;
	int idx;
	ma = match();
	idx = flow_table_get_field_index(sw->table0, "dl_type");
	if(idx >= 0)
		match_add(ma, idx, value_from_16(0x0800), value_from_16(0xffff));
	ac = action();
	action_add(ac, AC_PACKET_IN, 0);
	msg = msg_flow_entry_add(sw->table0, prio, ma, ac);
	match_free(ma);
	action_free(ac);
	xswitch_send(sw, msg);
	return prio + 1;
}

void maple_switch_up(struct xswitch *sw)
{
	struct msgbuf *msg;
	/* init trace tree */
	sw->trace_tree = trace_tree_E();

	/* init match fields */
	sw->table0 = flow_table(0, FLOW_TABLE_TYPE_MM, 10);
	flow_table_add_field(sw->table0, "in_port", MATCH_FIELD_METADATA, 16, 8);
	flow_table_add_field(sw->table0, "dl_dst", MATCH_FIELD_PACKET, 0, 48);
	flow_table_add_field(sw->table0, "dl_src", MATCH_FIELD_PACKET, 48, 48);
	flow_table_add_field(sw->table0, "dl_type", MATCH_FIELD_PACKET, 96, 16);
	/* match_field 不够用了，暂时不要nw_proto */
	/* flow_table_add_field(sw->table0, "nw_proto", MATCH_FIELD_PACKET, 112+64+8, 8); */
	flow_table_add_field(sw->table0, "nw_src", MATCH_FIELD_PACKET, 112+96, 32);
	flow_table_add_field(sw->table0, "nw_dst", MATCH_FIELD_PACKET, 112+128, 32);
	flow_table_add_field(sw->table0, "tp_src", MATCH_FIELD_PACKET, 112+160, 16);
	flow_table_add_field(sw->table0, "tp_dst", MATCH_FIELD_PACKET, 112+176, 16);

	/* create table */
	msg = msg_flow_table_add(sw->table0);
	xswitch_send(sw, msg);
	sw->hack_start_prio = 0;

	/* init entry */
	sw->hack_start_prio = init_entry(sw, sw->hack_start_prio);
}

struct route *f(struct packet *pk);

void maple_packet_in(struct xswitch *sw, int in_port, const uint8_t *packet, int packet_len)
{
	int i;
	struct route *r;
	struct packet pk;

	/* init */
	trace_clear();

	/* run */
	pk.pp = packet_parser(packet, packet_len);
	r = f(&pk);
	packet_parser_free(pk.pp);

	/* learn */
	for(i = 0; i < r->num_edges; i++) {
		dpid_t dpid = r->edges[i].dpid1;
		int out_port = r->edges[i].out_port;
		fprintf(stderr, "handle edge (0x%x, %d, 0x%x, %d):\n",
			r->edges[i].dpid1,
			r->edges[i].out_port,
			r->edges[i].dpid2,
			r->edges[i].in_port);
		if(dpid) {
			struct entity *cur_e = topo_get_switch(dpid);
			struct xswitch *cur_sw = entity_get_xswitch(cur_e);
			struct action *a = action();
			int j;

			action_add(a, AC_OUTPUT, out_port);

			if(cur_sw == NULL)
				abort();

			if(r->edges[i].dpid2 == 0) {
				struct msgbuf *mb;
				mb = msg_packet_out(0, packet, packet_len, a);
				xswitch_send(cur_sw, mb);
			}

			for(j = 0; j < r->num_edges; j++) {
				if(r->edges[j].dpid2 == dpid)
					break;
			}
			assert(j < r->num_edges);
			trace_mod_in_port(r->edges[j].in_port);

			if(augment_tt(&(cur_sw->trace_tree), &trace, a)) {
				struct match *ma = match();
				match_add(ma,
					  flow_table_get_field_index(cur_sw->table0, "dl_type"),
					  value_from_16(0x0800),
					  value_from_16(0xffff));

				cur_sw->hack_start_prio =
					init_entry(cur_sw, cur_sw->hack_start_prio);

				fprintf(stderr, "---- flow table for 0x%x ---\n", dpid);
				dump_tt(cur_sw->trace_tree);
				fprintf(stderr, "\n");
				cur_sw->hack_start_prio =
					build_flow_table(cur_sw, cur_sw->trace_tree, ma,
							 cur_sw->hack_start_prio) + 1;
				match_free(ma);
			}
			action_free(a);
		}
	}
	route_free(r);

	/* invalidate */
	int j, num_switches;
	struct entity **switches = topo_get_switches(&num_switches);
	for(j = 0; j < trace.num_inv_events; j++) {
		const char *name;
		name = trace.inv_events[j].name;
		fprintf(stderr, "invalidate \"%s\":\n", name);
		for(i = 0; i < num_switches; i++) {
			struct xswitch *cur_sw = entity_get_xswitch(switches[i]);
			struct trace_tree_header *tt = cur_sw->trace_tree;
			if(invalidate_tt(&tt, name)) {
				struct match *ma = match();
				match_add(ma,
					  flow_table_get_field_index(cur_sw->table0, "dl_type"),
					  value_from_16(0x0800),
					  value_from_16(0xffff));

				cur_sw->hack_start_prio =
					init_entry(cur_sw, cur_sw->hack_start_prio);

				fprintf(stderr, "---- flow table for 0x%x ---\n", cur_sw->dpid);
				cur_sw->hack_start_prio =
					build_flow_table(cur_sw, cur_sw->trace_tree, ma,
							 cur_sw->hack_start_prio) + 1;
				match_free(ma);
			}
		}
	}
}

void maple_switch_down(struct xswitch *sw)
{
	flow_table_free(sw->table0);
	trace_tree_free(sw->trace_tree);
}
