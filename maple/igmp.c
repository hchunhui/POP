/*
 *IGMP包处理，接收IGMP v1、v2、v3的关系成员报告。发送成员关系查询报告为IGMP v2的查询报告。
 *	目前只完成成员关系报告的接收，而且被动维护成员之间的关系。如果有进程挂掉，控制器是不知道的。
 * 	后期会考虑加入主动发包。
*/
/*消息格式：
 * 	封装在IPv4数据报内，IP协议号是2，每一个IGMP消息的TTL是1，目的IP地址（查询报告为224.0.0.1子网的所有系统，
 * 	成员关系报告是组多播地址，在IGMPv3的成员关系报告里控制器接收的是224.0.0.22，退出报告为224.0.0.2子网的所有路由器）；
 *报告的类型号一共有如下几种：
 *  类型号type        消息名称
 *   0x11            成员关系查询 		(v1,v2,v3都是0x11)
 *   0x12            v1 成员关系报告    	RFC1112
 *   0x16            v2 成员关系报告    	RFC2236
 *   0x17            v2 离开组          	RFC2236
 *   0x22            v3 成员关系报告  	RFC3376
 *  不能识别的消息类型必须被丢弃，其它的消息类型可能会出现在更新的IGMP版本中，或者IGMP的扩展中，或者多播路由协议，或者其它。
 * 
 * 1)IGMP v1,v2报告格式：size = 8字节 (现在只接收type = {0x12, 16})
 * |0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7|
 * +---------------------------------------------------------------------+
 * |      Type      |  Max Rsp Code   |             Checksum             |
 * +---------------------------------------------------------------------+
 * |                             Group Address                           |
 * +---------------------------------------------------------------------+
 * |0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7|
 * 
 * 2）IGMP v3查询报告格式: (现在不发送查询报告，留着以后用)
 * |0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7|
 * +---------------------------------------------------------------------+
 * |   Type = 0x11  |  Max Rsp Code   |             Checksum             |
 * +---------------------------------------------------------------------+
 * |                             Group Address                           |
 * +---------------------------------------------------------------------+
 * |  Resv |S|  QRV |       QQIC      |      Number of Sources (N)       |
 * +---------------------------------------------------------------------+
 * |                            Source Address [1]                       |
 * +---------------------------------------------------------------------+
 * |                            Source Address [2]                       |
 * +---------------------------------------------------------------------+
 * |                               ... ...                               |
 * +---------------------------------------------------------------------+
 * |                            Source Address [M]                       |
 * +---------------------------------------------------------------------+
 * |0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7|
 * 
 * 3)IGMP v3成员关系报告格式：> 12字节（type = 0x22)（注意：v3版本的退出和加入组都是这一个报告）
 *   size = 12 + \sum_{i=1}^{N}{size of Group Record[i]} > 12字节
 * |0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7|
 * +---------------------------------------------------------------------+
 * |   Type = 0x22  |  Max Rsp Code   |             Checksum             |
 * +---------------------------------------------------------------------+
 * |             Reserved             |   Numbers of Group Records(M)    |
 * +---------------------------------------------------------------------+
 * |                                                                     |
 * ...                          Group Record [1]                       ...
 * |                                                                     |
 * +---------------------------------------------------------------------+
 * |                                                                     |
 * ...                          Group Record [2]                       ...
 * |                                                                     |
 * +---------------------------------------------------------------------+
 * |                                                                     |
 * ...                             ...   ...                           ...
 * |                                                                     |
 * +---------------------------------------------------------------------+
 * |                                                                     |
 * ...                          Group Record [M]                       ...
 * |                                                                     |
 * +---------------------------------------------------------------------+
 * 3)每一个组记录如下:size = 8 + M * 4 + Aux Data Len.
 * |0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7|
 * +---------------------------------------------------------------------+
 * |  Record Type   | Aux Data Len(0) |       Number of Sources(N)       |
 * +---------------------------------------------------------------------+
 * |                            Multicast Address                        |
 * +---------------------------------------------------------------------+
 * |                            Source Address [1]                       |
 * +---------------------------------------------------------------------+
 * |                            Source Address [2]                       |
 * +---------------------------------------------------------------------+
 * |                               ... ...                               |
 * +---------------------------------------------------------------------+
 * |                            Source Address [N]                       |
 * +---------------------------------------------------------------------+
 * |                                                                     |
 * ...                          Auxiliary Data 0                       ...
 * |                                                                     |
 * +---------------------------------------------------------------------+
 * |0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7 | 0 1 2 3 4 5 6 7|
 * 
 */

