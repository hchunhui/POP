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

void trace_DF(int offb, int lenb)
{
	int i = trace.num_mod_events;
	trace.mod_events[i].type = MEV_DF;
	trace.mod_events[i].u.df.offb = offb;
	trace.mod_events[i].u.df.lenb = lenb;
	trace.num_mod_events++;
}

void trace_AF(int offb, int lenb, value_t value)
{
	int i = trace.num_mod_events;
	trace.mod_events[i].type = MEV_AF;
	trace.mod_events[i].u.af.offb = offb;
	trace.mod_events[i].u.af.lenb = lenb;
	trace.mod_events[i].u.af.value = value;
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


#include "xswitch/xswitch.h"
#include "core/packet_parser.h"

bool trace_generate_action(struct trace *trace, struct action *ac_core, struct action *ac_edge)
{
	/* XXX:
	 * ac_edge: prelude for edge switches
	 * ac_core: prelude for core switches
	 * eq_edge_core: true if ac_edge == ac_core
	 */
	int i;
	bool eq_edge_core;

	eq_edge_core = true;
	for(i = 0; i < trace->num_mod_events; i++) {
		int offset, length;
		struct header *cur_spec;
		int j, hlen;
		const char *checksum_field;

		switch(trace->mod_events[i].type) {
		case MEV_P:
			cur_spec = trace->mod_events[i].u.p.new_spec;
			if(ac_edge)
				expr_generate_action_backward(
					header_get_length(cur_spec),
					trace->mod_events[i].u.p.stack_base, ac_edge);
			if(ac_core)
				expr_generate_action_backward(
					header_get_length(cur_spec),
					trace->mod_events[i].u.p.stack_base, ac_core);
			break;
		case MEV_M:
			if(ac_edge) {
				cur_spec = trace->mod_events[i].u.m.spec;
				header_get_field(cur_spec,
						 trace->mod_events[i].u.m.name,
						 &offset,
						 &length);
				action_add_set_field(ac_edge, offset, length,
						     trace->mod_events[i].u.m.value);
				checksum_field = header_get_sum(cur_spec);
				if(checksum_field) {
					header_get_field(cur_spec,
							 checksum_field,
							 &offset,
							 &length);
					/* XXX: hack
					 * Variable length checksum is not supported by POF-1.x.
					 */
					hlen = header_get_fixed_length(cur_spec);
					action_add_checksum(ac_edge, offset, length, 0, hlen);
				}
			}
			eq_edge_core = false;
			break;
		case MEV_A:
			if(ac_edge) {
				hlen = trace->mod_events[i].u.a.hlen;
				for(j = 0; j < hlen / 16; j++)
					action_add_add_field(ac_edge, 0, 16 * 8, value_from_8(0));
				if(hlen % 16)
					action_add_add_field(ac_edge, 0, (hlen%16) * 8, value_from_8(0));
			}
			eq_edge_core = false;
			break;
		case MEV_AF:
			if(ac_edge)
				action_add_add_field(ac_edge,
						     trace->mod_events[i].u.af.offb,
						     trace->mod_events[i].u.af.lenb,
						     trace->mod_events[i].u.af.value);
			eq_edge_core = false;
			break;
		case MEV_DF:
			if(ac_edge)
				action_add_del_field(ac_edge,
						     trace->mod_events[i].u.df.offb,
						     trace->mod_events[i].u.df.lenb);
			eq_edge_core = false;
			break;
		}
	}
	return eq_edge_core;
}
