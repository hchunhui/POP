#ifndef _VALUE_H_
#define _VALUE_H_
#include <inttypes.h>
#include <stdbool.h>
#include <string.h>
#define VALUE_LEN 8

typedef struct {
	uint8_t octet[6];
} haddr_t;

typedef struct {
	uint8_t v[VALUE_LEN];
} value_t;

/* value_t to haddr_t */
static inline haddr_t value_to_haddr(value_t v)
{
	haddr_t h;
	int i;
	for (i=0; i<6; i++)
		h.octet[i] = v.v[i];
	return h;
}

/* haddr_t to value_t */
static inline value_t value_from_haddr(haddr_t h)
{
	value_t v = {{0}};
	int i;
	for(i=0; i<6; i++)
		v.v[i] = h.octet[i];
	return v;
}

/* haddr_t equal */
static inline bool haddr_equal(haddr_t h1, haddr_t h2)
{
	int i;
	for (i = 0; i < 6; i++)
		if (h1.octet[i] != h2.octet[i])
			return false;
	return true;
}

/* big endian value to native integer */
static inline uint8_t value_to_8(value_t v)
{
	return v.v[0];
}

static inline uint16_t value_to_16(value_t v)
{
	return (v.v[0] << 8) | v.v[1];
}

static inline uint32_t value_to_32(value_t v)
{
	return
		(v.v[0] << 24) |
		(v.v[1] << 16) |
		(v.v[2] << 8)  |
		(v.v[3]);
}

static inline uint64_t value_to_48(value_t v)
{
	return
		(((uint64_t)v.v[0]) << 40) |
		(((uint64_t)v.v[1]) << 32) |
		(((uint64_t)v.v[2]) << 24) |
		(((uint64_t)v.v[3]) << 16) |
		(((uint64_t)v.v[4]) << 8)  |
		(((uint64_t)v.v[5]));
}

static inline uint64_t value_to_64(value_t v)
{
	return
		(((uint64_t)v.v[0]) << 56) |
		(((uint64_t)v.v[1]) << 48) |
		(((uint64_t)v.v[2]) << 40) |
		(((uint64_t)v.v[3]) << 32) |
		(((uint64_t)v.v[4]) << 24) |
		(((uint64_t)v.v[5]) << 16) |
		(((uint64_t)v.v[6]) << 8)  |
		(((uint64_t)v.v[7]));
}

/* native integer to big endian value */
static inline value_t value_from_8(uint8_t x)
{
	value_t v = {{0}};
	v.v[0] = x;
	return v;
}

static inline value_t value_from_16(uint16_t x)
{
	value_t v = {{0}};
	v.v[0] = (x >> 8) & 0xff;
	v.v[1] = x & 0xff;
	return v;
}

static inline value_t value_from_32(uint32_t x)
{
	value_t v = {{0}};
	v.v[0] = (x >> 24) & 0xff;
	v.v[1] = (x >> 16) & 0xff;
	v.v[2] = (x >> 8) & 0xff;
	v.v[3] = x & 0xff;
	return v;
}

static inline value_t value_from_48(uint64_t x)
{
	value_t v = {{0}};
	v.v[0] = (x >> 40) & 0xff;
	v.v[1] = (x >> 32) & 0xff;
	v.v[2] = (x >> 24) & 0xff;
	v.v[3] = (x >> 16) & 0xff;
	v.v[4] = (x >> 8) & 0xff;
	v.v[5] = x & 0xff;
	return v;
}

static inline value_t value_from_64(uint64_t x)
{
	value_t v = {{0}};
	v.v[0] = (x >> 56) & 0xff;
	v.v[1] = (x >> 48) & 0xff;
	v.v[2] = (x >> 40) & 0xff;
	v.v[3] = (x >> 32) & 0xff;
	v.v[4] = (x >> 24) & 0xff;
	v.v[5] = (x >> 16) & 0xff;
	v.v[6] = (x >> 8) & 0xff;
	v.v[7] = x & 0xff;
	return v;
}

