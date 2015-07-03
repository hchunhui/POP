#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <assert.h>
#include "xswitch-private.h"
#include "pof_global.h"
#include "io/msgbuf.h"

const char *msg_get_pof_version(void)
{
	return POFSwitch_VERSION_STR;
}

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

static bool fill_action(struct pof_action *ma, struct action_entry *ae)
{
	struct pof_action_packet_in *ap;
	struct pof_action_drop *ad;
	struct pof_action_output *ao;
	struct pof_action_set_field *asf;
	struct pof_action_add_field *aaf;
	struct pof_action_delete_field *adf;
	struct pof_action_counter *ac;
	struct pof_action_calculate_checksum *cs;

	switch(ae->type) {
	case AC_DROP:
		ma->type = htons(POFAT_DROP);
		ma->len = htons(4 + sizeof(*ad));
		ad = (void *)(ma->action_data);
		ad->reason_code = htonl(POFR_ACTION);
		break;
	case AC_PACKET_IN:
		ma->type = htons(POFAT_PACKET_IN);
		ma->len = htons(4 + sizeof(*ap));
		ap = (void *)(ma->action_data);
		ap->reason_code = htonl(POFR_ACTION);
		break;
	case AC_OUTPUT:
		ma->type = htons(POFAT_OUTPUT);
		ma->len = htons(4 + sizeof(*ao));
		ao = (void *)(ma->action_data);
		ao->portId_type = 0;
		ao->outputPortId.value = htonl(ae->u.arg);
		ao->metadata_offset = htons(0);
		ao->metadata_len = htons(0);
		ao->packet_offset = htons(0);
		break;
	case AC_SET_FIELD:
		ma->type = htons(POFAT_SET_FIELD);
		ma->len = htons(4 + sizeof(*asf));
		asf = (void *)(ma->action_data);
		asf->field_setting.field_id = htons(0);
		asf->field_setting.offset = htons(u16(ae->u.set_field.dst_offset));
		asf->field_setting.len = htons(u16(ae->u.set_field.dst_length));
		memcpy(asf->field_setting.value, ae->u.set_field.val.v, VALUE_LEN);
		memcpy(asf->field_setting.mask, ae->u.set_field.val.v, VALUE_LEN);
		break;
	case AC_ADD_FIELD:
		ma->type = htons(POFAT_ADD_FIELD);
		ma->len = htons(4 + sizeof(*aaf));
		aaf = (void *)(ma->action_data);
		aaf->tag_id = htons(0); //?
		aaf->tag_pos = htons(u16(ae->u.add_field.dst_offset));
		aaf->tag_len = htonl(u32(ae->u.add_field.dst_length));
		memcpy(aaf->tag_value, ae->u.add_field.val.v, VALUE_LEN);
		break;
	case AC_DEL_FIELD:
		ma->type = htons(POFAT_DELETE_FIELD);
		ma->len = htons(4 + sizeof(*adf));
		adf = (void *)(ma->action_data);
		adf->tag_pos = htons(u16(ae->u.del_field.dst_offset));
		adf->len_type = 0;
		adf->tag_len.value = htonl(u32(ae->u.del_field.dst_length));
		break;
	case AC_COUNTER:
		ma->type = htons(POFAT_COUNTER);
		ma->len = htons(4 + sizeof(*ac));
		ac = (void *)(ma->action_data);
		ac->counter_id = htonl(u32(ae->u.arg));
		break;
	case AC_CHECKSUM:
		ma->type = htons(POFAT_CALCULATE_CHECKSUM);
		ma->len = htons(4 + sizeof(*cs));
		cs = (void *)(ma->action_data);
		cs->checksum_pos_type = 0;
		cs->cal_startpos_type = 0;
		cs->checksum_pos = htons(ae->u.checksum.sum_offset);
		cs->checksum_len = htons(ae->u.checksum.sum_length);
		cs->cal_startpos = htons(ae->u.checksum.cal_offset);
		cs->cal_len      = htons(ae->u.checksum.cal_length);
		break;
	default:
		return false;
	}
	return true;
}

