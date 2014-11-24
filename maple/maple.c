#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "xswitch/xswitch-private.h"
#include "topo/topo.h"
#include "entity.h"
#include "route.h"

#include "maple.h"
#include "packet_parser.h"
#include "spec_parser.h"

#include "maple_api.h"

static struct header *header_spec;

enum event_type { EV_R, EV_T, EV_RE, EV_G };

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
		struct {
			struct header *spec;
			int move_length;
		} g;
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

static void trace_G(struct header *spec, int move_length)
{
	int i = trace.num_events;
	trace.events[i].type = EV_G;
	trace.events[i].u.g.spec = spec;
	trace.events[i].u.g.move_length = move_length;
	trace.num_events++;
}

static void trace_clear(void)
{
	trace.num_events = 0;
	trace.num_inv_events = 0;
}

enum trace_tree_type { TT_E, TT_L, TT_V, TT_T, TT_D, TT_G };
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

struct trace_tree_G
{
	struct trace_tree_header h;
	struct flow_table *ft;
	struct header *spec;
	int move_length;
	int hack_start_prio;
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

static struct trace_tree_header *trace_tree_G(struct header *spec,
					      int move_length,
					      struct trace_tree_header *tt)
{
	struct trace_tree_G *t = malloc(sizeof *t);
	t->h.type = TT_G;
	t->ft = NULL;
	t->spec = spec;
	t->move_length = move_length;
	t->hack_start_prio = 0;
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
	struct trace_tree_G *tg;
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
	case TT_G:
		tg = (struct trace_tree_G *)t;
		trace_tree_free(tg->t);
		if(tg->ft)
			flow_table_free(tg->ft);
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
		case EV_G:
			root = trace_tree_G(ev->u.g.spec, ev->u.g.move_length, root);
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
	struct trace_tree_G *tg;
	int j;
	switch(tree->type) {
	case TT_E:
		fprintf(stderr, "(E)");
		break;
	case TT_V:
		tv = (struct trace_tree_V *) tree;
		fprintf(stderr, "(V %s", tv->name);
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
	case TT_G:
		tg = (struct trace_tree_G *) tree;
		fprintf(stderr, "(G %s ", header_get_name(tg->spec));
		dump_tt(tg->t);
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
		struct trace_tree_G *t_G;
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
				if(value_equal(t_V->branches[j].value, ev->u.r.value)) {
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
		case TT_G:
			/* ev->type == EV_G */
			assert(ev->type == EV_G);
			t_G = *(struct trace_tree_G **) t;
			if(t_G->t->type == TT_E) {
				free(t_G->t);
				t_G->t = events_to_tree(ev + 1, num_events - i - 1, a);
				return true;
			} else {
				t = &(t_G->t);
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
	struct trace_tree_G *tg;

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
	case TT_G:
		tg = (struct trace_tree_G *)t;
		return invalidate_tt(&(tg->t), name);
	}
	assert(0);
}

static void init_entry(struct xswitch *sw, struct flow_table *ft, int *hack_start_prio)
{
	struct match *ma;
	struct msgbuf *msg;
	struct action *ac;
	ma = match();
	ac = action();
	action_add(ac, AC_PACKET_IN, 0);
	msg = msg_flow_entry_add(ft, *hack_start_prio, ma, ac);
	(*hack_start_prio)++;
	match_free(ma);
	action_free(ac);
	xswitch_send(sw, msg);
}

static int __build_flow_table(struct xswitch *sw, struct flow_table *ft,
			      struct trace_tree_header *tree, struct match *ma, int priority,
			      struct action *ac_pi)
{
	int i;
	struct trace_tree_L *tl;
	struct trace_tree_V *tv;
	struct trace_tree_T *tt;
	struct trace_tree_D *td;
	struct trace_tree_G *tg;

	struct msgbuf *msg;
	struct match *maa;
	struct action *a;
	char buf[128], buf2[128];
	switch(tree->type) {
	case TT_L:
		tl = (struct trace_tree_L *)tree;
		match_dump(ma, buf, 128);
		action_dump(tl->ac, buf2, 128);
		fprintf(stderr, "tid %d: %2d, %s, %s\n",
			flow_table_get_tid(ft), priority, buf, buf2);
		msg = msg_flow_entry_add(ft, priority, ma, tl->ac);
		xswitch_send(sw, msg);
		return priority + 1;
	case TT_V:
		tv = (struct trace_tree_V *)tree;
		for(i = 0; i < tv->num_branches; i++) {
			maa = match_copy(ma);
			match_add(maa,
				  tv->name,
				  tv->branches[i].value,
				  value_from_64(0xffffffffffffffffull));
			priority = __build_flow_table(sw, ft, tv->branches[i].tree, maa, priority, ac_pi);
			match_free(maa);
		}
		return priority;
	case TT_T:
		tt = (struct trace_tree_T *)tree;
		priority = __build_flow_table(sw, ft, tt->f, ma, priority, ac_pi);
		maa = match_copy(ma);
		match_add(maa,
			  tt->name,
			  tt->value,
			  value_from_64(0xffffffffffffffffull));
		action_dump(ac_pi, buf, 128);
		fprintf(stderr, "tid %d: %2d, BARRIER, %s\n",
			flow_table_get_tid(ft), priority, buf);
		msg = msg_flow_entry_add(ft, priority, maa, ac_pi);
		xswitch_send(sw, msg);
		priority = __build_flow_table(sw, ft, tt->t, maa, priority + 1, ac_pi);
		match_free(maa);
		return priority;
	case TT_G:
		tg = (struct trace_tree_G *)tree;
		if(tg->ft == NULL) {
			int tid = sw->next_table_id++;
			// add a new table
			tg->ft = header_make_flow_table(tg->spec, tid);
			msg = msg_flow_table_add(tg->ft);
			xswitch_send(sw, msg);
		}
		// insert GOTO_TABLE into orig table
		a = action();
		action_add2(a, AC_GOTO_TABLE, flow_table_get_tid(tg->ft), tg->move_length);

		match_dump(ma, buf, 128);
		action_dump(a, buf2, 128);
		fprintf(stderr, "tid %d: %2d, %s, %s\n",
			flow_table_get_tid(ft), priority, buf, buf2);

		msg = msg_flow_entry_add(ft, priority, ma, a);
		xswitch_send(sw, msg);
		action_free(a);

		init_entry(sw, tg->ft, (&tg->hack_start_prio));
		maa = match();
		tg->hack_start_prio =
			__build_flow_table(sw, tg->ft, tg->t, maa, tg->hack_start_prio, ac_pi);
		match_free(maa);
		return priority + 1;
	case TT_D:
		td = (struct trace_tree_D *)tree;
		return __build_flow_table(sw, ft, td->t, ma, priority, ac_pi);
	case TT_E:
		return priority;
	}
	assert(0);
}

static void build_flow_table(struct xswitch *sw, struct trace_tree_header *tree)
{
	struct match *ma = match();
	struct action *ac_pi = action();
	action_add(ac_pi, AC_PACKET_IN, 0);
	sw->hack_start_prio =
		__build_flow_table(sw, sw->table0, tree, ma, sw->hack_start_prio, ac_pi) + 1;
	action_free(ac_pi);
	match_free(ma);
}

struct packet {
	struct packet_parser *pp;
};

void pull_header(struct packet *pkt)
{
	value_t sel_value;
	struct header *old_spec, *new_spec;
	/* XXX: hack */
	packet_parser_pull(pkt->pp, &old_spec, &sel_value, &new_spec);
	trace_R(header_get_sel(old_spec), sel_value);
	trace_G(new_spec, header_get_length(old_spec));
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
	bool result = value_equal(v, value);
	trace_T(field, value, result);
	return result;
}

const uint8_t *read_payload(struct packet *pkt, int *length)
{
	return packet_parser_get_payload(pkt->pp, length);
}

void record(const char *name)
{
	trace_RE(name);
}

void invalidate(const char *name)
{
	trace_IE(name);
}

struct entity **get_hosts(int *pnum)
{
	return topo_get_hosts(pnum);
}

struct entity **get_switches(int *pnum)
{
	return topo_get_switches(pnum);
}

struct entity *get_switch(dpid_t dpid)
{
	return topo_get_switch(dpid);
}

struct entity *get_host_by_haddr(haddr_t addr)
{
	return topo_get_host_by_haddr(addr);
}

struct entity *get_host_by_paddr(uint32_t addr)
{
	return topo_get_host_by_paddr(addr);
}

void maple_init(void)
{
	fprintf(stderr, "loading header spec...\n");
	header_spec = spec_parser_file("scripts/header.spec");
	assert(header_spec);
}

void maple_switch_up(struct xswitch *sw)
{
	/* init trace tree */
	sw->trace_tree = trace_tree_E();
}

struct route *f(struct packet *pk);

static void mod_in_port(int in_port)
{
	int i = trace.num_events - 1;
	assert(i >= 0);
	assert(trace.events[i].type == EV_R);
	assert(strcmp(trace.events[i].u.r.name, "in_port") == 0);
	trace.events[i].u.r.value = value_from_8(in_port);
}

void maple_packet_in(struct xswitch *sw, int in_port, const uint8_t *packet, int packet_len)
{
	int i;
	struct route *r;
	struct packet pk;
	edge_t *edges;
	int num_edges;

	/* init */
	trace_clear();

	/* run */
	pk.pp = packet_parser(header_spec, packet, packet_len);
	trace_G(header_spec, 0);

	r = f(&pk);

	trace_R("in_port", value_from_8(0));
	packet_parser_free(pk.pp);

	/* learn */
	edges = route_get_edges(r, &num_edges);
	for(i = 0; i < num_edges; i++) {
		struct entity *cur_ent = edges[i].ent1;
		int out_port = edges[i].port1;
		fprintf(stderr, "handle edge (0x%x, %d, 0x%x, %d):\n",
			entity_get_dpid(edges[i].ent1),
			edges[i].port1,
			entity_get_dpid(edges[i].ent2),
			edges[i].port2);
		if(cur_ent) {
			struct xswitch *cur_sw = entity_get_xswitch(cur_ent);
			struct action *a = action();
			int j;

			action_add(a, AC_OUTPUT, out_port);

			assert(cur_sw);

			if(!edges[i].ent2) {
				struct msgbuf *mb;
				mb = msg_packet_out(0, packet, packet_len, a);
				xswitch_send(cur_sw, mb);
			}

			for(j = 0; j < num_edges; j++) {
				if(edges[j].ent2 == cur_ent)
					break;
			}
			assert(j < num_edges);
			mod_in_port(edges[j].port2);

			if(augment_tt(&(cur_sw->trace_tree), &trace, a)) {
				fprintf(stderr, "--- flow table for 0x%x ---\n", entity_get_dpid(cur_ent));
				dump_tt(cur_sw->trace_tree);
				fprintf(stderr, "\n");
				init_entry(cur_sw, cur_sw->table0, &(cur_sw->hack_start_prio));
				build_flow_table(cur_sw, cur_sw->trace_tree);
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
				init_entry(cur_sw, cur_sw->table0, &(cur_sw->hack_start_prio));
				fprintf(stderr, "---- flow table for 0x%x ---\n", cur_sw->dpid);
				build_flow_table(cur_sw, cur_sw->trace_tree);
			}
		}
	}
}

void maple_switch_down(struct xswitch *sw)
{
	flow_table_free(sw->table0);
	trace_tree_free(sw->trace_tree);
}
