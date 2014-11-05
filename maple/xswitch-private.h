#ifndef _XSWITCH_PRIVATE_H_
#define _XSWITCH_PRIVATE_H_
#include "xswitch.h"

struct sw;

/* xswitch */
struct trace_tree_header;
struct xswitch
{
	enum {
		XS_HELLO,
		XS_FEATURES_REPLY,
		XS_RUNNING,
	} state;
	dpid_t dpid;
	int n_ports;
	int hack_start_prio;
	//struct rconn *rconn;
	struct flow_table *table0;
	struct trace_tree_header *trace_tree;

	struct sw *sw;
};

void xswitch_up(struct xswitch *sw);
void xswitch_down(struct xswitch *sw);
void xswitch_packet_in(struct xswitch *sw, int in_port, const uint8_t *packet, int packet_len);


/* msg */
void msg_process(struct xswitch *sw, const struct msgbuf *msg);
bool msg_process_hello(const struct msgbuf *msg);
bool msg_process_features_reply(const struct msgbuf *msg, dpid_t *dpid, int *n_ports);


/* flow table */
#define MATCH_FIELD_NAME_LEN 32
#define FLOW_TABLE_NUM_FIELDS 8

struct match_field
{
	char name[MATCH_FIELD_NAME_LEN];
	enum match_field_type type;
	int offset;
	int length;
};

struct flow_table
{
	int tid;
	enum flow_table_type type;
	int size;
	int fields_num;
	struct match_field fields[FLOW_TABLE_NUM_FIELDS];
};


/* match */
struct match
{
	int fields_num;
	struct {
		int index;
		value_t value;
		value_t mask;
	} m[FLOW_TABLE_NUM_FIELDS];
};


/* action */
#define ACTION_NUM_ACTIONS 6
struct action
{
	int num_actions;
	struct {
		enum action_type type;
		int arg1;
		int arg2;
	} a[ACTION_NUM_ACTIONS];
};

#endif /* _XSWITCH_PRIVATE_H_ */
