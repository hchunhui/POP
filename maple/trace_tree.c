#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "types.h"
#include "trace.h"
#include "trace_tree.h"
#include "packet_parser.h"
#include "xswitch/xswitch-private.h"

/* data type */
enum trace_tree_type { TT_E, TT_L, TT_V, TT_T, TT_D, TT_G };
struct trace_tree
{
	enum trace_tree_type type;
};

struct trace_tree_E
{
	struct trace_tree h;
};

struct trace_tree_L
{
	struct trace_tree h;
	int index;
	struct action *ac;
};

struct trace_tree_Vb {
	value_t value;
	struct trace_tree *tree;
};

struct trace_tree_V
{
	struct trace_tree h;
	char name[32];
	int num_branches;
	struct trace_tree_Vb branches[0];
};

struct trace_tree_T
{
	struct trace_tree h;
	char name[32];
	value_t value;
	int barrier_index;
	struct trace_tree *t, *f;
};

struct trace_tree_D
{
	struct trace_tree h;
	char name[32];
	void *arg;
	struct trace_tree *t;
};

struct trace_tree_G
{
	struct trace_tree h;
	int index;
	struct flow_table *ft;
	struct header *spec;
	struct expr *move_expr;
	int hack_start_prio;
	struct trace_tree *t;
};

/* constructors */
static struct trace_tree *trace_tree_E(void)
{
	struct trace_tree_E *t = malloc(sizeof *t);
	t->h.type = TT_E;
	return (struct trace_tree *)t;
}

static struct trace_tree *trace_tree_L(struct action *a)
{
	struct trace_tree_L *t = malloc(sizeof *t);
	t->h.type = TT_L;
	t->ac = action_copy(a);
	t->index = -1;
	return (struct trace_tree *)t;
}

static struct trace_tree *trace_tree_V(const char *name,
				       value_t v,
				       struct trace_tree *p)
{
	struct trace_tree_V *t = malloc(sizeof *t + sizeof (struct trace_tree_Vb));
	t->h.type = TT_V;
	strncpy(t->name, name, 32);
	t->name[31] = 0;
	t->num_branches = 1;
	t->branches[0].value = v;
	t->branches[0].tree = p;
	return (struct trace_tree *)t;
}

static struct trace_tree *trace_tree_T(const char *name,
				       value_t v,
				       struct trace_tree *tt,
				       struct trace_tree *tf)
{
	struct trace_tree_T *t = malloc(sizeof *t);
	t->h.type = TT_T;
	strncpy(t->name, name, 32);
	t->name[31] = 0;
	t->value = v;
	t->t = tt;
	t->f = tf;
	t->barrier_index = -1;
	return (struct trace_tree *)t;
}

static struct trace_tree *trace_tree_D(const char *name,
				       void *arg,
				       struct trace_tree *tt)
{
	struct trace_tree_D *t = malloc(sizeof *t);
	t->h.type = TT_D;
	strncpy(t->name, name, 32);
	t->name[31] = 0;
	t->arg = arg;
	t->t = tt;
	return (struct trace_tree *)t;
}

static struct trace_tree *trace_tree_G(struct header *spec,
				       struct expr *move_expr,
				       struct trace_tree *tt)
{
	struct trace_tree_G *t = malloc(sizeof *t);
	t->h.type = TT_G;
	t->ft = NULL;
	t->spec = spec;
	t->move_expr = move_expr;
	t->hack_start_prio = 0;
	t->t = tt;
	t->index = -1;
	return (struct trace_tree *)t;
}

struct trace_tree *trace_tree(void)
{
	return trace_tree_E();
}

/* destructor */
void trace_tree_free(struct trace_tree *t)
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

/* printer */
void trace_tree_print(struct trace_tree *tree)
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
			trace_tree_print(tv->branches[j].tree);
		}
		fprintf(stderr, ")");
		break;
	case TT_T:
		tt = (struct trace_tree_T *) tree;
		fprintf(stderr, "(T %s ", tt->name);
		trace_tree_print(tt->t);
		fprintf(stderr, " ");
		trace_tree_print(tt->f);
		fprintf(stderr, ")");
		break;
	case TT_D:
		td = (struct trace_tree_D *) tree;
		fprintf(stderr, "(D %s ", td->name);
		trace_tree_print(td->t);
		fprintf(stderr, ")");
		break;
	case TT_G:
		tg = (struct trace_tree_G *) tree;
		fprintf(stderr, "(G %s ", header_get_name(tg->spec));
		trace_tree_print(tg->t);
		fprintf(stderr, ")");
		break;
	case TT_L:
		fprintf(stderr, "(L)");
		break;
	}
}

/* algorithm */
static struct trace_tree *events_to_tree(struct event *events, int num_events,
					 struct action *a)
{
	int i;
	struct trace_tree *root;
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
			root = trace_tree_D(ev->u.re.name, ev->u.re.arg, root);
			break;
		case EV_G:
			root = trace_tree_G(ev->u.g.spec, ev->u.g.move_expr, root);
			break;
		}
	}
	return root;
}

