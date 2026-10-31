#include "../bridge-ring.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
static bridge_ring_t ring;
static void *produce(void *unused)
{
	(void)unused;
	for (uint32_t i = 0; i < 2000000; i++)
		while (!ring_push(&ring, (uint8_t)i)) {
		}
	return NULL;
}
int main(void)
{
	uint8_t b;
	assert(!ring_pop(&ring, &b));
	for (uint32_t i = 0; i < BRIDGE_RING_SIZE; i++)
		assert(ring_push(&ring, (uint8_t)i));
	assert(!ring_push(&ring, 0));
	for (uint32_t i = 0; i < BRIDGE_RING_SIZE; i++) {
		assert(ring_pop(&ring, &b));
		assert(b == (uint8_t)i);
	}
	ring.head = UINT32_MAX - 10;
	ring.tail = ring.head;
	for (int i = 0; i < 100; i++) {
		assert(ring_push(&ring, (uint8_t)i));
		assert(ring_pop(&ring, &b));
		assert(b == (uint8_t)i);
	}
	pthread_t thread;
	assert(!pthread_create(&thread, NULL, produce, NULL));
	for (uint32_t i = 0; i < 2000000; i++) {
		while (!ring_pop(&ring, &b)) {
		}
		assert(b == (uint8_t)i);
	}
	pthread_join(thread, NULL);
	assert(ring_count(&ring) == 0);
	puts("ring ordering/full/empty/wrap/concurrency: PASS (2M bytes)");
}
