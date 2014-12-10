#include "trace.h"

static __thread struct trace trace = { 0 };

void trace_R(const char *name, value_t value)
{
	int i = trace.num_events;
	trace.events[i].type = EV_R;
	strncpy(trace.events[i].u.r.name, name, 32);
	trace.events[i].u.r.name[31] = 0;
	trace.events[i].u.r.value = value;
	trace.num_events++;
}

void trace_T(const char *name, value_t value, bool result)
{
	int i = trace.num_events;
	trace.events[i].type = EV_T;
	strncpy(trace.events[i].u.t.name, name, 32);
	trace.events[i].u.t.name[31] = 0;
	trace.events[i].u.t.value = value;
	trace.events[i].u.t.test = result;
	trace.num_events++;
}

void trace_RE(const char *name, void *arg)
{
	int i = trace.num_events;
	trace.events[i].type = EV_RE;
	strncpy(trace.events[i].u.re.name, name, 32);
	trace.events[i].u.re.name[31] = 0;
	trace.events[i].u.re.arg = arg;
	trace.num_events++;
}

void trace_IE(const char *name)
{
	int i = trace.num_inv_events;
	strncpy(trace.inv_events[i].name, name, 32);
	trace.inv_events[i].name[31] = 0;
	trace.num_inv_events++;
}

void trace_G(struct header *spec, struct expr *move_expr)
{
	int i = trace.num_events;
	trace.events[i].type = EV_G;
	trace.events[i].u.g.spec = spec;
	trace.events[i].u.g.move_expr = move_expr;
	trace.num_events++;
}

void trace_clear(void)
{
	trace.num_events = 0;
	trace.num_inv_events = 0;
}

struct trace *trace_get(void)
{
	return &trace;
}