/* little endian value to native integer */
static inline uint8_t value_to_8l(value_t v)
{
	return v.v[0];
}

static inline uint16_t value_to_16l(value_t v)
{
	return (v.v[1] << 8) | v.v[0];
}

static inline uint32_t value_to_32l(value_t v)
{
	return
		(v.v[3] << 24) |
		(v.v[2] << 16) |
		(v.v[1] << 8)  |
		(v.v[0]);
}

static inline uint64_t value_to_48l(value_t v)
{
	return
		(((uint64_t)v.v[5]) << 40) |
		(((uint64_t)v.v[4]) << 32) |
		(((uint64_t)v.v[3]) << 24) |
		(((uint64_t)v.v[2]) << 16) |
		(((uint64_t)v.v[1]) << 8)  |
		(((uint64_t)v.v[0]));
}

static inline uint64_t value_to_64l(value_t v)
{
	return
		(((uint64_t)v.v[7]) << 56) |
		(((uint64_t)v.v[6]) << 48) |
		(((uint64_t)v.v[5]) << 40) |
		(((uint64_t)v.v[4]) << 32) |
		(((uint64_t)v.v[3]) << 24) |
		(((uint64_t)v.v[2]) << 16) |
		(((uint64_t)v.v[1]) << 8)  |
		(((uint64_t)v.v[0]));
}

/* native integer to little endian value */
static inline value_t value_from_8l(uint8_t x)
{
	value_t v = {{0}};
	v.v[0] = x;
	return v;
}

static inline value_t value_from_16l(uint16_t x)
{
	value_t v = {{0}};
	v.v[1] = (x >> 8) & 0xff;
	v.v[0] = x & 0xff;
	return v;
}

static inline value_t value_from_32l(uint32_t x)
{
	value_t v = {{0}};
	v.v[3] = (x >> 24) & 0xff;
	v.v[2] = (x >> 16) & 0xff;
	v.v[1] = (x >> 8) & 0xff;
	v.v[0] = x & 0xff;
	return v;
}

static inline value_t value_from_48l(uint64_t x)
{
	value_t v = {{0}};
	v.v[5] = (x >> 40) & 0xff;
	v.v[4] = (x >> 32) & 0xff;
	v.v[3] = (x >> 24) & 0xff;
	v.v[2] = (x >> 16) & 0xff;
	v.v[1] = (x >> 8) & 0xff;
	v.v[0] = x & 0xff;
	return v;
}

static inline value_t value_from_64l(uint64_t x)
{
	value_t v = {{0}};
	v.v[7] = (x >> 56) & 0xff;
	v.v[6] = (x >> 48) & 0xff;
	v.v[5] = (x >> 40) & 0xff;
	v.v[4] = (x >> 32) & 0xff;
	v.v[3] = (x >> 24) & 0xff;
	v.v[2] = (x >> 16) & 0xff;
	v.v[1] = (x >> 8) & 0xff;
	v.v[0] = x & 0xff;
	return v;
}

/* extract value from a buffer */
static inline value_t value_extract(const uint8_t *buf, int offset, int length)
{
	value_t v = {{0}};
	uint8_t tmp[VALUE_LEN+1];
	int low = offset / 8;
	int high = (offset + length + 7) / 8;
	int shift = offset % 8;
	int i;
	memcpy(tmp, buf + low, high - low);
	for(i = 0; i < high - low - 1; i++) {
		v.v[i] = tmp[i];
		v.v[i] >>= shift;
		v.v[i] |= tmp[i+1] << (8 - shift);
	}
	v.v[i] = buf[low + i];
	v.v[i] >>= shift;
	if(length % 8)
		v.v[i] %= (1 << (length % 8)) - 1;
	return v;
}

/* value_t equal */
static inline bool value_equal(value_t a, value_t b)
{
	int i;
	for(i = 0; i < VALUE_LEN; i++)
		if(a.v[i] != b.v[i])
			return false;
	return true;
}

#endif /* _VALUE_H_ */
