#ifndef _XSWITCH_H_
#define _XSWITCH_H_
#include "types.h"

struct msgbuf;
struct vconn;

struct xswitch;
struct flow_table;
struct match;
struct action;

/* xswitch */
void xswitch_send(struct xswitch *sw, struct msgbuf *b);


/* flow table */
enum match_field_type { MATCH_FIELD_PACKET, MATCH_FIELD_METADATA };
enum flow_table_type { FLOW_TABLE_TYPE_MM, FLOW_TABLE_TYPE_LPM };
struct flow_table *flow_table(int tid, enum flow_table_type type, int size);
void flow_table_free(struct flow_table *ft);
void flow_table_add_field(struct flow_table *ft,
			  const char *name, enum match_field_type type, int offset, int length);
int flow_table_get_field_index(struct flow_table *ft, const char *name);
int flow_table_get_tid(struct flow_table *ft);


/* match */
struct match *match(void);
struct match *match_copy(struct match *m);
void match_add(struct match *m, int index, value_t value, value_t mask);
void match_free(struct match *m);


/* action */
enum action_type { AC_DROP, AC_PACKET_IN, AC_OUTPUT, AC_GOTO_TABLE };
struct action *action(void);
struct action *action_copy(struct action *a);
void action_add(struct action *a, enum action_type type, int arg);
void action_add2(struct action *a, enum action_type type, int arg1, int arg2);
void action_free(struct action *a);
int action_num_actions(struct action *a);
void action_union(struct action *a1, struct action *a2);
void action_dump(struct action *a, char *buf, int n);


/* msg */
struct msgbuf *msg_hello(void);
struct msgbuf *msg_features_request(void);
struct msgbuf *msg_set_config(int miss_send_len);
struct msgbuf *msg_get_config_request(void);

struct msgbuf *msg_flow_table_add(struct flow_table *ft);
struct msgbuf *msg_flow_table_del(struct flow_table *ft);
struct msgbuf *msg_flow_entry_add(struct flow_table *ft,
				 int priority, struct match *ma, struct action *a);
struct msgbuf *msg_packet_out(int in_port, const uint8_t *pkt, int pkt_len, struct action *a);

#endif /* _XSWITCH_H_ */
