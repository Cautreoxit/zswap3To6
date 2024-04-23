// This file is for migrating zswap from 3.12.60 to 6.4.3


/*********************************
* include
**********************************/
#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/highmem.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/atomic.h>
#include <linux/frontswap.h>
#include <linux/rbtree.h>
#include <linux/swap.h>
#include <linux/crypto.h>
#include <linux/scatterlist.h>
#include <linux/mempool.h>
#include <linux/zpool.h>
#include <crypto/acompress.h>

#include <linux/mm_types.h>
#include <linux/page-flags.h>
#include <linux/swapops.h>
#include <linux/writeback.h>
#include <linux/pagemap.h>
#include <linux/workqueue.h>


#include <linux/myInterface.h>

#include <linux/list.h>
#include <linux/mm.h>
#include <linux/preempt.h>


#include <linux/notifier.h>


u64 zswap_pool_total_size;


void get_online_cpus(void)
{
	return;
}

void put_online_cpus(void)
{
	return;
}

static RAW_NOTIFIER_HEAD(cpu_chain);

int __ref register_cpu_notifier(struct notifier_block *nb)
{
	int ret;
	cpu_maps_update_begin();
	ret = raw_notifier_chain_register(&cpu_chain, nb);
	cpu_maps_update_done();
	return ret;
}


void __set_page_locked(struct page *page)
{
	__set_bit(PG_locked, &page->flags);
}


int __add_to_swap_cache(struct page *page, swp_entry_t entry)
{
	return add_to_swap_cache(page_folio(page), entry, GFP_KERNEL, NULL);
}


void lru_cache_add_anon(struct page *page)
{
	folio_add_lru(page_folio(page));
}

void __clear_page_locked(struct page *page)
{
	__clear_bit(PG_locked, &page->flags);
}


void swapcache_free(swp_entry_t entry, struct page *page)
{
	put_swap_folio(page_folio(page), entry);
}


#define NCHUNKS_ORDER	6

#define CHUNK_SHIFT	(PAGE_SHIFT - NCHUNKS_ORDER)
#define CHUNK_SIZE	(1 << CHUNK_SHIFT)
#define NCHUNKS		(PAGE_SIZE >> CHUNK_SHIFT)
#define ZHDR_SIZE_ALIGNED CHUNK_SIZE

struct zbud_pool {
	spinlock_t lock;
	struct list_head unbuddied[NCHUNKS];
	struct list_head buddied;
	struct list_head lru;
	u64 pages_nr;
	struct zbud_ops *ops;
};

struct zbud_header {
	struct list_head buddy;
	struct list_head lru;
	unsigned int first_chunks;
	unsigned int last_chunks;
	bool under_reclaim;
};

enum buddy {
	FIRST,
	LAST
};

static int size_to_chunks(int size)
{
	return (size + CHUNK_SIZE - 1) >> CHUNK_SHIFT;
}

#define for_each_unbuddied_list(_iter, _begin) \
	for ((_iter) = (_begin); (_iter) < NCHUNKS; (_iter)++)


static struct zbud_header *init_zbud_page(struct page *page)
{
	struct zbud_header *zhdr = page_address(page);
	zhdr->first_chunks = 0;
	zhdr->last_chunks = 0;
	INIT_LIST_HEAD(&zhdr->buddy);
	INIT_LIST_HEAD(&zhdr->lru);
	zhdr->under_reclaim = 0;
	return zhdr;
}


static void free_zbud_page(struct zbud_header *zhdr)
{
	__free_page(virt_to_page(zhdr));
}


static unsigned long encode_handle(struct zbud_header *zhdr, enum buddy bud)
{
	unsigned long handle;

	handle = (unsigned long)zhdr;
	if (bud == FIRST)
		handle += ZHDR_SIZE_ALIGNED;
	else
		handle += PAGE_SIZE - (zhdr->last_chunks  << CHUNK_SHIFT);
	return handle;
}


static struct zbud_header *handle_to_zbud_header(unsigned long handle)
{
	return (struct zbud_header *)(handle & PAGE_MASK);
}


static int num_free_chunks(struct zbud_header *zhdr)
{
	return NCHUNKS - zhdr->first_chunks - zhdr->last_chunks - 1;
}


struct zbud_pool *zbud_create_pool(gfp_t gfp, struct zbud_ops *ops)
{
	struct zbud_pool *pool;
	int i;

	pool = kmalloc(sizeof(struct zbud_pool), gfp);
	if (!pool)
		return NULL;
	spin_lock_init(&pool->lock);
	for_each_unbuddied_list(i, 0)
		INIT_LIST_HEAD(&pool->unbuddied[i]);
	INIT_LIST_HEAD(&pool->buddied);
	INIT_LIST_HEAD(&pool->lru);
	pool->pages_nr = 0;
	pool->ops = ops;
	return pool;
}


void zbud_destroy_pool(struct zbud_pool *pool)
{
	kfree(pool);
}


