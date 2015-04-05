/*
 * Make our system as a POX application...
 */

#include <stdio.h>
#include <Python.h>

#include "param.h"
#include "io/msgbuf.h"

#include "xswitch/xswitch-private.h"

#define __unused __attribute__((unused))

extern const char *msg_get_pof_version(void);
extern void xswitch_init(void);
extern void xswitch_on_timeout(void);

/* XXX */
/* msgbuf queue: serialize xswitch_send() requests */
static struct msgbuf *mb_h, *mb_t;
/* in_xmit: ensure there's only one coroutine runs xmit() */
static bool in_xmit;

static void xmit(void)
{
	if(in_xmit)
		return;

	in_xmit = true;
	while(mb_h) {
		struct msgbuf *b = mb_h;
		PyObject *obj = b->sw;
		PyObject *arglist = Py_BuildValue("(s#)", b->data, b->size);

		/* dequeue before switch */
		mb_h = mb_h->next;
		if(obj) {
			/* switch point */
			PyObject_CallObject(obj, arglist);
		}
		Py_DECREF(arglist);
		msgbuf_delete(b);
	}
	in_xmit = false;
}

int send_msgbuf(struct msgbuf *b)
{
	if(mb_h) {
		b->next = NULL;
		mb_t->next = b;
		mb_t = b;
	} else {
		mb_h = mb_t = b;
	}
	return 0;
}

static PyObject *
packet_in(PyObject *self __unused, PyObject *args)
{
	void *sw;
	const uint8_t *_pkt;
	uint8_t pkt[MSGBUF_MAX_LENGTH];
	int port_id, pkt_len;

	PyArg_ParseTuple(args, "kis#", &sw, &port_id, &_pkt, &pkt_len);
	memcpy(pkt, _pkt, pkt_len);
	xswitch_packet_in(sw, port_id, pkt, pkt_len);
	xmit();
	return Py_BuildValue("");
}

static PyObject *
port_status(PyObject *self __unused, PyObject *args)
{
	void *sw;
	int port_id, down;
	PyArg_ParseTuple(args, "kii", &sw, &port_id, &down);
	xswitch_port_status(sw, port_id, down ? PORT_DOWN : PORT_UP);
	xmit();
	return Py_BuildValue("");
}

static PyObject *
going_up(PyObject *self __unused, PyObject *args)
{
	uint32_t dpid;
	int n_ports;
	struct xswitch *sw;
	PyObject *obj;

	PyArg_ParseTuple(args, "IiO", &dpid, &n_ports, &obj);
	Py_INCREF(obj);
	sw = xswitch(dpid, n_ports, obj);
	xmit();
	return Py_BuildValue("k", sw);
}

static PyObject *
going_down(PyObject *self __unused, PyObject *args)
{
	struct xswitch *sw;
	struct msgbuf *b;

	PyArg_ParseTuple(args, "k", &sw);
	/* delete dangling references */
	for(b = mb_h; b; b = b->next) {
		if(b->sw == sw->conn)
			b->sw = NULL;
	}
	Py_DECREF(sw->conn);
	sw->conn = NULL;
	xswitch_free(sw);
	xmit();
	return Py_BuildValue("");
}

static PyObject *
timeout(PyObject *self __unused, PyObject *args __unused)
{
	xswitch_on_timeout();
	xmit();
	return Py_BuildValue("");
}

static PyMethodDef methods[] = {
	{ "going_up", going_up, METH_VARARGS, "" },
	{ "going_down", going_down, METH_VARARGS, "" },
	{ "packet_in", packet_in, METH_VARARGS, "" },
	{ "port_status", port_status, METH_VARARGS, "" },
	{ "timeout", timeout, METH_VARARGS, "" },
	{ NULL, NULL, 0, NULL }
};

PyMODINIT_FUNC
initpofmaple_pox(void)
{
	Py_InitModule("pofmaple_pox", methods);
	mb_h = NULL;
	in_xmit = false;
	fprintf(stderr, "POF Version: %s\n", msg_get_pof_version());
	xswitch_init();
	xmit();
}