/*
  *1、先判断是不是IGMP包，如果是则读入数据内容，否则退出。
  *2、读入数据内容之后根据包的长度可以判断是哪个版本的;
  *		如果长度==8，则为v2，如果长度>=12，则为v3
  *3、每个版本安装自己版本的处理方式进行处理。
  *4、在每个版本的处理过程中，的流程是：
  * 	1）验证校验和，正确则继续，否则丢弃
  * 	2）如果要读组管理表，则要先使用record("igmp_table"),通知流表生成模块。
  * 	3）如果修改了组管理表，则要invalidatae("igmp_table")，废弃该事件，使用到该事件的流表项要重新生成。
  */

#include "igmp.h"
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

/*debug log file*/
#define LOG_FILE_NAME "/tmp/log_igmp_gehu.txt"
#include <time.h>
FILE *log_file;
time_t timep;
int no;
bool is_log_file_init = false;
void init_log(){
	if ( (log_file = fopen(LOG_FILE_NAME, "a+")) == NULL){
		perror("init error: open log_file error!\n");
		exit(0);
	}
	is_log_file_init = true;
}
void write_log(char *log_info){
	if(is_log_file_init == false)
		init_log();
	time(&timep);
	fprintf(log_file, "\nNO:%d  Time:%s", ++no, ctime(&timep));
	fprintf(log_file, "%s\n", log_info);
	fflush(log_file);
}
char log_buf[1024]; //一条log记录最长的长度
int log_buf_len = 0; //一条记录中已经有的字符数。
/*end debug log file */

/**************** Group Table ************************************************/
/*组表结构：先对groupid进行一次hash，然后hash冲突的用链表散列，
 *		用双向链表存储组表项，每个组表项里面有一个指向成员链表的指针，链表中存储所有组内成员。
 */
#define HASH_TABLE_SIZE 256
#define Hash_key(key) ((key) % HASH_TABLE_SIZE)

/*origin address*/
struct OriginAddr{
	struct OriginAddr *next;
	uint32_t address; //src ip
};
/*group table*/
struct GroupTable{
	uint32_t groupid;                 // The group to route
	struct OriginAddr *origin_addrs;  // The origin adresses
	uint32_t origin_len;              // origin array numbers

	struct GroupTable *next, *prev;   // Pointer to the next and prev group in line.
    /*
     *跟定时器有关的，可以再加。
     */
};
static struct GroupTable *grouptable[HASH_TABLE_SIZE] = {NULL}; //grouptable
static uint32_t group_num = 0; 		//组表元素个数

/*return the pointer to the gourpid entry if in the table, or return NULL*/
static struct GroupTable *_get_group_entry(uint32_t groupid){
	struct GroupTable *ptb = grouptable[Hash_key(groupid)];
	while(ptb != NULL){
		if(ptb->groupid == groupid)
			return ptb;
		ptb = ptb->next;
	}