bool trace_tree_augment(struct trace_tree **tree, struct trace *trace, struct action *a)
{
	int i, j;
	struct trace_tree **t = tree;
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

static void revoke_rule(struct xswitch *sw, struct flow_table *ft, struct trace_tree *tree)
{
	int i;
	struct trace_tree_L *tl;
	struct trace_tree_V *tv;
	struct trace_tree_T *tt;
	struct trace_tree_D *td;
	struct trace_tree_G *tg;

	struct msgbuf *msg;
	switch(tree->type) {
	case TT_L:
		tl = (struct trace_tree_L *)tree;
		msg = msg_flow_entry_del(ft, tl->index);
		xswitch_send(sw, msg);
		flow_table_put_entry_index(ft, tl->index);
		return;
	case TT_V:
		tv = (struct trace_tree_V *)tree;
		for(i = 0; i < tv->num_branches; i++) {
			revoke_rule(sw, ft, tv->branches[i].tree);
		}
		return;
	case TT_T:
		tt = (struct trace_tree_T *)tree;
		revoke_rule(sw, ft, tt->f);
		msg = msg_flow_entry_del(ft, tt->barrier_index);
		xswitch_send(sw, msg);
		flow_table_put_entry_index(ft, tt->barrier_index);
		revoke_rule(sw, ft, tt->t);
		return;
	case TT_G:
		tg = (struct trace_tree_G *)tree;
		msg = msg_flow_entry_del(ft, tg->index);
		xswitch_send(sw, msg);
		flow_table_put_entry_index(ft, tg->index);
		revoke_rule(sw, tg->ft, tg->t);
		return;
	case TT_D:
		td = (struct trace_tree_D *)tree;
		revoke_rule(sw, ft, td->t);
		return;
	case TT_E:
		return;
	}
	assert(0);
}

bool trace_tree_invalidate(struct trace_tree **tree, struct xswitch *sw, struct flow_table *ft,
			   bool (*p)(void *p_data, const char *name, void *arg), void *p_data)
{
	int i;
	bool b, b1;
	struct trace_tree *t = *tree;
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
			b1 = trace_tree_invalidate(&(tv->branches[i].tree), sw, ft, p, p_data);
			b = b || b1;
		}
		return b;
	case TT_T:
		tt = (struct trace_tree_T *)t;
		b = trace_tree_invalidate(&(tt->t), sw, ft, p, p_data);
		b1 = trace_tree_invalidate(&(tt->f), sw, ft, p, p_data);
		return b || b1;
	case TT_D:
		td = (struct trace_tree_D *)t;
		if(p(p_data, td->name, td->arg)) {
			revoke_rule(sw, ft, td->t);
			trace_tree_free(td->t);
			td->t = trace_tree_E();
			return true;
		}
		return trace_tree_invalidate(&(td->t), sw, ft, p, p_data);
	case TT_G:
		tg = (struct trace_tree_G *)t;
		return trace_tree_invalidate(&(tg->t), sw, tg->ft, p, p_data);
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
	msg = msg_flow_entry_add(ft, flow_table_get_entry_index(ft), *hack_start_prio, ma, ac);
	(*hack_start_prio)++;
	match_free(ma);
	action_free(ac);
	xswitch_send(sw, msg);
}

static int emit_rule(struct xswitch *sw, struct flow_table *ft,
		     struct trace_tree *tree, struct match *ma, int priority,
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
		tl->index = flow_table_get_entry_index(ft);
		msg = msg_flow_entry_add(ft, tl->index, priority, ma, tl->ac);
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
			priority = emit_rule(sw, ft, tv->branches[i].tree, maa, priority, ac_pi);
			match_free(maa);
		}
		return priority;
	case TT_T:
		tt = (struct trace_tree_T *)tree;
		priority = emit_rule(sw, ft, tt->f, ma, priority, ac_pi);
		maa = match_copy(ma);
		match_add(maa,
			  tt->name,
			  tt->value,
			  value_from_64(0xffffffffffffffffull));
		action_dump(ac_pi, buf, 128);
		fprintf(stderr, "tid %d: %2d, BARRIER, %s\n",
			flow_table_get_tid(ft), priority, buf);
		tt->barrier_index = flow_table_get_entry_index(ft);
		msg = msg_flow_entry_add(ft, tt->barrier_index, priority, maa, ac_pi);
		xswitch_send(sw, msg);
		priority = emit_rule(sw, ft, tt->t, maa, priority + 1, ac_pi);
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
		expr_generate_action(tg->move_expr, tg->ft, a);

		match_dump(ma, buf, 128);
		action_dump(a, buf2, 128);
		fprintf(stderr, "tid %d: %2d, %s, %s\n",
			flow_table_get_tid(ft), priority, buf, buf2);

		tg->index = flow_table_get_entry_index(ft);
		msg = msg_flow_entry_add(ft, tg->index, priority, ma, a);
		xswitch_send(sw, msg);
		action_free(a);

		init_entry(sw, tg->ft, (&tg->hack_start_prio));
		maa = match();
		tg->hack_start_prio =
			emit_rule(sw, tg->ft, tg->t, maa, tg->hack_start_prio, ac_pi);
		match_free(maa);
		return priority + 1;
	case TT_D:
		td = (struct trace_tree_D *)tree;
		return emit_rule(sw, ft, td->t, ma, priority, ac_pi);
	case TT_E:
		return priority;
	}
	assert(0);
}

void trace_tree_emit_rule(struct xswitch *sw, struct trace_tree *tree)
{
	struct match *ma = match();
	struct action *ac_pi = action();
	action_add(ac_pi, AC_PACKET_IN, 0);
	init_entry(sw, sw->table0, &(sw->hack_start_prio));
	sw->hack_start_prio =
		emit_rule(sw, sw->table0, tree, ma, sw->hack_start_prio, ac_pi) + 1;
	action_free(ac_pi);
	match_free(ma);
}