static int fill_actions(struct pof_action *ma, int num, struct action *a)
{
	int i, idx;
	assert(a->num_actions <= num);

	idx = 0;
	for(i = 0; i < a->num_actions; i++) {
		if(fill_action(&ma[idx], &a->a[i]))
			idx++;
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
	cf->src_operand.value = htonl(imm);
}

static int fill_instructions(struct pof_instruction *mi, int num, struct action *a)
{
	bool b;
	int i, idx;
	struct pof_instruction_apply_actions *ia;
	struct pof_instruction_goto_table *gt;
	struct pof_instruction_mov_packet_offset *mpo;
	struct pof_instruction_write_metadata *wm;
	assert(a->num_actions <= num);

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
		case AC_MOVE_PACKET:
			mi[idx].type = htons(POFIT_MOVE_PACKET_OFFSET);
			mi[idx].len = htons(sizeof(*mpo));
			mpo = (void *)(mi[idx].instruction_data);
			if(a->a[i].u.move_packet.dir == MOVE_FORWARD)
				mpo->direction = 0;
			else
				mpo->direction = 1;
			mpo->value_type = 1;
			mpo->movement.field.field_id =
				htons(get_pof_match_field_id(a->a[i].u.move_packet.type));
			mpo->movement.field.offset =
				htons(u16(a->a[i].u.move_packet.offset));
			mpo->movement.field.len =
				htons(u16(a->a[i].u.move_packet.length));
			idx++;
			break;
		case AC_MOVE_PACKET_IMM:
			mi[idx].type = htons(POFIT_MOVE_PACKET_OFFSET);
			mi[idx].len = htons(sizeof(*mpo));
			mpo = (void *)(mi[idx].instruction_data);
			if(a->a[i].u.move_packet_imm.dir == MOVE_FORWARD)
				mpo->direction = 0;
			else
				mpo->direction = 1;
			mpo->value_type = 0;
			mpo->movement.value = htonl(u32(a->a[i].u.move_packet_imm.value));
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
			mi[idx].type = htons(POFIT_APPLY_ACTIONS);
			mi[idx].len = htons(8 + sizeof(*ia));
			ia = (void *)(mi[idx].instruction_data);
			ia->action_num = 1;
			b = fill_action(ia->action, &a->a[i]);
			assert(b);
			idx++;
			break;
		}
	}
	assert(idx == a->num_actions);
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

struct msgbuf *msg_query_all(uint16_t slotID)
{
	struct msgbuf *msg;
	struct pof_queryall_request *qr;
	make_pof_msg(sizeof(struct pof_header) + sizeof(struct pof_queryall_request)
		     , POFT_QUERYALL_REQUEST,
		     &msg);
	qr = GET_BODY(msg);
	qr->slotID = htons(u16(slotID));
	return msg;
}

struct msgbuf *msg_counter_add(int counter_id)
{
	struct msgbuf *msg;
	struct pof_counter *pc;
	make_pof_msg(sizeof(struct pof_header) + sizeof(struct pof_counter),
		     POFT_COUNTER_MOD,
		     &msg);
	pc = GET_BODY(msg);
	pc->command = POFCC_ADD;
#ifdef POF_MULTIPLE_SLOTS
	pc->slotID = htons(0);
#endif
	pc->counter_id = htonl(u32(counter_id));
	return msg;
}

struct msgbuf *msg_counter_request(int counter_id)
{
	struct msgbuf *msg;
	struct pof_counter *pc;
	make_pof_msg(sizeof(struct pof_header) + sizeof(struct pof_counter),
		     POFT_COUNTER_REQUEST,
		     &msg);
	pc = GET_BODY(msg);
	pc->command = POFCC_QUERY;
#ifdef POF_MULTIPLE_SLOTS
	pc->slotID = htons(0);
#endif
	pc->counter_id = htonl(u32(counter_id));
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
	mft->slotID = htons(0);
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
	mft->slotID = htons(0);
	return msg;
}

struct msgbuf *msg_flow_entry_add(struct flow_table *ft, int index,
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
	mfe->index = htonl(index);
	mfe->slotID = htons(0);

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
	mfe->instruction_num = u8(fill_instructions(mfe->instruction, POF_MAX_INSTRUCTION_NUM, a));
	return msg;
}

struct msgbuf *msg_flow_entry_del(struct flow_table *ft, int index)
{
	struct msgbuf *msg;
	struct pof_flow_entry *mfe;

	make_pof_msg(sizeof(struct pof_header) + sizeof(struct pof_flow_entry),
		     POFT_FLOW_MOD, &msg);
	mfe = GET_BODY(msg);
	mfe->command = POFFC_DELETE;

	mfe->table_id = u8(ft->tid);
	mfe->table_type = get_pof_table_type(ft->type);
	mfe->index = htonl(index);
	mfe->slotID = htons(0);
	return msg;
}

struct msgbuf *msg_flow_entry_mod(struct flow_table *ft, int index,
				  int priority, struct match *ma, struct action *a)
{
	int i;
	struct msgbuf *msg;
	struct pof_flow_entry *mfe;

	make_pof_msg(sizeof(struct pof_header) + sizeof(struct pof_flow_entry),
		     POFT_FLOW_MOD, &msg);
	mfe = GET_BODY(msg);
	mfe->command = POFFC_MODIFY;
	mfe->match_field_num = u8(ft->fields_num);
	mfe->instruction_num = 0; //set later

	mfe->table_id = u8(ft->tid);
	mfe->table_type = get_pof_table_type(ft->type);
	mfe->idle_timeout = htons(0);
	mfe->hard_timeout = htons(0);
	mfe->priority = htons(u16(priority));
	mfe->index = htonl(index);
	mfe->slotID = htons(0);

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
	mfe->instruction_num = u8(fill_instructions(mfe->instruction, POF_MAX_INSTRUCTION_NUM, a));
	return msg;
}

