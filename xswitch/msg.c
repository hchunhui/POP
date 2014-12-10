#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <assert.h>
#include "xswitch-private.h"
#include "pof/pof_global.h"
#include "io/msgbuf.h"

/* helper functions */
static inline uint32_t alloc_xid(void)
{
	static __thread uint32_t xid;
	return ++xid;
}

/* Same as put_pof_msg(), except for a transaction id 'xid'. */
static void *put_pof_msg_xid(size_t length, uint8_t type, uint32_t xid,
			     struct msgbuf *buffer)
{
	struct pof_header *ph;
	assert(length >= sizeof *ph);
	assert(length <= UINT16_MAX);

	ph = msgbuf_put_uninit(buffer, length);
	ph->version = POF_VERSION;
	ph->type = type;
	ph->length = htons(length);
	ph->xid = xid;
	memset(ph + 1, 0, length - sizeof *ph);
	return ph;
}

/* Appends 'length' bytes to 'buffer', starting with an POF header
 * with the given 'type' and an arbitrary transaction id.  Allocated bytes
 * beyond the header, if any, are zeroed.
 *
 * The POF header length is initially set to 'length'; if the
 * message is later extended, the length should be updated with
 * update_pof_msg_length() before sending.
 *
 * Returns the header. */
#if 0
static void *put_pof_msg(size_t length, uint8_t type, struct msgbuf *buffer)
{
	return put_pof_msg_xid(length, type, alloc_xid(), buffer);
}
#endif

/* Same as make_pof_msg(), except for a transaction id 'xid'. */
static void *make_pof_msg_xid(size_t length, uint8_t type, uint32_t xid,
			      struct msgbuf **bufferp)
{
	*bufferp = msgbuf_new(length);
	return put_pof_msg_xid(length, type, xid, *bufferp);
}

/* Allocates and stores in '*bufferp' a new msgbuf with a size of
 * 'length', starting with an POF header with the given 'type' and
 * an arbitrary transaction id.  Allocated bytes beyond the header, if any, are
 * zeroed.
 *
 * The caller is responsible for freeing '*bufferp' when it is no longer
 * needed.
 *
 * The POF header length is initially set to 'length'; if the
 * message is later extended, the length should be updated with
 * update_pof_msg_length() before sending.
 *
 * Returns the header. */
static void *make_pof_msg(size_t length, uint8_t type, struct msgbuf **bufferp)
{
	*bufferp = msgbuf_new(length);
	return put_pof_msg_xid(length, type, alloc_xid(), *bufferp);
}

static struct msgbuf *make_echo_reply(uint32_t xid)
{
	struct msgbuf *msg;
	make_pof_msg_xid(sizeof(struct pof_header),
			 POFT_ECHO_REPLY,
			 xid,
			 &msg);
	return msg;
}

static uint8_t u8(int x)
{
	assert(x >= 0 && x < 256);
	return (uint8_t) x;
}

static uint16_t u16(int x)
{
	assert(x >= 0 && x < 65536);
	return (uint16_t) x;
}

static uint32_t u32(int x)
{
	assert(x >= 0);
	return (uint32_t) x;
}

static uint8_t get_pof_table_type(enum flow_table_type type)
{
	switch(type) {
	case FLOW_TABLE_TYPE_MM:
		return POF_MM_TABLE;
	case FLOW_TABLE_TYPE_LPM:
	        return POF_LPM_TABLE;
	}
	abort();
}

static uint16_t get_pof_match_field_id(enum match_field_type type)
{
	switch(type) {
	case MATCH_FIELD_PACKET:
		return 0;
	case MATCH_FIELD_METADATA:
		return 0xffff;
	}
	abort();
}

static void fill_match(struct pof_match *match, struct match_field *field)
{
	match->field_id = htons(get_pof_match_field_id(field->type));
	match->offset = htons(u16(field->offset));
	match->len = htons(u16(field->length));
}

