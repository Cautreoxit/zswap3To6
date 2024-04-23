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




#define totalram_pages totalram_pages()


#define CPU_UP_CANCELED		0x0004 

void get_online_cpus(void);
void put_online_cpus(void);
int __ref register_cpu_notifier(struct notifier_block *nb);


void __set_page_locked(struct page *page);
int __add_to_swap_cache(struct page *page, swp_entry_t entry);
void lru_cache_add_anon(struct page *page);
void __clear_page_locked(struct page *page);
void swapcache_free(swp_entry_t entry, struct page *page);
#define page_cache_release(page)	put_page(page)

#define __GET_MACRO(_1, _2, _3, NAME, ...) NAME
#define __swap_writepage(...) __GET_MACRO(__VA_ARGS__, __swap_writepage3, __swap_writepage2)(__VA_ARGS__)
#define __swap_writepage2(a, b) __swap_writepage(a, b)
#define __swap_writepage3(a, b, c) __swap_writepage2(a, b)


struct zbud_pool;

struct zbud_ops {
	int (*evict)(struct zbud_pool *pool, unsigned long handle);
};

struct zbud_pool *zbud_create_pool(gfp_t gfp, struct zbud_ops *ops);
void zbud_destroy_pool(struct zbud_pool *pool);
int zbud_alloc(struct zbud_pool *pool, int size, gfp_t gfp,
	unsigned long *handle);
void zbud_free(struct zbud_pool *pool, unsigned long handle);
int zbud_reclaim_page(struct zbud_pool *pool, unsigned int retries);
void *zbud_map(struct zbud_pool *pool, unsigned long handle);
void zbud_unmap(struct zbud_pool *pool, unsigned long handle);
u64 zbud_get_pool_size(struct zbud_pool *pool);


#include <linux/blk_types.h> 

int sio_pool_init(void);
struct swap_iocb;
void swap_readpage(struct page *page, bool do_poll, struct swap_iocb **plug);
void __swap_read_unplug(struct swap_iocb *plug);
static inline void swap_read_unplug(struct swap_iocb *plug)
{
	if (unlikely(plug))
		__swap_read_unplug(plug);
}
void swap_write_unplug(struct swap_iocb *sio);
int swap_writepage(struct page *page, struct writeback_control *wbc);
void __swap_writepage(struct page *page, struct writeback_control *wbc);


#define SWAP_ADDRESS_SPACE_SHIFT	14
#define SWAP_ADDRESS_SPACE_PAGES	(1 << SWAP_ADDRESS_SPACE_SHIFT)
extern struct address_space *swapper_spaces[];
#define swap_address_space(entry)			    \
	(&swapper_spaces[swp_type(entry)][swp_offset(entry) \
		>> SWAP_ADDRESS_SPACE_SHIFT])

void show_swap_cache_info(void);
bool add_to_swap(struct folio *folio);
void *get_shadow_from_swap_cache(swp_entry_t entry);
int add_to_swap_cache(struct folio *folio, swp_entry_t entry,
		      gfp_t gfp, void **shadowp);
void __delete_from_swap_cache(struct folio *folio,
			      swp_entry_t entry, void *shadow);
void delete_from_swap_cache(struct folio *folio);
void clear_shadow_from_swap_cache(int type, unsigned long begin,
				  unsigned long end);
struct folio *swap_cache_get_folio(swp_entry_t entry,
		struct vm_area_struct *vma, unsigned long addr);
struct folio *filemap_get_incore_folio(struct address_space *mapping,
		pgoff_t index);

struct page *read_swap_cache_async(swp_entry_t entry, gfp_t gfp_mask,
				   struct vm_area_struct *vma,
				   unsigned long addr,
				   bool do_poll,
				   struct swap_iocb **plug);
struct page *__read_swap_cache_async(swp_entry_t entry, gfp_t gfp_mask,
				     struct vm_area_struct *vma,
				     unsigned long addr,
				     bool *new_page_allocated);
struct page *swap_cluster_readahead(swp_entry_t entry, gfp_t flag,
				    struct vm_fault *vmf);
struct page *swapin_readahead(swp_entry_t entry, gfp_t flag,
			      struct vm_fault *vmf);

static inline unsigned int folio_swap_flags(struct folio *folio)
{
	return page_swap_info(&folio->page)->flags;
}