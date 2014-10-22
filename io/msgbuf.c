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

#include "msgbuf.h"
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static inline int MAX(int a, int b)
{
	return (a > b ? a : b);
}

/* Initializes 'b' as an empty msgbuf that contains the 'allocated' bytes of
 * memory starting at 'base'.
 *
 * 'base' should ordinarily be the first byte of a region obtained from
 * malloc(), but in circumstances where it can be guaranteed that 'b' will
 * never need to be expanded or freed, it can be a pointer into arbitrary
 * memory. */
void msgbuf_use(struct msgbuf *b, void *base, size_t allocated)
{
	b->base = b->data = base;
	b->allocated = allocated;
	b->size = 0;
	b->next = NULL;
}

/* Initializes 'b' as an empty msgbuf with an initial capacity of 'size'
 * bytes. */
void msgbuf_init(struct msgbuf *b, size_t size)
{
	msgbuf_use(b, size ? malloc(size) : NULL, size);
}

/* Frees memory that 'b' points to. */
void msgbuf_uninit(struct msgbuf *b)
{
	if (b) {
		free(b->base);
	}
}

/* Frees memory that 'b' points to and allocates a new msgbuf */
void msgbuf_reinit(struct msgbuf *b, size_t size)
{
	msgbuf_uninit(b);
	msgbuf_init(b, size);
}

/* Creates and returns a new msgbuf with an initial capacity of 'size'
 * bytes. */
struct msgbuf *msgbuf_new(size_t size)
{
	struct msgbuf *b = malloc(sizeof *b);
	msgbuf_init(b, size);
	return b;
}

struct msgbuf *msgbuf_clone(const struct msgbuf *buffer)
{
	return msgbuf_clone_data(buffer->data, buffer->size);
}

struct msgbuf *msgbuf_clone_data(const void *data, size_t size)
{
	struct msgbuf *b = msgbuf_new(size);
	msgbuf_put(b, data, size);
	return b;
}

/* Frees memory that 'b' points to, as well as 'b' itself. */
void msgbuf_delete(struct msgbuf *b)
{
	if (b) {
		msgbuf_uninit(b);
		free(b);
	}
}

/* Returns the number of bytes of headroom in 'b', that is, the number of bytes
 * of unused space in msgbuf 'b' before the data that is in use.  (Most
 * commonly, the data in a msgbuf is at its beginning, and thus the msgbuf's
 * headroom is 0.) */
size_t msgbuf_headroom(struct msgbuf *b)
{
	return (char *)b->data - (char *)b->base;
}

/* Returns the number of bytes that may be appended to the tail end of msgbuf
 * 'b' before the msgbuf must be reallocated. */
size_t msgbuf_tailroom(struct msgbuf *b)
{
	return (char *)msgbuf_end(b) - (char *)msgbuf_tail(b);
}

/* Ensures that 'b' has room for at least 'size' bytes at its tail end,
 * reallocating and copying its data if necessary. */
void msgbuf_prealloc_tailroom(struct msgbuf *b, size_t size)
{
	if (size > msgbuf_tailroom(b)) {
		size_t new_allocated = b->allocated + MAX(size, 64);
		void *new_base = malloc(new_allocated);
		uintptr_t base_delta = (char *)new_base - (char *)b->base;
		memcpy(new_base, b->base, b->allocated);
		free(b->base);
		b->base = new_base;
		b->allocated = new_allocated;
		b->data = (char *)b->data + base_delta;
	}
}

void msgbuf_prealloc_headroom(struct msgbuf *b, size_t size)
{
	assert(size <= msgbuf_headroom(b));
}

/* Appends 'size' bytes of data to the tail end of 'b', reallocating and
 * copying its data if necessary.  Returns a pointer to the first byte of the
 * new data, which is left uninitialized. */
void *msgbuf_put_uninit(struct msgbuf *b, size_t size)
{
	void *p;
	msgbuf_prealloc_tailroom(b, size);
	p = msgbuf_tail(b);
	b->size += size;
	return p;
}

/* Appends 'size' zeroed bytes to the tail end of 'b'.  Data in 'b' is
 * reallocated and copied if necessary.  Returns a pointer to the first byte of
 * the data's location in the msgbuf. */
void *msgbuf_put_zeros(struct msgbuf *b, size_t size)
{
	void *dst = msgbuf_put_uninit(b, size);
	memset(dst, 0, size);
	return dst;
}

/* Appends the 'size' bytes of data in 'p' to the tail end of 'b'.  Data in 'b'
 * is reallocated and copied if necessary.  Returns a pointer to the first
 * byte of the data's location in the msgbuf. */
void *msgbuf_put(struct msgbuf *b, const void *p, size_t size)
{
	void *dst = msgbuf_put_uninit(b, size);
	memcpy(dst, p, size);
	return dst;
}

/* Reserves 'size' bytes of headroom so that they can be later allocated with
 * msgbuf_push_uninit() without reallocating the msgbuf. */
void msgbuf_reserve(struct msgbuf *b, size_t size)
{
	assert(!b->size);
	msgbuf_prealloc_tailroom(b, size);
	b->data = (char *)b->data + size;
}

void *msgbuf_push_uninit(struct msgbuf *b, size_t size)
{
	msgbuf_prealloc_headroom(b, size);
	b->data = (char *)b->data - size;
	b->size += size;
	return b->data;
}

void *msgbuf_push(struct msgbuf *b, const void *p, size_t size)
{
	void *dst = msgbuf_push_uninit(b, size);
	memcpy(dst, p, size);
	return dst;
}

/* If 'b' contains at least 'offset + size' bytes of data, returns a pointer to
 * byte 'offset'.  Otherwise, returns a null pointer. */
void *msgbuf_at(const struct msgbuf *b, size_t offset, size_t size)
{
	return offset + size <= b->size ? (char *)b->data + offset : NULL;
}

/* Returns a pointer to byte 'offset' in 'b', which must contain at least
 * 'offset + size' bytes of data. */
void *msgbuf_at_assert(const struct msgbuf *b, size_t offset, size_t size)
{
	assert(offset + size <= b->size);
	return ((char *)b->data) + offset;
}

/* Returns the byte following the last byte of data in use in 'b'. */
void *msgbuf_tail(const struct msgbuf *b)
{
	return (char *)b->data + b->size;
}

/* Returns the byte following the last byte allocated for use (but not
 * necessarily in use) by 'b'. */
void *msgbuf_end(const struct msgbuf *b)
{
	return (char *)b->base + b->allocated;
}

/* Clears any data from 'b'. */
void msgbuf_clear(struct msgbuf *b)
{
	b->data = b->base;
	b->size = 0;
}

/* Removes 'size' bytes from the head end of 'b', which must contain at least
 * 'size' bytes of data.  Returns the first byte of data removed. */
void *msgbuf_pull(struct msgbuf *b, size_t size)
{
	void *data = b->data;
	assert(b->size >= size);
	b->data = (char *)b->data + size;
	b->size -= size;
	return data;
}

/* If 'b' has at least 'size' bytes of data, removes that many bytes from the
 * head end of 'b' and returns the first byte removed.  Otherwise, returns a
 * null pointer without modifying 'b'. */
void *msgbuf_try_pull(struct msgbuf *b, size_t size)
{
	return b->size >= size ? msgbuf_pull(b, size) : NULL;
}