static int fill_action(struct pof_action *ma, int num, struct action *a)
{
	struct pof_action_packet_in *ap;
	struct pof_action_drop *ad;
	struct pof_action_output *ao;
	int i, idx;
	assert(a->num_actions <= num);

	idx = 0;
	for(i = 0; i < a->num_actions; i++) {
		switch(a->a[i].type) {
		case AC_DROP:
			ma[idx].type = htons(POFAT_DROP);
			ma[idx].len = htons(4 + sizeof(*ad));
			ad = (void *)(ma[idx].action_data);
			ad->reason_code = htonl(POFR_ACTION);
			idx++;
			break;
		case AC_PACKET_IN:
			ma[idx].type = htons(POFAT_PACKET_IN);
			ma[idx].len = htons(4 + sizeof(*ap));
			ap = (void *)(ma[idx].action_data);
			ap->reason_code = htonl(POFR_ACTION);
			idx++;
			break;
		case AC_OUTPUT:
			ma[idx].type = htons(POFAT_OUTPUT);
			ma[idx].len = htons(4 + sizeof(*ao));
			ao = (void *)(ma[idx].action_data);
			ao->portId_type = 0;
			ao->outputPortId.value = htonl(a->a[i].u.arg);
			ao->metadata_offset = htons(0);
			ao->metadata_len = htons(0);
			ao->packet_offset = htons(0);
			idx++;
			break;
		default:
			break;
		}
	}
	return idx;
}

static enum pof_calc_type get_pof_calc_type(enum action_oper_type type)
{
	switch(type) {
	case AC_OP_ADD:
		return POFCT_ADD;
	case AC_OP_SUB:
		return POFCT_SUBTRACT;
	case AC_OP_AND:
		return POFCT_BITWISE_ADD; /* Why "ADD"?? */
	case AC_OP_OR:
		return POFCT_BITWISE_OR;
	case AC_OP_SHL:
		return POFCT_LEFT_SHIFT;
	case AC_OP_SHR:
		return POFCT_RIGHT_SHIFT;
	case AC_OP_XOR:
		return POFCT_BITWISE_XOR;
	case AC_OP_NOR:
		return POFCT_BITWISE_NOR;
	}
	abort();
}

static void emit_calc_r(struct pof_instruction *mi, enum action_oper_type type,
			enum match_field_type dst_type, int dst_offset, int dst_length,
			enum match_field_type src_type, int src_offset, int src_length)
{
	struct pof_instruction_calc_field *cf;
	mi->type = htons(POFIT_CALCULATE_FIELD);
	mi->len = htons(sizeof(*cf));
	cf = (void *)(mi->instruction_data);
	cf->calc_type = htons(get_pof_calc_type(type));
	cf->src_type = 1;
	cf->dst_field.field_id = htons(get_pof_match_field_id(dst_type));
	cf->dst_field.offset = htons(u16(dst_offset));
	cf->dst_field.len = htons(u16(dst_length));
	cf->src_operand.src_field.field_id = htons(get_pof_match_field_id(src_type));
	cf->src_operand.src_field.offset = htons(u16(src_offset));
	cf->src_operand.src_field.len = htons(u16(src_length));
}

static void emit_calc_i(struct pof_instruction *mi, enum action_oper_type type,
			enum match_field_type dst_type, int dst_offset, int dst_length,
			uint32_t imm)
{
	struct pof_instruction_calc_field *cf;
	mi->type = htons(POFIT_CALCULATE_FIELD);
	mi->len = htons(sizeof(*cf));
	cf = (void *)(mi->instruction_data);
	cf->calc_type = htons(get_pof_calc_type(type));
	cf->src_type = 0;
	cf->dst_field.field_id = htons(get_pof_match_field_id(dst_type));
	cf->dst_field.offset = htons(u16(dst_offset));
	cf->dst_field.len = htons(u16(dst_length));
	cf->src_operand.value = imm;
}

