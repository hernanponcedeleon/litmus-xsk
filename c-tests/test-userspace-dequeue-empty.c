#include <assert.h>
#include <lkmm.h>
#include "defs.h"
#include "xsk_queue.c"

int main() {
    struct xsk_queue *q = xskq_create(2, true);

    u64 addr;
    bool r = xskq_cons_peek_addr_unchecked(q, &addr);
    assert(!r); // Ring is empty

    return 0;
}