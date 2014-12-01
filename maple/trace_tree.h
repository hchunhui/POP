#ifndef _TRACE_TREE_H_
#define _TRACE_TREE_H_

#include <stdbool.h>

struct trace_tree;
struct trace;
struct action;
struct xswitch;
struct trace_tree *trace_tree(void);
void trace_tree_free(struct trace_tree *t);
void trace_tree_print(struct trace_tree *tree);
bool trace_tree_augment(struct trace_tree **tree, struct trace *trace, struct action *a);
bool trace_tree_invalidate(struct trace_tree **tree, const char *name);
void trace_tree_emit_rule(struct xswitch *sw, struct trace_tree *tree);

#endif /* _TRACE_TREE_H_ */
