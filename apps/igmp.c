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
#include "map.h"

/*** group table ***/
static bool mgt_eq(int_or_ptr_t k1, int_or_ptr_t k2)
{
	return k1.v == k2.v;
}

static unsigned int mgt_hash(int_or_ptr_t key)
{
	return key.v;
}

static int_or_ptr_t mgt_dup_key(int_or_ptr_t key)
{
	return key;
}

static void mgt_free_key(int_or_ptr_t key)
{

}

static bool mgt_eq_val(int_or_ptr_t k1, int_or_ptr_t k2)
{
	return false;
}

static void mgt_free_val(int_or_ptr_t val)
{

}

struct map *igmp_init(void)
{
	return map(mgt_eq, mgt_hash, mgt_dup_key, mgt_free_key);
}

struct igmp_addrs *igmp_get_maddrs(struct map *group_table, uint32_t groupid)
{
	struct igmp_addrs *l;

	l = map_read(group_table, INT(groupid)).p;
	if(l == NULL) {
		l = malloc(sizeof(struct igmp_addrs));
		l->addrs = NULL;
		l->n = 0;
		map_add_key(group_table, INT(groupid), PTR(l), mgt_eq_val, mgt_free_val);
	}
	return l;
}

static bool addrs_in(struct igmp_addrs *l, uint32_t ip)
{
	int i;
	for(i = 0; i < l->n; i++)
		if(l->addrs[i] == ip)
			return true;
	return false;
}

static void addrs_add(struct igmp_addrs *l, uint32_t ip)
{
	if(addrs_in(l, ip))
		return;

	l->addrs = realloc(l->addrs, l->n + 1);
	l->addrs[l->n] = ip;
	l->n++;
}

static void addrs_del(struct igmp_addrs *l, uint32_t ip)
{
	int i;
	for(i = 0; i < l->n; i++)
		if(l->addrs[i] == ip) {
			int j;
			l->n--;
			for(j = i; j < l->n; j++)
				l->addrs[j] = l->addrs[j+1];
			return;
		}
}

static void enter_group(struct map *gt, uint32_t groupid, uint32_t ip)
{
	struct igmp_addrs *l;
	l = igmp_get_maddrs(gt, groupid);
	addrs_add(l, ip);
	map_mod(gt, INT(groupid), PTR(l));
}

static void exit_group(struct map *gt, uint32_t groupid, uint32_t ip)
{
	struct igmp_addrs *l;
	l = igmp_get_maddrs(gt, groupid);
	addrs_del(l, ip);
	map_mod(gt, INT(groupid), PTR(l));
}

/*checksum calculate, return the host order*/
uint16_t checksum(const uint8_t *buff, int len){
	int count = len >> 1;
	const uint16_t *data = (const uint16_t *)buff;
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

void process_igmp(struct map *group_table, uint32_t src_ip, const uint8_t *buffer, int len)
{
	if( checksum(buffer, len) != 0)
		return;
	if(len == IGMP_HEADER_LEN){ 	//igmp v1, v2`
		const struct igmphdr *igmp = (const struct igmphdr *) buffer;
		uint32_t groupid = igmp->groupid;

		switch( igmp->type ){
			case QUERY_REPORT:
				/*controller cannot receive query report packet*/
				break;
			case V1_MEMBERSHIP_REPORT:
				/*same as V2_MEMBERSHIP_REPORT...*/
			case V2_MEMBERSHIP_REPORT:
				enter_group(group_table, groupid, src_ip);
				break;
			case V2_LEAVE_GROUP:
				exit_group(group_table, groupid, src_ip);
				break;
			default:
				/*error*/
				break;
		}
	}else if(len >= IGMP_V3_REPORT_HEADER_LEN){ //igmp v3
		const struct igmpv3_report *igmpv3_pkt = (const struct igmpv3_report *) buffer;
		/*only deal with membership report*/
		if(igmpv3_pkt->type != V3_MEMBERSHIP_REPORT)
			return;
		uint16_t ngrecord = ntohs(igmpv3_pkt->ngrecord);
		const uint8_t *grecord = (const uint8_t *) igmpv3_pkt->grec;
		int i;

		/*deal with each record*/
		for(i = 0; i < ngrecord; i++){
			uint8_t grec_type = *grecord++;
			uint8_t grec_auxdlen = *grecord++; //一般为0，不为0,则要跳过末尾的grec_auxdlen个字节
			uint16_t grec_nsrc = ntohs(*((const uint16_t *)grecord));
			grecord += sizeof(uint16_t);
			uint32_t grec_mcaddr = ntohl(*((const uint32_t *)grecord));
			grecord += sizeof(uint32_t);

			switch( grec_type ){
				case V3_MODE_IS_INCLUDE: 	//接收来自源，如果源为空则， 为退出报告，否则加入组
					if( grec_nsrc == 0){ //INCLUDE{}, 表示退出报告
						exit_group(group_table, grec_mcaddr, src_ip);
					}else{ //不为0，表示对源进行控制，现在不对源进行控制，只进行插入
						enter_group(group_table, grec_mcaddr, src_ip);
					}
					break;
				case V3_MODE_IS_EXCLUDE:
					if( grec_nsrc == 0){ //EXCLUDE{}, 表示组加入报告
						enter_group(group_table, grec_mcaddr, src_ip);
					}else{ //不为0，表示对源进行控制，现在不对源进行控制，只进行插入
						enter_group(group_table, grec_mcaddr, src_ip);
					}
					break;
				case V3_CHANGE_TO_INCLUDE: //源地址不为空，则当做插入，否则当做退出操作
					if( grec_nsrc == 0){
						exit_group(group_table, grec_mcaddr, src_ip);
					}else{
						enter_group(group_table, grec_mcaddr, src_ip);
					}
					break;
				case V3_CHANGE_TO_EXCLUDE: //暂时当做插入处理
					enter_group(group_table, grec_mcaddr, src_ip);
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
		}
	}else{
		/*error, drop */
		return;
	}
	return;
}