	return NULL;
}
/*delete origin address from the group table entry*/
static void _delete_origin_address(struct GroupTable *ptb, uint32_t origin_addr){
	if(ptb == NULL)
		return;
	/*entry not empty*/
	struct OriginAddr *cur = ptb->origin_addrs;
	if(cur->address == origin_addr){  //first one
		ptb->origin_addrs = cur->next;
		ptb->origin_len --;
		free(cur);
	}else{
		struct OriginAddr *last = cur;
		cur = cur->next;
		while(cur != NULL && cur->address != origin_addr){
			last = cur;
			cur = cur->next;
		}
		if(cur == NULL) //not found origin address
			return;
		if(cur->address == origin_addr){
			last->next = cur->next;
			ptb->origin_len --;
			free(cur);
		}
	}
}
/*delete group table entry, ptb points to groupid table entry, i means pth in the hashtable[i]*/
static void _delete_gtb(struct GroupTable *ptb, int i){
	if( ptb == NULL) //groupid not in the table
		return;
	/*free origin address*/
	struct OriginAddr *tmp = ptb->origin_addrs;
	while(tmp != NULL){
		ptb->origin_addrs = tmp->next;
		free(tmp);
		tmp = ptb->origin_addrs;
	}
	/*free groupid*/
	if(ptb->next != NULL)
		ptb->next->prev = ptb->prev;
	if(ptb->prev != NULL){
		ptb->prev->next = ptb->next;
	}else{	//ptb->prev == NULL
		grouptable[i] = ptb->next;
	}

	free(ptb);
	if(grouptable[i] == NULL)
		group_num --;
}
/*return groups in grouptable*/
uint32_t get_gtb_num(){
	return group_num;
}
/*if groupid in grouptable return true, or return false*/
bool lookup_groupid(uint32_t groupid){
	return _get_group_entry(groupid) == NULL ? false : true;
}
/*if groupid and origin address src_ip in the grouptable return true, or return false*/
bool lookup_origin_addr(uint32_t groupid, uint32_t origin_addr){
	struct GroupTable *ptb = _get_group_entry(groupid);

	while(ptb != NULL){
		if(ptb->groupid == groupid){
			struct OriginAddr *poa = ptb->origin_addrs;
			while(poa != NULL){
				if(poa->address == origin_addr)
					return true;

				poa = poa->next;
			}
			return false;
		}//end if
		ptb = ptb->next;
	}//end while

	return false;
}
/*return the number of originadds in the groupid, return 0 if groupid not in the table*/
uint32_t get_origin_len(uint32_t groupid){
	uint32_t ret = 0;
	struct GroupTable *ptb = _get_group_entry(groupid);
	while(ptb != NULL){
		if(ptb->groupid == groupid){
			return ptb->origin_len;
		}
		ptb = ptb->next;
	}

	return ret;
}
/* return all origin addresses in a groupid in buffer, function return numbers of origin adress.
 * nums means buffer's num, sizeof(buffer) = 4 * nums (Bytes)， return members' number
 */
uint32_t get_group_maddrs(uint32_t groupid, uint32_t *buffer, uint32_t nums){
	struct GroupTable *ptb = _get_group_entry(groupid);
	uint32_t ret = 0; //the num of origin_addr write in buffer
	while(ptb != NULL){
		if(ptb->groupid == groupid){
			ret = ptb->origin_len;
			struct OriginAddr *poa = ptb->origin_addrs; //pointer to origin address
			assert(ret <= nums);
			int i = 0;
			while(poa != NULL){
				buffer[i++] = poa->address;
				poa = poa->next;
			}

			return ret;
		}
		ptb = ptb->next;
	}

	return ret;
}
/*insert a table entry with{groupid, origin address}*/
void insert_gtb(uint32_t groupid, uint32_t origin_addr){
	struct GroupTable *ptb = _get_group_entry(groupid);

	if(ptb == NULL){ //not exist, malloc and insert from head
		/*group table entry*/
		struct GroupTable *entry = (struct GroupTable *) malloc(sizeof(struct GroupTable));
		struct GroupTable *tmp = grouptable[Hash_key(groupid)];
		entry->groupid = groupid;
		entry->next = tmp; //head insert
		entry->prev = NULL;
		if(tmp != NULL)
			tmp->prev = entry;
		entry->origin_len = 1;
		/*origin address entry*/
		struct OriginAddr *poa = (struct OriginAddr *)malloc(sizeof(struct OriginAddr));
		poa->address = origin_addr;
		poa->next = NULL;
		entry->origin_addrs = poa;
		//grouptable
		grouptable[Hash_key(groupid)] = entry;
		group_num ++;
	}else{	//groupid in the table, address insert from head
		struct OriginAddr *poa = (struct OriginAddr *)malloc(sizeof(struct OriginAddr));
		poa->address = origin_addr;
		poa->next = ptb->origin_addrs;
		ptb->origin_addrs = poa;

		ptb->origin_len++;
	}
}
/*delete a table entry with groupid*/
void delete_gtb(uint32_t groupid){
	struct GroupTable *ptb = _get_group_entry(groupid);
	_delete_gtb(ptb, Hash_key(groupid));
}
/*delete an origin address in an table entry*/
void delete_origin_address(uint32_t groupid, uint32_t origin_addr){
	struct GroupTable *ptb = _get_group_entry(groupid);
	_delete_origin_address(ptb, origin_addr);
	if(ptb->origin_addrs == NULL){ 	//group emtpy, should be deleted
		_delete_gtb(ptb, Hash_key(groupid));
	}
}
/*delete all the origin address from every group*/
void clean_origin_address(uint32_t origin_addr){
	int i;
	for(i = 0; i < HASH_TABLE_SIZE; i++){
		struct GroupTable *ptb = grouptable[i];
		while(ptb != NULL){
			_delete_origin_address(ptb, origin_addr);
			ptb = ptb->next;
		}
	}
}
/********* Group Table End ***************************************/

