#include <string.h>
#include "trace.h"

static __thread struct trace trace;

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

void trace_RE(const char *name, const void *arg)
{
	int i = trace.num_events;
	trace.events[i].type = EV_RE;
	strncpy(trace.events[i].u.re.name, name, 32);
	trace.events[i].u.re.name[31] = 0;
	trace.events[i].u.re.arg = arg;
	trace.num_events++;
}

void trace_IE(const char *name, const void *arg)
{
	int i = trace.num_inv_events;
	strncpy(trace.inv_events[i].name, name, 32);
	trace.inv_events[i].name[31] = 0;
	trace.inv_events[i].arg = arg;
	trace.num_inv_events++;
}

void trace_G(struct header *old_spec, struct header *new_spec, int stack_base)
{
	int i = trace.num_events;
	trace.events[i].type = EV_G;
	trace.events[i].u.g.old_spec = old_spec;
	trace.events[i].u.g.new_spec = new_spec;
	trace.events[i].u.g.stack_base = stack_base;
	trace.num_events++;
}

void trace_P(struct header *new_spec, int stack_base)
{
	int i = trace.num_mod_events;
	trace.mod_events[i].type = MEV_P;
	trace.mod_events[i].u.p.new_spec = new_spec;
	trace.mod_events[i].u.p.stack_base = stack_base;
	trace.num_mod_events++;
}

void trace_M(const char *name, value_t value, struct header *spec)
{
	int i = trace.num_mod_events;
	trace.mod_events[i].type = MEV_M;
	strncpy(trace.mod_events[i].u.m.name, name, 32);
	trace.mod_events[i].u.m.name[31] = 0;
	trace.mod_events[i].u.m.value = value;
	trace.mod_events[i].u.m.spec = spec;
	trace.num_mod_events++;
}

void trace_A(int hlen)
{
	int i = trace.num_mod_events;
	trace.mod_events[i].type = MEV_A;
	trace.mod_events[i].u.a.hlen = hlen;
	trace.num_mod_events++;
}

void trace_clear(void)
{
	trace.num_events = 0;
	trace.num_inv_events = 0;
	trace.num_mod_events = 0;
}

struct trace *trace_get(void)
{
	return &trace;
}
