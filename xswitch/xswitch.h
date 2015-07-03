#ifndef _XSWITCH_H_
#define _XSWITCH_H_
#include "types.h"

#define XPORT_HASH_SIZE 1000

struct msgbuf;

struct xport;
struct xswitch;
struct flow_table;
struct match;
struct action;

enum port_status {
	PORT_DOWN,
	PORT_UP,
};

/* xswitch */
struct xswitch *xswitch(dpid_t dpid, int ports, void *conn);
void xswitch_free(struct xswitch *sw);
void xswitch_send(struct xswitch *sw, struct msgbuf *b);
dpid_t xswitch_get_dpid(struct xswitch *sw);
int xswitch_get_num_ports(struct xswitch *sw);
struct xport **xswitch_get_xports(struct xswitch *sw);
void xswitch_table_lock(struct xswitch *sw);
void xswitch_table_unlock(struct xswitch *sw);

/* xport */
struct xport *xport_new(uint16_t port_id);
struct xport *xport_copy(const struct xport *xp);
void xport_free(struct xport *xp);
struct xport *xport_lookup(struct xswitch *sw, uint16_t port_id);
struct xport *xport_insert(struct xswitch *sw, const struct xport *xp);
void xport_update(struct xport *xp, uint64_t recvpkts, uint64_t recvbytes);
void xport_query(struct xport *xp,
		 uint64_t *recvpkts/*OUT*/,
		 uint64_t *recvbytes/*OUT*/,
		 uint64_t *recent_recvpkts/*OUT*/,
		 uint64_t *recent_recvbytes/*OUT*/);
bool xport_delete(struct xswitch *sw, struct xport *xp);
uint16_t xport_get_port_id(const struct xport *xp);
uint64_t xport_get_recvpkts(const struct xport *xp);
uint64_t xport_get_recvbytes(const struct xport *xp);
uint64_t xport_get_recent_recvpkts(const struct xport *xp);
uint64_t xport_get_recent_recvbytes(const struct xport *xp);
struct xport *xport_get_next(const struct xport *xp);

/* flow table */
enum match_field_type { MATCH_FIELD_PACKET, MATCH_FIELD_METADATA };
enum flow_table_type { FLOW_TABLE_TYPE_MM, FLOW_TABLE_TYPE_LPM };
struct flow_table *flow_table(int tid, enum flow_table_type type, int size);
void flow_table_free(struct flow_table *ft);
void flow_table_add_field(struct flow_table *ft,
			  const char *name, enum match_field_type type, int offset, int length);
int flow_table_get_field_index(struct flow_table *ft, const char *name);
int flow_table_get_tid(struct flow_table *ft);
void flow_table_get_offset_length(struct flow_table *ft, int idx, int *offset, int *length);
int flow_table_get_entry_index(struct flow_table *ft);
void flow_table_put_entry_index(struct flow_table *ft, int index);

/* match */
struct match *match(void);
struct match *match_copy(struct match *m);
void match_add(struct match *m, const char *name, value_t value, value_t mask);
void match_free(struct match *m);
void match_dump(struct match *m, char *buf, int n);


/* action */
enum action_oper_type {
	AC_OP_ADD,
	AC_OP_SUB,
	AC_OP_AND,
	AC_OP_OR,
	AC_OP_SHL,
	AC_OP_SHR,
	AC_OP_XOR,
	AC_OP_NOR,
};

enum action_type {
	/* "actions" */
	AC_DROP,
	AC_PACKET_IN,
	AC_OUTPUT             /*port*/,
	AC_SET_FIELD,
	AC_ADD_FIELD,
	AC_DEL_FIELD,
	AC_COUNTER,
	/* "instructions" */
	AC_GOTO_TABLE,
	AC_MOVE_PACKET,
	AC_MOVE_PACKET_IMM,
	AC_CALC_R,
	AC_CALC_I,
	AC_WRITE_METADATA,
	AC_CHECKSUM,
};

enum move_direction {
	MOVE_FORWARD,
	MOVE_BACKWARD
};

struct action *action(void);
struct action *action_copy(struct action *a);
void action_add(struct action *a, enum action_type type, int arg);
void action_add_goto_table(struct action *a, int tid, int offset);
void action_add_calc_r(struct action *a, enum action_oper_type op_type,
		       enum match_field_type dst_type,
		       int dst_offset, int dst_length,
		       enum match_field_type src_type,
		       int src_offset, int src_length);
void action_add_calc_i(struct action *a, enum action_oper_type op_type,
		       enum match_field_type dst_type,
		       int dst_offset, int dst_length,
		       uint32_t src_value);
void action_add_write_metadata(struct action *a, int dst_offset, int dst_length, value_t val);
void action_add_set_field(struct action *a, int dst_offset, int dst_length, value_t val);
void action_add_add_field(struct action *a, int dst_offset, int dst_length, value_t val);
void action_add_del_field(struct action *a, int dst_offset, int dst_length);
void action_add_move_packet(struct action *a,
			    enum move_direction dir,
			    enum match_field_type type, int offset, int length);
void action_add_move_packet_imm(struct action *a, enum move_direction dir, int value);
void action_add_checksum(struct action *a, int sum_offset, int sum_length, int cal_offset, int cal_length);
void action_free(struct action *a);
int action_num_actions(struct action *a);
void action_union(struct action *a1, struct action *a2);
void action_dump(struct action *a, char *buf, int n);


/* msg */
struct msgbuf *msg_hello(void);
struct msgbuf *msg_features_request(void);
struct msgbuf *msg_set_config(int miss_send_len);
struct msgbuf *msg_get_config_request(void);
struct msgbuf *msg_counter_add(int counter_id);
struct msgbuf *msg_counter_request(int counter_id);
struct msgbuf *msg_query_all(uint16_t slotID);
struct msgbuf *msg_flow_table_add(struct flow_table *ft);
struct msgbuf *msg_flow_table_del(struct flow_table *ft);
struct msgbuf *msg_flow_entry_add(struct flow_table *ft, int index,
				  int priority, struct match *ma, struct action *a);
struct msgbuf *msg_flow_entry_del(struct flow_table *ft, int index);
struct msgbuf *msg_flow_entry_mod(struct flow_table *ft, int index,
				  int priority, struct match *ma, struct action *a);
struct msgbuf *msg_packet_out(int in_port, const uint8_t *pkt, int pkt_len, struct action *a);

#endif /* _XSWITCH_H_ */
