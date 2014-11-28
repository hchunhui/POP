#ifndef __LLDP_H
#define __LLDP_H
#include "types.h"

enum tlv_type
{
	END_TLV = 0, 
	CHASSIS_ID_TLV,
	PORT_ID_TLV,
	TTL_TLV,
	PORT_DESC_TLV,  //4
	SYSTEM_NAME_TLV,
	SYSTEM_DESC_TLV,
	SYSTEM_CAP_TLV,
	MANAGEMENT_ADDR_TLV, //8
	ORGANIZATIONALLY_SPECIFIC_TLV = 127
};
enum chassis_id_subtype
{
	SUB_RESERVED = 0,
	SUB_CHASSIS,
	SUB_IF_ALIAS,
	SUB_PORT,
	SUB_MAC, // 4
	SUB_NETWORK,
	SUB_IF_NAME,
	SUB_LOCAL, // 7
};
enum port_id_subtype
{
	PORT_SUB_RESERVED = 0,
	PORT_SUB_IF_ALIAS,
	PORT_SUB_PORT,
	PORT_SUB_MAC, // 4
	PORT_SUB_NETWORK,
	PORT_SUB_IF_NAME,
	PORT_SUB_CIRC_ID,
	PORT_SUB_LOCAL, // 7
};
struct lldp_tlv {
	enum tlv_type type;
	uint16_t length;
	uint8_t value[512];
};
static inline
uint8_t
lldp_tlv_get_type(struct lldp_tlv *tlv)
{
	return tlv->type;
}
static inline
uint16_t
lldp_tlv_get_length(struct lldp_tlv *tlv)
{
	return tlv->length;
}


#endif
