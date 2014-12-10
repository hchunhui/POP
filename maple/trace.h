#ifndef _TRACE_H_
#define _TRACE_H_

#include "types.h"

struct header;
struct expr;

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
			void *arg;
		} re;
		struct {
			struct header *spec;
			struct expr *move_expr;
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

void trace_R(const char *name, value_t value);
void trace_T(const char *name, value_t value, bool result);
void trace_RE(const char *name, void *arg);
void trace_IE(const char *name);
void trace_G(struct header *spec, struct expr *move_expr);
void trace_clear(void);
struct trace *trace_get(void);

#endif /* _TRACE_H_ */