struct msgbuf *msg_packet_out(int in_port, const uint8_t *pkt, int pkt_len, struct action *a)
{
	struct msgbuf *msg;
	struct pof_packet_out *mpo;
	int num_actions = action_num_actions(a); //upper bound
	struct pof_action buf[num_actions];
	num_actions = fill_actions(buf, num_actions, a); //real size

	make_pof_msg(sizeof(struct pof_header) + sizeof(struct pof_packet_out) +
		     num_actions * sizeof(struct pof_action) + pkt_len,
		     POFT_PACKET_OUT,
		     &msg);
	mpo = GET_BODY(msg);
	mpo->buffer_id = htonl(0xffffffff);
#ifdef POF_MULTIPLE_SLOTS
	mpo->slotID = htons(0);
#endif
	mpo->in_port = htons(u16(in_port));
	mpo->actions_len = htons(num_actions * sizeof(struct pof_action));
	fill_actions(mpo->actions, num_actions, a);
	memcpy((mpo->actions) + num_actions, pkt, pkt_len);
	return msg;
}

static void dump_packet(const struct msgbuf *msg)
{
	uint8_t *data = msg->data;
	size_t size = msg->size;
	size_t i;

	fprintf(stderr, "dumping...\nmsg size: %zu\nmsg data:", size);
	for(i = 0; i < size; i++)
	{
		if(i%16 == 0)
			fprintf(stderr, "\n%04zx(%04zu): ", i, i);
		if(i%16 == 8)
			fprintf(stderr, "  ");
		fprintf(stderr, "%02x  ", data[i]);
	}
	fprintf(stderr, "\n----------");
	for (i = 0; i < size; i++)
	{
		if(i%16 == 0)
			fprintf(stderr, "\n%04zx(%04zu): ", i, i);
		if(i%16 == 8)
			fprintf(stderr, "  ");
		fprintf(stderr, "%03u ", data[i]);
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
	struct pof_counter *ct;
	struct msgbuf *rmsg;
	int i;
	int port_id;
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
		fprintf(stderr, "receive error packet:\n"
			"dev_id: 0x%x\n"
			"type: %hu\n"
			"code: %hu\n"
			"string: %s\n",
			ntohl(e->device_id), ntohs(e->type),
			ntohs(e->code), e->err_str);
		break;
	case POFT_GET_CONFIG_REPLY:
		sc = GET_BODY(msg);
		fprintf(stderr, "receive switch config reply:\n"
			"flags: 0x%hx  miss_send_len: %hu\n",
			ntohs(sc->flags), ntohs(sc->miss_send_len));
		break;
	case POFT_RESOURCE_REPORT:
		tr = GET_BODY(msg);
		fprintf(stderr, "receive resource report:\n"
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
#ifdef POF_MULTIPLE_SLOTS
		port_id = ntohs(ps->desc.port_id);
#else
		port_id = ntohl(ps->desc.port_id);
#endif
		fprintf(stderr, "receive port status:\n"
			" reason: %d\n"
			" port_id: 0x%x, name: %s\n"
			" of_enable: %s\n"
			" state: 0x%x\n",
			ps->reason, port_id, ps->desc.name,
			ps->desc.of_enable?"TRUE":"FALSE",
			ntohl(ps->desc.state));
		/* All ports are not Openflow(POF) enabled by default, enable it. */
		if (!ps->desc.of_enable) {
			struct pof_port_status *p;
			struct xport *xp;
			make_pof_msg(sizeof(struct pof_header) + sizeof(struct pof_port_status),
				     POFT_PORT_MOD, &rmsg);
			p = GET_BODY(rmsg);
			*p = *ps;
			p->reason = POFPR_MODIFY;
			p->desc.of_enable = POFE_ENABLE;
			xswitch_send(sw, rmsg);
			sw->n_ready_ports++;

			xp = xport_new(port_id);
			xport_insert(sw, xp);
			xport_free(xp);

			/* Are we ready? */
			if(sw->n_ready_ports == sw->n_ports)
				xswitch_up(sw);
		} else {
			if (ntohl(ps->desc.state) & POFPS_LINK_DOWN)
				xswitch_port_status(sw, port_id, PORT_DOWN);
			else
				xswitch_port_status(sw, port_id, PORT_UP);
		}
		break;
	case POFT_COUNTER_REPLY:
		ct = GET_BODY(msg);
#if 0
		fprintf(stderr, "receive counter reply from switch %d:\n"
			"counter_id: %d\n"
			"value: %"PRIu64"\n"
			"byte_value: %"PRIu64"\n",
			xswitch_get_dpid(sw),
			ntohl(ct->counter_id),
			ntohll(ct->value),
			ntohll(ct->byte_value));
#endif
		xport_update(xport_lookup(sw, (uint16_t)ntohl(ct->counter_id)),
			     ntohll(ct->value),
			     ntohll(ct->byte_value));
		break;
	case POFT_QUERYALL_FIN:
		fprintf(stderr, "receive queryall fin\n");
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
