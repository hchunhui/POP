#ifndef _MAPLE_H_
#define _MAPLE_H_
#include "types.h"

struct xswitch;
void maple_switch_up(struct xswitch *sw);
void maple_packet_in(struct xswitch *sw, int in_port, const uint8_t *packet, int packet_len);
void maple_switch_down(struct xswitch *sw);

#endif /* _MAPLE_H_ */