/*checksum calculate, return the host order*/
uint16_t checksum(uint8_t *buff, uint16_t len){
	int count = len >> 1;
	uint16_t *data = (uint16_t *)buff;
	uint32_t csum = 0;

	int i;
	for(i = 0; i < count; i++)
		csum += ntohs(*(data + i));

	if(len & 0x01){
		uint32_t tmp = data[len - 1] << 8;
		csum += tmp;
	}
	uint16_t high = (csum & 0xffff0000 ) >> 16;
	uint16_t low = csum & 0xffff;

	return ~(low + high);
}

uint8_t buffer[MAX_PACKET_SIZE];
bool is_igmp(struct packet *pkt){
	/*判断是否为igmp包*/
	value_t v = {{0}};
	v = value_from_8(2);  //igmp协议号
	if(test_equal(pkt, "nw_proto", v)){
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:test_equal(pkt, \"nw_proto\", v):true", __FILE__, __LINE__);
	write_log(log_buf);
/*write to log end*/
		return true;
	}

	return false;
}
struct route *f_igmp(struct packet *pkt){
	int i;
	/*get src ip*/
	value_t v = {{0}};
	v = read_packet(pkt, "nw_src");
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:read_packet(pkt, \"nw_src\")", __FILE__, __LINE__);
	write_log(log_buf);
/*write to log end*/

	uint32_t src_ip = value_to_32(v);
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:read_packet(pkt, \"nw_src\"), src_ip=0x%x", __FILE__, __LINE__, src_ip);
	write_log(log_buf);
/*write to log end*/