int zbud_alloc(struct zbud_pool *pool, int size, gfp_t gfp,
			unsigned long *handle)
{
	int chunks, i, freechunks;
	struct zbud_header *zhdr = NULL;
	enum buddy bud;
	struct page *page;

	if (size <= 0 || gfp & __GFP_HIGHMEM)
		return -EINVAL;
	if (size > PAGE_SIZE - ZHDR_SIZE_ALIGNED - CHUNK_SIZE)
		return -ENOSPC;
	chunks = size_to_chunks(size);
	spin_lock(&pool->lock);

	zhdr = NULL;
	for_each_unbuddied_list(i, chunks) {
		if (!list_empty(&pool->unbuddied[i])) {
			zhdr = list_first_entry(&pool->unbuddied[i],
					struct zbud_header, buddy);
			list_del(&zhdr->buddy);
			if (zhdr->first_chunks == 0)
				bud = FIRST;
			else
				bud = LAST;
			goto found;
		}
	}

	spin_unlock(&pool->lock);
	page = alloc_page(gfp);
	if (!page)
		return -ENOMEM;
	spin_lock(&pool->lock);
	pool->pages_nr++;
	zhdr = init_zbud_page(page);
	bud = FIRST;

found:
	if (bud == FIRST)
		zhdr->first_chunks = chunks;
	else
		zhdr->last_chunks = chunks;

	if (zhdr->first_chunks == 0 || zhdr->last_chunks == 0) {
		freechunks = num_free_chunks(zhdr);
		list_add(&zhdr->buddy, &pool->unbuddied[freechunks]);
	} else {
		list_add(&zhdr->buddy, &pool->buddied);
	}

	if (!list_empty(&zhdr->lru))
		list_del(&zhdr->lru);
	list_add(&zhdr->lru, &pool->lru);

	*handle = encode_handle(zhdr, bud);
	spin_unlock(&pool->lock);

	return 0;
}


void zbud_free(struct zbud_pool *pool, unsigned long handle)
{
	struct zbud_header *zhdr;
	int freechunks;

	spin_lock(&pool->lock);
	zhdr = handle_to_zbud_header(handle);

	if ((handle - ZHDR_SIZE_ALIGNED) & ~PAGE_MASK)
		zhdr->last_chunks = 0;
	else
		zhdr->first_chunks = 0;

	if (zhdr->under_reclaim) {
		spin_unlock(&pool->lock);
		return;
	}

	list_del(&zhdr->buddy);

	if (zhdr->first_chunks == 0 && zhdr->last_chunks == 0) {
		list_del(&zhdr->lru);
		free_zbud_page(zhdr);
		pool->pages_nr--;
	} else {
		freechunks = num_free_chunks(zhdr);
		list_add(&zhdr->buddy, &pool->unbuddied[freechunks]);
	}

	spin_unlock(&pool->lock);
}

#define list_tail_entry(ptr, type, member) \
	list_entry((ptr)->prev, type, member)


int zbud_reclaim_page(struct zbud_pool *pool, unsigned int retries)
{
	int i, ret, freechunks;
	struct zbud_header *zhdr;
	unsigned long first_handle = 0, last_handle = 0;

	spin_lock(&pool->lock);
	if (!pool->ops || !pool->ops->evict || list_empty(&pool->lru) ||
			retries == 0) {
		spin_unlock(&pool->lock);
		return -EINVAL;
	}
	for (i = 0; i < retries; i++) {
		zhdr = list_tail_entry(&pool->lru, struct zbud_header, lru);
		list_del(&zhdr->lru);
		list_del(&zhdr->buddy);
		zhdr->under_reclaim = true;
		first_handle = 0;
		last_handle = 0;
		if (zhdr->first_chunks)
			first_handle = encode_handle(zhdr, FIRST);
		if (zhdr->last_chunks)
			last_handle = encode_handle(zhdr, LAST);
		spin_unlock(&pool->lock);

		if (first_handle) {
			ret = pool->ops->evict(pool, first_handle);
			if (ret)
				goto next;
		}
		if (last_handle) {
			ret = pool->ops->evict(pool, last_handle);
			if (ret)
				goto next;
		}
next:
		spin_lock(&pool->lock);
		zhdr->under_reclaim = false;
		if (zhdr->first_chunks == 0 && zhdr->last_chunks == 0) {
			free_zbud_page(zhdr);
			pool->pages_nr--;
			spin_unlock(&pool->lock);
			return 0;
		} else if (zhdr->first_chunks == 0 ||
				zhdr->last_chunks == 0) {
			freechunks = num_free_chunks(zhdr);
			list_add(&zhdr->buddy, &pool->unbuddied[freechunks]);
		} else {
			list_add(&zhdr->buddy, &pool->buddied);
		}

		list_add(&zhdr->lru, &pool->lru);
	}
	spin_unlock(&pool->lock);
	return -EAGAIN;
}


void *zbud_map(struct zbud_pool *pool, unsigned long handle)
{
	return (void *)(handle);
}


void zbud_unmap(struct zbud_pool *pool, unsigned long handle)
{
}

u64 zbud_get_pool_size(struct zbud_pool *pool)
{
	return pool->pages_nr;
}