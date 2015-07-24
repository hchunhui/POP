#ifndef _CORE_H_
#define _CORE_H_
#include "types.h"

struct xswitch;
void core_init(const char *algo_file, const char *spec_file);
void core_switch_up(struct xswitch *sw);
void core_packet_in(struct xswitch *sw, int in_port, uint8_t *packet, int packet_len);
void core_switch_down(struct xswitch *sw);

void core_invalidate(bool (*p)(void *p_data, const char *name, const void *arg), void *p_data);
#endif /* _CORE_H_ */