	//判断IGMP的版本
	uint16_t len = get_packet_data(pkt, "igmp_packet", buffer, MAX_PACKET_SIZE);
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:get_packet_data, len=%d", __FILE__, __LINE__, len);
	write_log(log_buf);
/*write to log end*/
	if( checksum(buffer, len) != 0)
		return route(); /*checksum not equal zero, drop*/
	if(len == IGMP_HEADER_LEN){ 	//igmp v1, v2`
		struct igmphdr *igmp = (struct igmphdr *)buffer;
		uint32_t groupid = igmp->groupid;
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:igmp v2, groupid=0x%x, src_ip=%x", __FILE__, __LINE__, groupid, src_ip);
	write_log(log_buf);
/*write to log end*/
		switch( igmp->type ){
			case QUERY_REPORT:
				/*controller cannot receive query report packet*/
				break;
			case V1_MEMBERSHIP_REPORT:
				/*same as V2_MEMBERSHIP_REPORT...*/
			case V2_MEMBERSHIP_REPORT:
/*write to log */
	lookup_origin_addr(groupid, src_ip);
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:lookup_origin_addr(groupid, src_ip);", __FILE__, __LINE__);

	//遍历组表，输出所有的组内容
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "grouptable as following, has %d groups:", group_num);
	for(i = 0; i < HASH_TABLE_SIZE; i++){
		if(grouptable[i] == NULL)
			continue;

		struct GroupTable *debug_pgtb = grouptable[i];
		while(debug_pgtb != NULL){
			log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nGroupid:0x%x numbers of members:%u",
					debug_pgtb->groupid, debug_pgtb->origin_len);

			struct OriginAddr *debug_poad = debug_pgtb->origin_addrs;
			while(debug_poad != NULL){
				log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nsrc_ip:0x%x", debug_poad->address);
				debug_poad = debug_poad->next;
			}

			debug_pgtb = debug_pgtb->next;
		}
	}
	write_log(log_buf);
/*write to log end*/
				if( lookup_origin_addr(groupid, src_ip) == false){	//如果不存在，则加入
					insert_gtb(groupid, src_ip);
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:insert_gtb(groupid, src_ip);", __FILE__, __LINE__);

	//遍历组表，输出所有的组内容
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "grouptable as following, has %d groups:", group_num);
	for(i = 0; i < HASH_TABLE_SIZE; i++){
		if(grouptable[i] == NULL)
			continue;
		struct GroupTable *debug_pgtb = grouptable[i];
		while(debug_pgtb != NULL){
			log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nGroupid:0x%x numbers of members:%u",
					debug_pgtb->groupid, debug_pgtb->origin_len);

			struct OriginAddr *debug_poad = debug_pgtb->origin_addrs;
			while(debug_poad != NULL){
				log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nsrc_ip:0x%x", debug_poad->address);

				debug_poad = debug_poad->next;
			}

			debug_pgtb = debug_pgtb->next;
		}
	}
	write_log(log_buf);
/*write to log end*/
					invalidate(RECORD_NAME); //修改组表
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:invalidate(RECORD_NAME);", __FILE__, __LINE__);
	write_log(log_buf);
/*write to log end*/
				}
				break;
			case V2_LEAVE_GROUP:
/*write to log*/
	lookup_origin_addr(groupid, src_ip);
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:lookup_origin_addr(groupid, src_ip);", __FILE__, __LINE__);

	//遍历组表，输出所有的组内容
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "grouptable as following, has %d groups:", group_num);
	for(i = 0; i < HASH_TABLE_SIZE; i++){
		if(grouptable[i] == NULL)
			continue;

		struct GroupTable *debug_pgtb = grouptable[i];
		while(debug_pgtb != NULL){
			log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nGroupid:0x%x numbers of members:%u",
					debug_pgtb->groupid, debug_pgtb->origin_len);

			struct OriginAddr *debug_poad = debug_pgtb->origin_addrs;
			while(debug_poad != NULL){
				log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nsrc_ip:0x%x", debug_poad->address);
				debug_poad = debug_poad->next;
			}

			debug_pgtb = debug_pgtb->next;
		}
	}
	write_log(log_buf);
/*write to log end*/
				if(lookup_origin_addr(groupid, src_ip) == true){ //如果存在则，删除
					delete_origin_address(groupid, src_ip);
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:lookup_origin_addr(groupid, src_ip);", __FILE__, __LINE__);

	//遍历组表，输出所有的组内容
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "grouptable as following, has %d groups:", group_num);
	for(i = 0; i < HASH_TABLE_SIZE; i++){
		if(grouptable[i] == NULL)
			continue;
		struct GroupTable *debug_pgtb = grouptable[i];
		while(debug_pgtb != NULL){
			log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nGroupid:0x%x numbers of members:%u",
					debug_pgtb->groupid, debug_pgtb->origin_len);

			struct OriginAddr *debug_poad = debug_pgtb->origin_addrs;
			while(debug_poad != NULL){
				log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nsrc_ip:0x%x", debug_poad->address);
				debug_poad = debug_poad->next;
			}

			debug_pgtb = debug_pgtb->next;
		}
	}
	write_log(log_buf);
