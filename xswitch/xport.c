#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "types.h"
#include "xswitch-private.h"

struct xport *xport_new(uint16_t port_id)
{
	struct xport *xp = calloc(1, sizeof(struct xport));
	xp->port_id = port_id;
	return xp;
}
struct xport *xport_copy(const struct xport *xp)
{
	struct xport *rxp;
	if (xp == NULL)
		return NULL;
	rxp = malloc(sizeof(struct xport));
	memcpy(rxp, xp, sizeof(struct xport));
	return rxp;
}
void xport_free(struct xport *xp)
{
	free(xp);
}
struct xport *xport_lookup(struct xswitch *sw, uint16_t port_id)
{
	struct xport **xps = xswitch_get_xports(sw);
	struct xport *xp;
	uint16_t index = port_id/XPORT_HASH_SIZE;
	for (xp = xps[index]; xp != NULL; xp = xp->next)
		if (xp->port_id == port_id)
			return xp;
	return NULL;
}
struct xport *xport_insert(struct xswitch *sw, const struct xport *xp)
{
	struct xport **xps = xswitch_get_xports(sw);
	uint16_t index;
	struct xport *rxp, *tmp;
	if (xp == NULL)
		return NULL;
	index = xp->port_id/XPORT_HASH_SIZE;
	rxp = xps[index];
	if (rxp == NULL) {
		xps[index] = xport_copy(xp);
		return xps[index];
	}
	for (; rxp != NULL; rxp = rxp->next) {
		tmp = rxp;
		if (rxp->port_id == xp->port_id)
			return rxp;
	}
	tmp->next = xport_copy(xp);
	return tmp->next;
}
void xport_query(struct xport *xp, uint64_t *recvpkts, uint64_t *recvbytes,
		 uint64_t *recent_recvpkts, uint64_t *recent_recvbytes)
{
	if (xp == NULL)
		return;
	*recvpkts = xp->recvpkts;
	*recvbytes = xp->recvbytes;
	*recent_recvpkts = xp->recent_recvpkts;
	*recent_recvbytes = xp->recent_recvbytes;
}
void xport_update(struct xport *xp, uint64_t recvpkts, uint64_t recvbytes)
{
	if (xp == NULL)
		return;
	xp->recent_recvpkts = recvpkts - xp->recvpkts;
	xp->recent_recvbytes = recvbytes - xp->recvbytes;
	xp->recvpkts = recvpkts;
	xp->recvbytes = recvbytes;
}
bool xport_delete(struct xswitch *sw, struct xport *xp)
{
	uint16_t index = xp->port_id/XPORT_HASH_SIZE;
	struct xport **xps = xswitch_get_xports(sw);
	struct xport *tmp1, *tmp2;
	tmp1 = tmp2 = xps[index];
	if (tmp1 == NULL || xp == NULL)
		return false;
	if (tmp1 == xp)
		xps[index] = tmp1->next;
	while (tmp2) {
		tmp1 = tmp2->next;
		if (tmp1 == xp) {
			tmp2->next = tmp1->next;
			break;
		}
		tmp2 = tmp1;
	}
	if (tmp1) {
		free(tmp1);
		return true;
	}
	return false;
}
uint16_t xport_get_port_id(const struct xport *xp)
{
	if (xp == NULL)
		return 0;
	return xp->port_id;
}
uint64_t xport_get_recvpkts(const struct xport *xp)
{
	if (xp == NULL)
		return 0;
	return xp->recvpkts;
}
uint64_t xport_get_recvbytes(const struct xport *xp)
{
	if (xp == NULL)
		return 0;
	return xp->recvbytes;
}
uint64_t xport_get_recent_recvpkts(const struct xport *xp)
{
	if (xp == NULL)
		return 0;
	return xp->recent_recvpkts;
}
uint64_t xport_get_recent_recvbytes(const struct xport *xp)
{
	if (xp == NULL)
		return 0;
	return xp->recent_recvbytes;
}
struct xport *xport_get_next(const struct xport *xp)
{
	if (xp == NULL)
		return NULL;
	return xp->next;
}