static int fill_instruction(struct pof_instruction *mi, int num, struct action *a)
{
	int i, idx;
	struct pof_instruction_apply_actions *ia;
	struct pof_instruction_goto_table *gt;
	struct pof_instruction_mov_packet_offset *mpo;
	struct pof_instruction_write_metadata *wm;
	assert(a->num_actions <= num);

	/* XXX: hack */
	/* before actions */
	idx = 0;
	for(i = 0; i < a->num_actions; i++) {
		switch(a->a[i].type) {
		case AC_CALC_R:
			emit_calc_r(&(mi[idx]), a->a[i].u.op_r.op_type,
				    a->a[i].u.op_r.dst_type,
				    a->a[i].u.op_r.dst_offset,
				    a->a[i].u.op_r.dst_length,
				    a->a[i].u.op_r.src_type,
				    a->a[i].u.op_r.src_offset,
				    a->a[i].u.op_r.src_length);
			idx++;
			break;
		case AC_CALC_I:
			emit_calc_i(&(mi[idx]), a->a[i].u.op_i.op_type,
				    a->a[i].u.op_i.dst_type,
				    a->a[i].u.op_i.dst_offset,
				    a->a[i].u.op_i.dst_length,
				    a->a[i].u.op_i.src_value);
			idx++;
			break;
		case AC_WRITE_METADATA:
			mi[idx].type = htons(POFIT_WRITE_METADATA);
			mi[idx].len = htons(sizeof(*wm));
			wm = (void *)(mi[idx].instruction_data);
			wm->metadata_offset = htons(u16(a->a[i].u.write_metadata.dst_offset));
			wm->len = htons(u16(a->a[i].u.write_metadata.dst_length));
			assert(VALUE_LEN <= POF_MAX_FIELD_LENGTH_IN_BYTE);
			memcpy(wm->value, a->a[i].u.write_metadata.val.v, VALUE_LEN);
			idx++;
			break;
		default:
			break;
		}
	}
	// fill action first
	mi[idx].type = htons(POFIT_APPLY_ACTIONS);
	mi[idx].len = htons(8 + sizeof(*ia));
	ia = (void *)(mi[idx].instruction_data);
	ia->action_num = u8(fill_action(ia->action, POF_MAX_ACTION_NUMBER_PER_INSTRUCTION, a));
	idx++;
	for(i = 0; i < a->num_actions; i++) {
		switch(a->a[i].type) {
		case AC_MOVE_PACKET:
			mi[idx].type = htons(POFIT_MOVE_PACKET_OFFSET);
			mi[idx].len = htons(sizeof(*mpo));
			mpo = (void *)(mi[idx].instruction_data);
			mpo->direction = 0;
			mpo->value_type = 1;
			mpo->movement.field.field_id =
				htons(get_pof_match_field_id(a->a[i].u.move_packet.type));
			mpo->movement.field.offset =
				htons(u16(a->a[i].u.move_packet.offset));
			mpo->movement.field.len =
				htons(u16(a->a[i].u.move_packet.length));
			idx++;
			break;
		case AC_GOTO_TABLE:
			mi[idx].type = htons(POFIT_GOTO_TABLE);
			mi[idx].len = htons(sizeof(*gt));
			gt = (void *)(mi[idx].instruction_data);
			gt->next_table_id = u8(a->a[i].u.goto_table.tid);
			gt->match_field_num = 0; //?
			gt->packet_offset = htons(u16(a->a[i].u.goto_table.offset));
			idx++;
			break;
		default:
			break;
		}
	}
	assert(ia->action_num + idx - 1 == a->num_actions);
	return idx;
}

/* export functions */
struct msgbuf *msg_hello(void)
{
	struct msgbuf *msg;
	make_pof_msg(sizeof(struct pof_header), POFT_HELLO, &msg);
	return msg;
}

struct msgbuf *msg_features_request(void)
{
	struct msgbuf *msg;
	make_pof_msg(sizeof(struct pof_header), POFT_FEATURES_REQUEST, &msg);
	return msg;
}

struct msgbuf *msg_set_config(int miss_send_len)
{
	struct msgbuf *msg;
	struct pof_switch_config *sc;
	make_pof_msg(sizeof(struct pof_header) + sizeof(struct pof_switch_config),
		     POFT_SET_CONFIG,
		     &msg);
	sc = GET_BODY(msg);
	sc->flags = htons(POFC_FRAG_NORMAL);
	sc->miss_send_len = htons(u16(miss_send_len));
	return msg;
}

struct msgbuf *msg_get_config_request(void)
{
	struct msgbuf *msg;
	make_pof_msg(sizeof(struct pof_header), POFT_GET_CONFIG_REQUEST, &msg);
	return msg;
}

struct msgbuf *msg_flow_table_add(struct flow_table *ft)
{
	int i;
	struct msgbuf *msg;
	struct pof_flow_table *mft;
	int key_len;
	make_pof_msg(sizeof(struct pof_header) + sizeof(struct pof_flow_table),
		     POFT_TABLE_MOD,
		     &msg);
	mft = GET_BODY(msg);
	mft->command = POFTC_ADD;
	mft->tid = u8(ft->tid);
	mft->type = get_pof_table_type(ft->type);
	mft->match_field_num = u8(ft->fields_num);
	mft->size = htonl(u32(ft->size));
	snprintf(mft->table_name, POF_NAME_MAX_LENGTH, "ft%u", ft->tid);
	key_len = 0;
	for(i = 0; i < ft->fields_num; i++) {
		fill_match(&(mft->match[i]), &(ft->fields[i]));
		key_len += ft->fields[i].length;
	}
	mft->key_len = htons(u16(key_len));
	fprintf(stderr, "----key_len: %d, fields_num: %d\n", key_len, ft->fields_num);
	return msg;
}