/*write to log end*/
					invalidate(RECORD_NAME); //修改组表
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:invalidate(RECORD_NAME);", __FILE__, __LINE__);
	write_log(log_buf);
/*write to log end*/
				}
				break;
			default:
				/*error*/
				break;
		}
	}else if(len >= IGMP_V3_REPORT_HEADER_LEN){ //igmp v3
		struct igmpv3_report *igmpv3_pkt = (struct igmpv3_report *)buffer;
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:igmp v3 packet:type = %u, numbers of records = %u ", __FILE__, __LINE__,
			igmpv3_pkt->type, ntohs(igmpv3_pkt->ngrecord));
	write_log(log_buf);
/*write to log end*/
		/*only deal with membership report*/
		if(igmpv3_pkt->type != V3_MEMBERSHIP_REPORT)
			return route();
		uint16_t ngrecord = ntohs(igmpv3_pkt->ngrecord);
		uint8_t *grecord = (uint8_t *)igmpv3_pkt->grec;
		int i;

		/*deal with each record*/
		for(i = 0; i < ngrecord; i++){
			uint8_t grec_type = *grecord++;
			uint8_t grec_auxdlen = *grecord++; //一般为0，不为0,则要跳过末尾的grec_auxdlen个字节
			uint16_t grec_nsrc = ntohs(*((uint16_t *)grecord));
			grecord += sizeof(uint16_t);
			uint32_t grec_mcaddr = ntohl(*((uint32_t *)grecord));
			grecord += sizeof(uint32_t);
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:igmp v3 packet records[%d]:groupid=%x type=%x, nums=%u", __FILE__, __LINE__,
			i, grec_mcaddr, grec_type, grec_nsrc);
	write_log(log_buf);
/*write to log end*/
			switch( grec_type ){
				case V3_MODE_IS_INCLUDE: 	//接收来自源，如果源为空则， 为退出报告，否则加入组
					if( grec_nsrc == 0){ //INCLUDE{}, 表示退出报告
						if(lookup_origin_addr(grec_mcaddr, src_ip) == true){
							delete_origin_address(grec_mcaddr, src_ip);
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:delete_origin_address(grec_mcaddr, src_ip);", __FILE__, __LINE__);

	//遍历组表，输出所有的组内容
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "grouptable as following, has %d groups:", group_num);
	for(i = 0; i < HASH_TABLE_SIZE; i++){
		if(grouptable[i] == NULL)
			continue;

		struct GroupTable *debug_pgtb = grouptable[i];
		while(debug_pgtb != NULL){
			log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nGroupid:0x%x numbers of members:%u",
					debug_pgtb->groupid, debug_pgtb->origin_len);

			struct OriginAddr *debug_poad = debug_pgtb->origin_addrs;
			while(debug_poad != NULL){
				log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nsrc_ip:0x%x", debug_poad->address);
				debug_poad = debug_poad->next;
			}

			debug_pgtb = debug_pgtb->next;
		}
	}
	write_log(log_buf);
/*write to log end*/
							invalidate(RECORD_NAME); //修改组表
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:invalidate(RECORD_NAME);", __FILE__, __LINE__);
		write_log(log_buf);
/*write to log end*/
					}
					}else{ //不为0，表示对源进行控制，现在不对源进行控制，只进行插入
						if(lookup_origin_addr(grec_mcaddr, src_ip) == false)
							insert_gtb(grec_mcaddr, src_ip);
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:insert_gtb(grec_mcaddr, src_ip);", __FILE__, __LINE__);

	//遍历组表，输出所有的组内容
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "grouptable as following, has %d groups:", group_num);

	for(i = 0; i < HASH_TABLE_SIZE; i++){
		if(grouptable[i] == NULL)
			continue;

		struct GroupTable *debug_pgtb = grouptable[i];
		while(debug_pgtb != NULL){
			log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nGroupid:0x%x numbers of members:%u",
					debug_pgtb->groupid, debug_pgtb->origin_len);

			struct OriginAddr *debug_poad = debug_pgtb->origin_addrs;
			while(debug_poad != NULL){
				log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nsrc_ip:0x%x", debug_poad->address);
				debug_poad = debug_poad->next;
			}

			debug_pgtb = debug_pgtb->next;
		}
	}
	write_log(log_buf);
