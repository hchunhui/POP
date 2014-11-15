#ifndef _IGMP_H_
#define _IGMP_H_

#include "maple_api.h"
#include "types.h"
#include "value.h"
#include <assert.h>
#include <string.h>
#include <arpa/inet.h> //ntohl, ntohs, htonl, htons

/* IGMP 包长度的最大值*/

#define IGMP_PROTOCOL_NUM 	2

/*IGMP packet version type */
#define QUERY_REPORT 	 		0x11 	//v1 v2
#define V1_MEMBERSHIP_REPORT 	0x12
#define V2_MEMBERSHIP_REPORT 	0x16
#define V2_LEAVE_GROUP 			0x17
#define V3_MEMBERSHIP_REPORT 	0x22

/* IGMP v3 membership report record type */
#define V3_MODE_IS_INCLUDE 		1
#define V3_MODE_IS_EXCLUDE 		2
#define V3_CHANGE_TO_INCLUDE 	3
#define V3_CHANGE_TO_EXCLUDE 	4
#define V3_ALLOW_NEW_SOURCE 	5
#define V3_BLOCK_OLD_SOURCE 	6

/* query interval seconds */
#define QUERY_INTERVAL 125
#define QUERY_RESPONSE_INTERVAL 100 //10s, 100 * 0.1s

#define IGMP_ALL_HOSTS_MAC  	(0xE0000001L) //224.0.0.1
#define IGMP_ALL_ROUTERS_MAC  	(0xE0000002L) //224.0.0.2
#define IGMPV3_ALL_MAR		  	(0xE0000016L) //224.0.0.22
#define IGMP_LOCAL_GROUP	  	(0xE0000000L) //224.0.0.0
#define IGMP_LOCAL_GOURP_MASK  	(0xFFFFFF00L) //255.255.255.0

#define MAX_PACKET_SIZE 1480 	//1500 - 20
// #define PACKET_SIZE 8	//v2, v1 packet size
#define IGMP_HEADER_LEN 8 	//V1, V2
#define IGMP_V3_QUERY_HEADER_LEN 12
#define IGMP_V3_RECORD_HEADER_LEN 8
#define IGMP_V3_REPORT_HEADER_LEN 12

#define RECORD_NAME "igmp_record"

/*IGMP v1,v2 header */
struct igmphdr{
	uint8_t type; 	//packet type 0x11 0x12 0x16 0x17
	uint8_t max_resp_code;
	uint16_t csum;
	uint32_t groupid;  //224.0.0.1在内存中为：（从低地址到高地址）0x01, 0x00, 0x00, 0xea
};
/* IGMP v3  group record*/
struct igmpv3_grec{
	uint8_t grec_type; 	 	//record type
	uint8_t grec_auxdlen;
	uint16_t grec_nsrcs;	//num of source adresses
	uint32_t grec_mcaddr; 	//multicast address, 224.0.0.1在内存中为：（从低地址到高地址）0x01, 0x00, 0x00, 0xea
	uint32_t grec_src[0]; 	//source addresses IP地址：10.0.0.1在内存中为：（从低地址到高地址）0x01, 0x00, 0x00, 0x0a
	/*紧随grec_auxlen个source adress， 但是协议的auxiliary data就在source adress之后，
	 * 只能在结构体之外体现了。
	*/
};
/* IGMP v3 membership report packet*/
struct igmpv3_report{
	uint8_t type ; 		//report type 0x22
	uint8_t max_rsp_code;
	uint16_t csum;
	uint16_t resv; 	//reserved
	uint16_t ngrecord;	//number of group records
	struct igmpv3_grec grec[0]; //group records
};

/*IGMP v3 membership query packet.(not used now)*/
struct igmpv3_query{
	uint8_t type ;  	//report type 0x11
	uint8_t max_rsp_code;
	uint16_t csum;
	uint32_t groupid;
	uint8_t resv_s_qrv; //4bit resv, 1bit S, 3bit QRV
	uint8_t qqic;
	uint16_t nsrcs; 	//number of source address
	uint32_t src[0];
};

uint32_t get_gtb_num(); //组表表项数目
bool lookup_groupid(uint32_t groupid);	//查组地址
bool lookup_origin_addr(uint32_t groupid, uint32_t origin_addr); //查（组地址，源地址）
uint32_t get_origin_len(uint32_t groupid); //查一个组有多少个成员地址
uint32_t get_group_maddrs(uint32_t groupid, uint32_t *buffer, uint32_t nums); //获取一个组的所有成员到buffer里
void insert_gtb(uint32_t groupid, uint32_t origin_addr); //将（组地址，源地址）插入组表
void delete_gtb(uint32_t groupid); //删除一个组地址选项
void delete_origin_address(uint32_t groupid, uint32_t origin_addr); //删除一个（组地址，源地址）对
void clean_origin_address(uint32_t origin_addr); //清除一个主机

#endif  /*_IGMP_H_*/