struct msgbuf *msg_flow_table_del(struct flow_table *ft)
{
	struct msgbuf *msg;
	struct pof_flow_table *mft;
	make_pof_msg(sizeof(struct pof_header) + sizeof(struct pof_flow_table),
		     POFT_TABLE_MOD,
		     &msg);
	mft = GET_BODY(msg);
	mft->command = POFTC_DELETE;
	mft->tid = u8(ft->tid);
	mft->type = get_pof_table_type(ft->type);
	return msg;
}

struct msgbuf *msg_flow_entry_add(struct flow_table *ft,
				 int priority, struct match *ma, struct action *a)
{
	int i;
	struct msgbuf *msg;
	struct pof_flow_entry *mfe;

	make_pof_msg(sizeof(struct pof_header) + sizeof(struct pof_flow_entry),
		     POFT_FLOW_MOD, &msg);
	mfe = GET_BODY(msg);
	mfe->command = POFFC_ADD;
	mfe->match_field_num = u8(ft->fields_num);
	mfe->instruction_num = 0; //set later

	mfe->table_id = u8(ft->tid);
	mfe->table_type = get_pof_table_type(ft->type);
	mfe->idle_timeout = htons(0);
	mfe->hard_timeout = htons(0);
	mfe->priority = htons(u16(priority));
	mfe->index = htonl(0); //??

	for(i = 0; i < ft->fields_num; i++) {
		/* assume pof_match and pof_match_x is compatible */
		fill_match((struct pof_match *)&(mfe->match[i]), &(ft->fields[i]));
		memset(mfe->match[i].value, 0, POF_MAX_FIELD_LENGTH_IN_BYTE);
		memset(mfe->match[i].mask, 0, POF_MAX_FIELD_LENGTH_IN_BYTE);
	}
	for(i = 0; i < ma->fields_num; i++) {
		int idx = flow_table_get_field_index(ft, ma->m[i].name);
		assert(idx >= 0);
		int bytes = (ft->fields[idx].length + 7) / 8;
		memcpy(mfe->match[idx].value,
		       ma->m[i].value.v,
		       bytes);
		memcpy(mfe->match[idx].mask,
		       ma->m[i].mask.v,
		       bytes);
	}
	mfe->instruction_num = u8(fill_instruction(mfe->instruction, POF_MAX_INSTRUCTION_NUM, a));
	return msg;
}

struct msgbuf *msg_packet_out(int in_port, const uint8_t *pkt, int pkt_len, struct action *a)
{
	struct msgbuf *msg;
	struct pof_packet_out *mpo;
	int num_actions = action_num_actions(a); //upper bound
	struct pof_action buf[num_actions];
	num_actions = fill_action(buf, num_actions, a); //real size

	make_pof_msg(sizeof(struct pof_header) + sizeof(struct pof_packet_out) +
		     num_actions * sizeof(struct pof_action) + pkt_len,
		     POFT_PACKET_OUT,
		     &msg);
	mpo = GET_BODY(msg);
	mpo->buffer_id = htonl(0xffffffff);
	mpo->in_port = htons(u16(in_port));
	mpo->actions_len = htons(num_actions * sizeof(struct pof_action));
	fill_action(mpo->actions, num_actions, a);
	memcpy((mpo->actions) + num_actions, pkt, pkt_len);
	return msg;
}

static void dump_packet(const struct msgbuf *msg)
{
	uint8_t *data = msg->data;
	size_t size = msg->size;
	size_t i;
	fprintf(stderr, "dumping...\n");
	for(i = 0; i < size; i++)
	{
		if(i%16 == 0)
			fprintf(stderr, "\n");
		fprintf(stderr, "%02x ", data[i]);
	}
	fprintf(stderr, "\n----------\n");
}

