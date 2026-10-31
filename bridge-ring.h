// SPDX-License-Identifier: MIT
#ifndef BRIDGE_RING_H
#define BRIDGE_RING_H
#include <stdbool.h>
#include <stdint.h>
#ifndef BRIDGE_RING_SIZE
#define BRIDGE_RING_SIZE 32768u
#endif
_Static_assert(BRIDGE_RING_SIZE >= 2 && (BRIDGE_RING_SIZE & (BRIDGE_RING_SIZE - 1)) == 0,
               "ring size must be a power of two");
/* Exactly one producer and consumer. Publish bytes/slots with release/acquire;
 * no mutex in the ISR, no memmove, and every slot is usable. uint32 wrap is
 * intentional and valid while occupancy remains <= capacity. */
typedef struct {
	uint32_t head, tail;
	uint8_t bytes[BRIDGE_RING_SIZE];
} bridge_ring_t;
static inline uint32_t ring_count(const bridge_ring_t *r)
{
	uint32_t h = __atomic_load_n(&r->head, __ATOMIC_ACQUIRE);
	uint32_t t = __atomic_load_n(&r->tail, __ATOMIC_ACQUIRE);
	return h - t;
}
static inline bool ring_push(bridge_ring_t *r, uint8_t b)
{
	uint32_t h = __atomic_load_n(&r->head, __ATOMIC_RELAXED);
	if (h - __atomic_load_n(&r->tail, __ATOMIC_ACQUIRE) == BRIDGE_RING_SIZE)
		return false;
	r->bytes[h & (BRIDGE_RING_SIZE - 1)] = b;
	__atomic_store_n(&r->head, h + 1, __ATOMIC_RELEASE);
	return true;
}
static inline bool ring_pop(bridge_ring_t *r, uint8_t *b)
{
	uint32_t t = __atomic_load_n(&r->tail, __ATOMIC_RELAXED);
	if (t == __atomic_load_n(&r->head, __ATOMIC_ACQUIRE))
		return false;
	*b = r->bytes[t & (BRIDGE_RING_SIZE - 1)];
	__atomic_store_n(&r->tail, t + 1, __ATOMIC_RELEASE);
	return true;
}
#endif
