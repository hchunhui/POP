#ifndef _TRACE_TREE_H_
#define _TRACE_TREE_H_

#include <stdbool.h>

struct trace_tree;
struct trace;
struct action;
struct xswitch;
struct flow_table;
struct trace_tree *trace_tree(void);
void trace_tree_free(struct trace_tree *t);
void trace_tree_print(struct trace_tree *tree);
#ifdef ENABLE_WEB
void trace_tree_print_json(struct trace_tree *tree, dpid_t dpid);
void trace_tree_print_ft_json(struct trace_tree *tree, dpid_t dpid);
#endif
bool trace_tree_augment(struct trace_tree **tree, struct trace *trace, struct action *a);
bool trace_tree_invalidate(struct trace_tree **tree, struct xswitch *sw, struct flow_table *ft,
			   bool (*p)(void *p_data, const char *name, const void *arg), void *p_data);
void trace_tree_emit_rule(struct xswitch *sw, struct trace_tree *tree);

#endif /* _TRACE_TREE_H_ */
