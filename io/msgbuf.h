/* Copyright (c) 2008 The Board of Trustees of The Leland Stanford
 * Junior University
 * 
 * We are making the OpenFlow specification and associated documentation
 * (Software) available for public use and benefit with the expectation
 * that others will use, modify and enhance the Software and contribute
 * those enhancements back to the community. However, since we would
 * like to make the Software available for broadest use, with as few
 * restrictions as possible permission is hereby granted, free of
 * charge, to any person obtaining a copy of this Software to deal in
 * the Software under the copyrights without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * The name and trademarks of copyright holder(s) may NOT be used in
 * advertising or publicity pertaining to the Software or any
 * derivatives without specific, written prior permission.
 */

#ifndef XPBUF_H
#define XPBUF_H 1

#include <stddef.h>

/* --- */
struct msgbuf;
struct sw;
struct msgbuf *recv_msgbuf(int cpuid);
int send_msgbuf(struct msgbuf *msg);
/* --- */

/* Buffer for holding arbitrary data.  An msgbuf is automatically reallocated
 * as necessary if it grows too large for the available memory. */
struct msgbuf {
	void *base;		/* First byte of area malloc()'d area. */
	size_t allocated;	/* Number of bytes allocated. */

	void *data;		/* First byte actually in use. */
	size_t size;		/* Number of bytes in use. */

	void *l2;		/* Link-level header. */
	void *l3;		/* Network-level header. */
	void *l4;		/* Transport-level header. */
	void *l7;		/* Application data. */

	struct msgbuf *next;	/* Next in a list of msgbufs. */
	void *private;		/* Private pointer for use by owner. */

	struct sw *sw;
};

void msgbuf_use(struct msgbuf *, void *, size_t);

void msgbuf_init(struct msgbuf *, size_t);
void msgbuf_uninit(struct msgbuf *);
void msgbuf_reinit(struct msgbuf *, size_t);

struct msgbuf *msgbuf_new(size_t);
struct msgbuf *msgbuf_clone(const struct msgbuf *);
struct msgbuf *msgbuf_clone_data(const void *, size_t);
void msgbuf_delete(struct msgbuf *);

void *msgbuf_at(const struct msgbuf *, size_t offset, size_t size);
void *msgbuf_at_assert(const struct msgbuf *, size_t offset, size_t size);
void *msgbuf_tail(const struct msgbuf *);
void *msgbuf_end(const struct msgbuf *);

void *msgbuf_put_uninit(struct msgbuf *, size_t);
void *msgbuf_put_zeros(struct msgbuf *, size_t);
void *msgbuf_put(struct msgbuf *, const void *, size_t);
void msgbuf_reserve(struct msgbuf *, size_t);
void *msgbuf_push_uninit(struct msgbuf *b, size_t);
void *msgbuf_push(struct msgbuf *b, const void *, size_t);

size_t msgbuf_headroom(struct msgbuf *);
size_t msgbuf_tailroom(struct msgbuf *);
void msgbuf_prealloc_headroom(struct msgbuf *, size_t);
void msgbuf_prealloc_tailroom(struct msgbuf *, size_t);

void msgbuf_clear(struct msgbuf *);
void *msgbuf_pull(struct msgbuf *, size_t);
void *msgbuf_try_pull(struct msgbuf *, size_t);

#endif /* msgbuf.h */
