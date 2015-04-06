#ifndef _MAPLE_H_
#define _MAPLE_H_
#include "types.h"

struct xswitch;
void maple_init(const char *algo_file, const char *spec_file);
void maple_switch_up(struct xswitch *sw);
void maple_packet_in(struct xswitch *sw, int in_port, uint8_t *packet, int packet_len);
void maple_switch_down(struct xswitch *sw);

void maple_invalidate(bool (*p)(void *p_data, const char *name, const void *arg), void *p_data);
#endif /* _MAPLE_H_ */
