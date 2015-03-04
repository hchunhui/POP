#ifndef _POF_HEADER_H_
#define _POF_HEADER_H_

#include <stdint.h>

struct pof_header {
	uint8_t  version; /* POF_VERSION. */
	uint8_t  type;    /* One of the POFT_ constants. */
	uint16_t length;  /* Length including this pof_header. */
	uint32_t xid;     /* Transaction id associated with this packet.
			     Replies use the same id as was in the request to facilitate pairing. */
};

#endif /* _POF_HEADER_H_ */