/*write to log end*/

						invalidate(RECORD_NAME); //修改组表
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:invalidate(RECORD_NAME);", __FILE__, __LINE__);
	write_log(log_buf);
/*write to log end*/
					}
					break;
				case V3_MODE_IS_EXCLUDE:
					if( grec_nsrc == 0){ //EXCLUDE{}, 表示组加入报告
						if(lookup_origin_addr(grec_mcaddr, src_ip) == false){
							insert_gtb(grec_mcaddr, src_ip);
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:insert_gtb(grec_mcaddr, src_ip);", __FILE__, __LINE__);

	//遍历组表，输出所有的组内容
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "grouptable as following, has %d groups:", group_num);
	for(i = 0; i < HASH_TABLE_SIZE; i++){
		if(grouptable[i] == NULL)
			continue;

		struct GroupTable *debug_pgtb = grouptable[i];
		while(debug_pgtb != NULL){
			log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nGroupid:0x%x numbers of members:%u",
					debug_pgtb->groupid, debug_pgtb->origin_len);

			struct OriginAddr *debug_poad = debug_pgtb->origin_addrs;
			while(debug_poad != NULL){
				log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nsrc_ip:0x%x", debug_poad->address);
				debug_poad = debug_poad->next;
			}

			debug_pgtb = debug_pgtb->next;
		}
	}
	write_log(log_buf);
/*write to log end*/
							invalidate(RECORD_NAME); //修改组表
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:invalidate(RECORD_NAME);", __FILE__, __LINE__);
	write_log(log_buf);
/*write to log end*/
						}
					}else{ //不为0，表示对源进行控制，现在不对源进行控制，只进行插入
						if(lookup_origin_addr(grec_mcaddr, src_ip) == false){
							insert_gtb(grec_mcaddr, src_ip);
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:insert_gtb(grec_mcaddr, src_ip);", __FILE__, __LINE__);
	//遍历组表，输出所有的组内容
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "grouptable as following, has %d groups:", group_num);

	for(i = 0; i < HASH_TABLE_SIZE; i++){
		if(grouptable[i] == NULL)
			continue;

		struct GroupTable *debug_pgtb = grouptable[i];
		while(debug_pgtb != NULL){
			log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nGroupid:0x%x numbers of members:%u",
					debug_pgtb->groupid, debug_pgtb->origin_len);

			struct OriginAddr *debug_poad = debug_pgtb->origin_addrs;
			while(debug_poad != NULL){
				log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nsrc_ip:0x%x", debug_poad->address);
				debug_poad = debug_poad->next;
			}

			debug_pgtb = debug_pgtb->next;
		}
	}
	write_log(log_buf);
/*write to log end*/
							invalidate(RECORD_NAME); //修改组表
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:invalidate(RECORD_NAME);", __FILE__, __LINE__);
	write_log(log_buf);
