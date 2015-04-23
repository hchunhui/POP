#ifndef _LEARNING_H_
#define _LEARNING_H_

#include "types.h"

struct route *learning(struct map *env, char *table_name,
		       struct entity *me, int in_port, uint64_t src,
		       uint64_t dst);
void learning_init(struct map *env, char *table_name);

#endif /* _LEARNING_H_ */
