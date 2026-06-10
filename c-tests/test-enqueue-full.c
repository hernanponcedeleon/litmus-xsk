#include <assert.h>
#include <lkmm.h>
#include "defs.h"
#include "xsk_queue.c"

int main() {
    struct xsk_queue *q = xskq_create(2, true);

    u32 prod_head, prod_next, n;
	xskq_move_prod_head(q, 1, &prod_head, &prod_next);
	xskq_move_prod_head(q, 1, &prod_head, &prod_next);
	n = xskq_move_prod_head(q, 1, &prod_head, &prod_next);
    assert(n == 0); // Ring is full

    return 0;
}