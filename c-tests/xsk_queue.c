#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include "xsk_queue.h"

// Mocked
static size_t xskq_get_ring_size(struct xsk_queue *q, bool umem_queue)
{
    assert(umem_queue);
    size_t base_size = sizeof(struct xsk_queue);
    size_t element_size = sizeof(uint64_t);
    if (q->nentries > 0 && (SIZE_MAX / q->nentries) < element_size) {
        return 0;
    }
    return base_size + (q->nentries * element_size);
}

// Identical to kernel
struct xsk_queue *xskq_create(u32 nentries, bool umem_queue)
{
	struct xsk_queue *q;
	size_t size;

	q = kzalloc_obj(*q);
	if (!q)
		return NULL;

	q->nentries = nentries;
	q->ring_mask = nentries - 1;

	size = xskq_get_ring_size(q, umem_queue);

	/* size which is overflowing or close to SIZE_MAX will become 0 in
	 * PAGE_ALIGN(), checking SIZE_MAX is enough due to the previous
	 * is_power_of_2(), the rest will be handled by vmalloc_user()
	 */
	if (unlikely(size == SIZE_MAX)) {
		kfree(q);
		return NULL;
	}

	size = PAGE_ALIGN(size);

	q->ring = vmalloc_user(size);
	if (!q->ring) {
		kfree(q);
		return NULL;
	}

	q->ring_vmalloc_size = size;
	return q;
}
