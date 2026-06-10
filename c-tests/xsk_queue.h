/* SPDX-License-Identifier: GPL-2.0 */
/* XDP user-space ring structure
 * Copyright(c) 2018 Intel Corporation.
 */

struct xdp_ring {
	u32 producer ____cacheline_aligned_in_smp;
	u32 consumer ____cacheline_aligned_in_smp;
	u32 flags;
	u32 producer_head ____cacheline_aligned_in_smp;
	u32 consumer_head ____cacheline_aligned_in_smp;
};

/* Used for the fill and completion queues for buffers */
struct xdp_umem_ring {
	struct xdp_ring ptrs;
	u64 desc[] ____cacheline_aligned_in_smp;
};

struct xsk_queue {
	u32 ring_mask;
	u32 nentries;
	u32 cached_prod;
	u32 cached_cons;
	struct xdp_ring *ring;
	u64 invalid_descs;
	u64 queue_empty_descs;
	size_t ring_vmalloc_size;
	/* Mutual exclusion of the completion ring in the SKB mode.
	 * Protect: when sockets share a single cq when the same netdev
	 * and queue id is shared.
	 */
	spinlock_t cq_cached_prod_lock;
};

/* Functions that will be used by userspace (single consumer rings). */

static inline void __xskq_cons_read_addr_unchecked(struct xsk_queue *q, u32 cached_cons, u64 *addr)
{
	struct xdp_umem_ring *ring = (struct xdp_umem_ring *)q->ring;
	u32 idx = cached_cons & q->ring_mask;

	*addr = ring->desc[idx];
}

static inline bool xskq_cons_read_addr_unchecked(struct xsk_queue *q, u64 *addr)
{
	if (q->cached_cons != q->cached_prod) {
		__xskq_cons_read_addr_unchecked(q, q->cached_cons, addr);
		return true;
	}

	return false;
}

static inline void __xskq_cons_release(struct xsk_queue *q)
{
	smp_store_release(&q->ring->consumer, q->cached_cons); /* D, matches A */
}

static inline void __xskq_cons_peek(struct xsk_queue *q)
{
	/* Refresh the local pointer */
	q->cached_prod = smp_load_acquire(&q->ring->producer);  /* C, matches B */
}

static inline void xskq_cons_get_entries(struct xsk_queue *q)
{
	__xskq_cons_release(q);
	__xskq_cons_peek(q);
}

static inline bool xskq_cons_peek_addr_unchecked(struct xsk_queue *q, u64 *addr)
{
	if (q->cached_prod == q->cached_cons)
		xskq_cons_get_entries(q);
	return xskq_cons_read_addr_unchecked(q, addr);
}

static inline void xskq_cons_release(struct xsk_queue *q)
{
	q->cached_cons++;
}

/* Functions for MPSC operations */

static inline u32 xskq_move_prod_head(struct xsk_queue *q, u32 n, u32 *old_head,
				      u32 *new_head)
{
	const u32 capacity = q->nentries;
	u32 free_entries, max = n;
	int success;

	struct xdp_ring *ring = (struct xdp_ring *)q->ring;

	do {
		n = max;

		*old_head = smp_load_acquire(&ring->producer_head);

		/* The subtraction is done between two unsigned 32bits value.
		 * So 'free_entries' is always between and capacity (which is < size).
		 */
		free_entries = (capacity + READ_ONCE(ring->consumer) - *old_head);
		if (unlikely(n > free_entries))
			n = free_entries;

		if (n == 0)
			return 0;

		*new_head = *old_head + n;
		success = (cmpxchg(&ring->producer_head, *old_head, *new_head) == *old_head);
	} while (unlikely(success == 0));

	return n;
}

static inline void xskq_update_prod_tail(struct xdp_ring *ring, u32 old_val, u32 new_val)
{
	while (READ_ONCE(ring->producer) != old_val)
		cpu_relax();

	smp_store_release(&ring->producer, new_val);
}

/* Can be used by kernel to bulk enqueue descs to rx ring */
static inline u32 xskq_bulk_enqueue_descs(struct xsk_queue *q, struct list_head *rx_list,
					  u32 rx_list_cnt)
{
	u32 prod_head, prod_next, idx, n;
	// struct xdp_desc desc;
	// struct xdp_buff_xsk *xskb;

	struct xdp_rxtx_ring *ring = (struct xdp_rxtx_ring *)q->ring;

	n = xskq_move_prod_head(q, rx_list_cnt, &prod_head, &prod_next);

	/* Ring full */
	if (n == 0)
		return 0;

	idx = prod_head & q->ring_mask;

	for (u32 i = 0; i < n; i++) {
		// xskb = list_first_entry(rx_list, struct xdp_buff_xsk, list_node);
		// list_del_init(&xskb->list_node);

		// desc.addr = xp_get_handle(xskb, xskb->pool);
		// desc.len = xskb->len;
		// desc.options = xskb->flags;

		// ring->desc[idx] = desc;
		// idx = ((idx + 1) & q->ring_mask);

		// xp_release(xskb);
	}

	xskq_update_prod_tail((struct xdp_ring *)ring, prod_head, prod_next);

	return n;
}

/* Functions for SPMC operations */

static inline u32 xskq_move_cons_head(struct xsk_queue *q, u32 n, u32 *old_head,
				      u32 *new_head)
{
	u32 entries, max = n;
	int success;

	struct xdp_ring *ring = (struct xdp_ring *)q->ring;

	do {
		n = max;

		*old_head = READ_ONCE(ring->consumer_head);

		/* The subtraction is done between two unsigned 32bits value.
		 * So 'free_entries' is always between and capacity (which is < size).
		 */
		entries = READ_ONCE(ring->producer) - *old_head;
		if (n > entries)
			n = entries;

		if (unlikely(n == 0))
			return 0;

		*new_head = *old_head + n;
		success = (cmpxchg(&ring->consumer_head, *old_head, *new_head) == *old_head);
	} while (unlikely(success == 0));

	return n;
}

static inline void xskq_update_cons_tail(struct xdp_ring *ring, u32 old_val, u32 new_val)
{
	while (READ_ONCE(ring->consumer) != old_val)
		cpu_relax();

	smp_store_release(&ring->consumer, new_val);
}

/* Used by driver to find available entries during batching */
static inline u32 xskq_cons_available_entries(struct xsk_queue *q, u32 max)
{
	struct xdp_ring *ring = (struct xdp_ring *)q->ring;
	u32 entries = READ_ONCE(ring->producer) - READ_ONCE(ring->consumer_head);

	return entries >= max ? max : entries;
}

/* Can be used by kernel to dequeue an addr from fq/cq ring.
 * The batched variant for the same is present in xsk_buff_pool.c
 * used for dequing from fq - `xp_alloc_new_from_fq()`
 */
static inline bool xskq_dequeue_addr(struct xsk_queue *q, u64 *addr)
{
	u32 cons_head, cons_next, idx, n;
	struct xdp_umem_ring *ring = (struct xdp_umem_ring *)q->ring;

	n = xskq_move_cons_head(q, 1, &cons_head, &cons_next);

	/* Ring empty */
	if (n == 0)
		return false;

	/* A, matches D */
	idx = cons_head & q->ring_mask;
	*addr = ring->desc[idx];

	xskq_update_cons_tail((struct xdp_ring *)ring, cons_head, cons_next);

	return true;
}
