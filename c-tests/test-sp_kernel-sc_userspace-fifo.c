#include <pthread.h>
#include <assert.h>
#include <lkmm.h>
#include "defs.h"
#include "xsk_queue.c"

// This thread will behave like kernel's producer (using the new MPSC API)
void *producer_thread(void *arg) {
    struct xsk_queue *q = (struct xsk_queue *)arg;
    struct xdp_umem_ring *ring = (struct xdp_umem_ring *)q->ring;
    
    u32 prod_head, prod_next, n;
    for(int i = 0; i < 2; i++) {
        // Simplified xskq_bulk_enqueue_descs to avoid having to deal with decs and lists
        n = xskq_move_prod_head(q, 1, &prod_head, &prod_next);
        __VERIFIER_assume(n > 0);
            
        ring->desc[prod_head & q->ring_mask] = i + 1;
        xskq_update_prod_tail(&ring->ptrs, prod_head, prod_next);
    }

    return NULL;
}

// This thread will behave like userspace single consumer
// Meaning it doesn't know about the new API and uses SPSC semantics
void *consumer_thread(void *arg) {
    struct xsk_queue *q = (struct xsk_queue *)arg;
    u64 addr;

    for(int i = 0; i < 2; i++) {
        bool r = xskq_cons_peek_addr_unchecked(q, &addr);
        // Assume the ring is not empty, i.e., the producer thread already ran
        __VERIFIER_assume(r);

        // Should not read stale data
        assert(addr != 0);
        // Check FIFO
        assert(i != 0 || addr == 1);
        assert(i != 1 || addr == 2);

        xskq_cons_release(q);
    }

    return NULL;
}

int main() {
    struct xsk_queue *q = xskq_create(2, true);
    pthread_t p1_thread, c1_thread;

    pthread_create(&p1_thread, NULL, producer_thread, q);
    pthread_create(&c1_thread, NULL, consumer_thread, q);

    pthread_join(p1_thread, NULL);
    pthread_join(c1_thread, NULL);

    return 0;
}