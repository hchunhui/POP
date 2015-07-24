#ifndef _TRACE_H_
#define _TRACE_H_

#include "types.h"

struct header;

enum event_type { EV_R, EV_T, EV_RE, EV_G };
enum mod_event_type { MEV_P, MEV_M, MEV_A };

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
			const void *arg;
		} re;
		struct {
			struct header *old_spec;
			struct header *new_spec;
			int stack_base;
		} g;
	} u;
};

struct mod_event
{
	enum mod_event_type type;
	union {
		struct {
			struct header *new_spec;
			int stack_base;
		} p;
		struct {
			char name[32];
			value_t value;
			struct header *spec;
		} m;
		struct {
			int hlen;
		} a;
	} u;
};

struct trace
{
	int num_events;
	struct event events[1024];
	int num_inv_events;
	struct {
		char name[32];
		const void *arg;
	} inv_events[1024];
	int num_mod_events;
	struct mod_event mod_events[1024];
};

void trace_R(const char *name, value_t value);
void trace_T(const char *name, value_t value, bool result);
void trace_RE(const char *name, const void *arg);
void trace_IE(const char *name, const void *arg);
void trace_G(struct header *old_spec, struct header *new_spec, int stack_base);
void trace_P(struct header *new_spec, int stack_base);
void trace_M(const char *name, value_t value, struct header *spec);
void trace_A(int hlen);
void trace_clear(void);
struct trace *trace_get(void);

#endif /* _TRACE_H_ */