/*write to log end*/
						}
					}
					break;
				case V3_CHANGE_TO_INCLUDE: //源地址不为空，则当做插入，否则当做退出操作
					if( grec_nsrc == 0){
						if(lookup_origin_addr(grec_mcaddr, src_ip) == true){
							delete_origin_address(grec_mcaddr, src_ip);
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:delete_origin_address(grec_mcaddr, src_ip);", __FILE__, __LINE__);
	//遍历组表，输出所有的组内容
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\ngrouptable as following, has %d groups:", group_num);

	for(i = 0; i < HASH_TABLE_SIZE; i++){
		if(grouptable[i] == NULL)
			continue;

		struct GroupTable *debug_pgtb = grouptable[i];
		while(debug_pgtb != NULL){
			log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nGroupid:0x%x numbers of members:%u",
					debug_pgtb->groupid, debug_pgtb->origin_len);
			struct OriginAddr *debug_poad = debug_pgtb->origin_addrs;
			while(debug_poad != NULL){
				log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nsrc_ip:0x%x", debug_poad->address);
				debug_poad = debug_poad->next;
			}

			debug_pgtb = debug_pgtb->next;
		}
	}
	write_log(log_buf);
/*write to log end*/
							invalidate(RECORD_NAME); //修改组表
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:invalidate(RECORD_NAME);", __FILE__, __LINE__);
	write_log(log_buf);
/*write to log end*/
						}
					}else{
						if(lookup_origin_addr(grec_mcaddr, src_ip) == false){
							insert_gtb(grec_mcaddr, src_ip);
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:insert_gtb(grec_mcaddr, src_ip);", __FILE__, __LINE__);

	//遍历组表，输出所有的组内容
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "grouptable as following, has %d groups:", group_num);
	for(i = 0; i < HASH_TABLE_SIZE; i++){
		if(grouptable[i] == NULL)
			continue;

		struct GroupTable *debug_pgtb = grouptable[i];
		while(debug_pgtb != NULL){
			log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nGroupid:0x%x numbers of members:%u",
					debug_pgtb->groupid, debug_pgtb->origin_len);

			struct OriginAddr *debug_poad = debug_pgtb->origin_addrs;
			while(debug_poad != NULL){
				log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nsrc_ip:0x%x", debug_poad->address);
				debug_poad = debug_poad->next;
			}

			debug_pgtb = debug_pgtb->next;
		}
	}
	write_log(log_buf);
/*write to log end*/
						invalidate(RECORD_NAME); //修改组表
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:invalidate(RECORD_NAME);", __FILE__, __LINE__);
	write_log(log_buf);
/*write to log end*/
						}
					}
					break;
				case V3_CHANGE_TO_EXCLUDE: //暂时当做插入处理
					if(lookup_origin_addr(grec_mcaddr, src_ip) == false){
						insert_gtb(grec_mcaddr, src_ip);
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:insert_gtb(multiaddr:0x%x, src_ip:0x%x);", __FILE__, __LINE__, grec_mcaddr, src_ip);

	//遍历组表，输出所有的组内容
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\ngrouptable as following, has %d groups:", group_num);

	for(i = 0; i < HASH_TABLE_SIZE; i++){
		if(grouptable[i] == NULL)
			continue;
		struct GroupTable *debug_pgtb = grouptable[i];
		while(debug_pgtb != NULL){
			log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nGroupid:0x%x numbers of members:%u",
					debug_pgtb->groupid, debug_pgtb->origin_len);
			struct OriginAddr *debug_poad = debug_pgtb->origin_addrs;
			while(debug_poad != NULL){
				log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "\nsrc_ip:0x%x", (debug_poad->address));
				debug_poad = debug_poad->next;
			}

			debug_pgtb = debug_pgtb->next;
		}
	}
	write_log(log_buf);
/*write to log end*/
						invalidate(RECORD_NAME); //修改组表
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:invalidate(RECORD_NAME);", __FILE__, __LINE__);
	write_log(log_buf);
/*write to log end*/
					}
					break;
				case V3_ALLOW_NEW_SOURCE: //不处理
					break;
				case V3_BLOCK_OLD_SOURCE: //不处理
					break;
				default:
					/*error*/
					break;
			}
			grecord += sizeof(uint32_t) * grec_nsrc + grec_auxdlen; //step to next record
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:next record", __FILE__, __LINE__);
	write_log(log_buf);
/*write to log end*/
		}
	}else{
		/*error, drop */
		return route();
	}
	return route(); //不下发流表，则返回路径为NULL
/*write to log*/
	log_buf_len = 0;
	log_buf_len += snprintf(log_buf+log_buf_len, 1024-log_buf_len, "%s:%d:return", __FILE__, __LINE__);
	write_log(log_buf);
/*write to log end*/
}
