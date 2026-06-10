typedef uint32_t u32;
typedef uint64_t u64;
typedef _Bool bool;

#define ____cacheline_aligned_in_smp

#define true 1
#define false 0
#define unlikely(X) (X)
#define PAGE_ALIGN(X) (X)
#define cpu_relax() do {} while(0)
#define kzalloc_obj(X) malloc(sizeof(X))
#define vmalloc_user malloc
#define kfree free

struct list_head {
	struct list_head *next, *prev;
};