void msg_process(struct xswitch *sw, const struct msgbuf *msg)
{
	struct pof_header *oh;
	struct pof_error *e;
	struct pof_switch_config *sc;
	struct pof_flow_table_resource *tr;
	struct pof_port_status *ps;
	struct pof_packet_in *pi;
	struct msgbuf *rmsg;
	int i;
	oh = GET_HEAD(msg);
	switch(oh->type) {
	case POFT_ECHO_REQUEST:
		rmsg = make_echo_reply(oh->xid);
		xswitch_send(sw, rmsg);
		break;
	case POFT_PACKET_IN:
		pi = GET_BODY(msg);
		xswitch_packet_in(sw, ntohs(pi->port_id), (uint8_t *)pi->data, ntohs(pi->total_len));
		break;
	case POFT_FLOW_REMOVED:
		fprintf(stderr, "flow removed\n");
		break;
	case POFT_ERROR:
		e = GET_BODY(msg);
		fprintf(stderr, "recive error packet:\n"
			"dev_id: 0x%x\n"
			"type: %hu\n"
			"code: %hu\n"
			"string: %s\n",
			ntohl(e->device_id), ntohs(e->type),
			ntohs(e->code), e->err_str);
		break;
	case POFT_GET_CONFIG_REPLY:
		sc = GET_BODY(msg);
		fprintf(stderr, "recive switch config reply:\n"
			"flags: 0x%hx  miss_send_len: %hu\n",
			ntohs(sc->flags), ntohs(sc->miss_send_len));
		break;
	case POFT_RESOURCE_REPORT:
		tr = GET_BODY(msg);
		fprintf(stderr, "recive resource report:\n"
			"counter_num: %u\n"
			"meter_num: %u\n"
			"group_num: %u\n",
			ntohl(tr->counter_num),
			ntohl(tr->meter_num),
			ntohl(tr->group_num));
		for(i = 0; i < POF_MAX_TABLE_TYPE; i++) {
			fprintf(stderr, "table %d, type %d\n", i, tr->tbl_rsc_desc[i].type);
			fprintf(stderr, " tbl_num: %d, key_len: %d, total_size: %u\n",
				tr->tbl_rsc_desc[i].tbl_num,
				ntohs(tr->tbl_rsc_desc[i].key_len),
				ntohl(tr->tbl_rsc_desc[i].total_size));
		}
		break;
	case POFT_PORT_STATUS:
		ps = GET_BODY(msg);
		fprintf(stderr, "recive port status:\n"
			" reason: %d\n"
			" port_id: 0x%x, name: %s\n"
			" of_enable: %s\n"
			" state: 0x%x\n",
			ps->reason, ntohl(ps->desc.port_id), ps->desc.name,
			ps->desc.of_enable?"TRUE":"FALSE",
			ntohl(ps->desc.state));
		/* All ports are not Openflow(POF) enabled by default, enable it. */
		if (!ps->desc.of_enable) {
			struct pof_port_status *p;
			make_pof_msg(sizeof(struct pof_header) + sizeof(struct pof_port_status),
				     POFT_PORT_MOD, &rmsg);
			p = GET_BODY(rmsg);
			*p = *ps;
			p->reason = POFPR_MODIFY;
			p->desc.of_enable = POFE_ENABLE;
			xswitch_send(sw, rmsg);
			sw->n_ready_ports++;

			/* Are we ready? */
			if(sw->n_ready_ports == sw->n_ports)
				xswitch_up(sw);
		}
		// handle_port_status(uint32_t state, uint32_t port_id);
		if (ps->desc.state == POFPS_LINK_DOWN)
			xswitch_port_down(sw, ps->desc.port_id);
		break;
	default:
		fprintf(stderr, "POF packet ignored\n");
		dump_packet(msg);
	}
}

bool msg_process_hello(const struct msgbuf *msg)
{
	if(GET_HEAD(msg)->type == POFT_HELLO) {
		if(GET_HEAD(msg)->version != POF_VERSION) {
			fprintf(stderr, "POF version mismatch!\n");
			abort();
		}
		return true;
	}
	return false;
}

bool msg_process_features_reply(const struct msgbuf *msg, dpid_t *dpid, int *n_ports)
{
	if(GET_HEAD(msg)->type == POFT_FEATURES_REPLY) {
		struct pof_switch_features *sf = GET_BODY(msg);
		*dpid = ntohl(sf->dev_id);
		*n_ports = ntohs(sf->port_num);
		fprintf(stderr,
			"fetures reply ...\n"
			"dpid 0x%x\n""ports %d\n",
			*dpid, *n_ports);
		return true;
	} else {
		fprintf(stderr, "ignore packet...\n");
		return false;
	}
}
