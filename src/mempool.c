/* 
 * IRC - Internet Relay Chat, src/mempool.c
 * Copyright (c) 2007-2012, The Tor Project, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following disclaimer
 *     in the documentation and/or other materials provided with the
 *     distribution.
 *
 *   * Neither the names of the copyright owners nor the names of its
 *     contributors may be used to endorse or promote products derived from
 *     this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Imported from hybrid svn:
 * \file mempool.c
 * \brief A pooling allocator
 * \version $Id: mempool.c 1967 2013-05-08 14:33:22Z michael $
 */

#include "unrealircd.h"

#if __has_feature(address_sanitizer) || defined(__SANITIZE_ADDRESS__)
/* When running with AddressSanitizer, if using memory pools we will
 * likely NOT detect various kinds of misusage. (This is a known problem)
 * Therefore, if ASan is enabled, we don't use memory pools and will
 * use malloc/free instead, so ASan can do it's job much better.
 * Theoretically we could still use ASan + memory pools if we manually
 * designate and mark red zones. However, that still does not fix the
 * case of quick memory re-use. Mempool re-uses a freed area quickly
 * to be efficient, while ASan does the exact opposite (putting
 * freed items in a 256MB quarantine buffer), since quick re-use
 * will hide use-after-free bugs. -- Syzop
 */
void mp_pool_init(void)
{
}

mp_pool_t *mp_pool_new(size_t sz, size_t ignored)
{
    mp_pool_t *m = safe_alloc(sizeof(mp_pool_t));
    /* We (mis)use the item_alloc_size. It has a slightly different
     * meaning in the real mempool code where it's aligned, rounded, etc.
     * That is something we don't want as it would hide small overflows.
     */
    m->item_alloc_size = sz;
    return m;
}

void *mp_pool_get(mp_pool_t *pool)
{
    return malloc(pool->item_alloc_size);
}

void mp_pool_release(void *item)
{
    safe_free(item);
}
#else

/** Returns floor(log2(u64)).  If u64 is 0, (incorrectly) returns 0. */
static int
tor_log2(uint64_t u64)
{
  int r = 0;

  if (u64 >= (1ULL << 32))
  {
    u64 >>= 32;
    r = 32;
  }
  if (u64 >= (1ULL << 16))
  {
    u64 >>= 16;
    r += 16;
  }
  if (u64 >= (1ULL <<  8))
  {
    u64 >>= 8;
    r += 8;
  }
  if (u64 >= (1ULL <<  4))
  {
    u64 >>= 4;
    r += 4;
  }
  if (u64 >= (1ULL <<  2))
  {
    u64 >>= 2;
    r += 2;
  }
  if (u64 >= (1ULL <<  1))
  {
    u64 >>= 1;
    r += 1;
  }

  return r;
}

/** Return the power of 2 in range [1,UINT64_MAX] closest to <b>u64</b>.  If
 * there are two powers of 2 equally close, round down. */
static uint64_t
round_to_power_of_2(uint64_t u64)
{
  int lg2;
  uint64_t low;
  uint64_t high;

  if (u64 == 0)
    return 1;

  lg2 = tor_log2(u64);
  low = 1ULL << lg2;

  if (lg2 == 63)
    return low;

  high = 1ULL << (lg2 + 1);
  if (high - u64 < u64 - low)
    return high;
  else
    return low;
}

/* OVERVIEW:
 *
 *     This is an implementation of memory pools for Tor cells.  It may be
 *     useful for you too.
 *
 *     Generally, a memory pool is an allocation strategy optimized for large
 *     numbers of identically-sized objects.  Rather than the elaborate arena
 *     and coalescing strategies you need to get good performance for a
 *     general-purpose malloc(), pools use a series of large memory "chunks",
 *     each of which is carved into a bunch of smaller "items" or
 *     "allocations".
 *
 *     To get decent performance, you need to:
 *        - Minimize the number of times you hit the underlying allocator.
 *        - Try to keep accesses as local in memory as possible.
 *        - Try to keep the common case fast.
 *
 *     Our implementation uses three lists of chunks per pool.  Each chunk can
 *     be either "full" (no more room for items); "empty" (no items); or
 *     "used" (not full, not empty).  There are independent doubly-linked
 *     lists for each state.
 *
 * CREDIT:
 *
 *     I wrote this after looking at 3 or 4 other pooling allocators, but
 *     without copying.  The strategy this most resembles (which is funny,
 *     since that's the one I looked at longest ago) is the pool allocator
 *     underlying Python's obmalloc code.  Major differences from obmalloc's
 *     pools are:
 *       - We don't even try to be threadsafe.
 *       - We only handle objects of one size.
 *       - Our list of empty chunks is doubly-linked, not singly-linked.
 *         (This could change pretty easily; it's only doubly-linked for
 *         consistency.)
 *       - We keep a list of full chunks (so we can have a "nuke everything"
 *         function).  Obmalloc's pools leave full chunks to float unanchored.
 *
 * LIMITATIONS:
 *   - Not even slightly threadsafe.
 *   - Likes to have lots of items per chunks.
 *   - One pointer overhead per allocated thing.  (The alternative is
 *     something like glib's use of an RB-tree to keep track of what
 *     chunk any given piece of memory is in.)
 *   - Only aligns allocated things to void* level: redefine ALIGNMENT_TYPE
 *     if you need doubles.
 *   - Could probably be optimized a bit; the representation contains
 *     a bit more info than it really needs to have.
 */

/* Tuning parameters */
/** Largest type that we need to ensure returned memory items are aligned to.
 * Change this to "double" if we need to be safe for structs with doubles. */
#define ALIGNMENT_TYPE void *
/** Increment that we need to align allocated. */
#define ALIGNMENT sizeof(ALIGNMENT_TYPE)
/** Largest memory chunk that we should allocate. */
#define MAX_CHUNK (8 *(1L << 20))
/** Smallest memory chunk size that we should allocate. */
#define MIN_CHUNK 4096

typedef struct mp_allocated_t mp_allocated_t;
typedef struct mp_chunk_t mp_chunk_t;

/** Holds a single allocated item, allocated as part of a chunk. */
struct mp_allocated_t {
  /** The chunk that this item is allocated in.  This adds overhead to each
   * allocated item, thus making this implementation inappropriate for
   * very small items. */
  mp_chunk_t *in_chunk;

  union {
    /** If this item is free, the next item on the free list. */
    mp_allocated_t *next_free;

    /** If this item is not free, the actual memory contents of this item.
     * (Not actual size.) */
    char mem[1];

    /** An extra element to the union to insure correct alignment. */
    ALIGNMENT_TYPE dummy_;
  } u;
};

/** 'Magic' value used to detect memory corruption. */
#define MP_CHUNK_MAGIC 0x09870123

/** A chunk of memory.  Chunks come from malloc; we use them  */
struct mp_chunk_t {
  uint32_t magic; /**< Must be MP_CHUNK_MAGIC if this chunk is valid. */
  mp_chunk_t *next; /**< The next free, used, or full chunk in sequence. */
  mp_chunk_t *prev; /**< The previous free, used, or full chunk in sequence. */
  mp_pool_t *pool; /**< The pool that this chunk is part of. */

  /** First free item in the freelist for this chunk.  Note that this may be
   * NULL even if this chunk is not at capacity: if so, the free memory at
   * next_mem has not yet been carved into items.
   */
  mp_allocated_t *first_free;
  int n_allocated; /**< Number of currently allocated items in this chunk. */
  int capacity; /**< Number of items that can be fit into this chunk. */
  size_t mem_size; /**< Number of usable bytes in mem. */
  char *next_mem; /**< Pointer into part of <b>mem</b> not yet carved up. */
  char mem[]; /**< Storage for this chunk. */
};

static mp_pool_t *mp_allocated_pools = NULL;

/** Number of extra bytes needed beyond mem_size to allocate a chunk. */
#define CHUNK_OVERHEAD offsetof(mp_chunk_t, mem[0])

/** Given a pointer to a mp_allocated_t, return a pointer to the memory
 * item it holds. */
#define A2M(a) (&(a)->u.mem)
/** Given a pointer to a memory_item_t, return a pointer to its enclosing
 * mp_allocated_t. */
#define M2A(p) (((char *)p) - offsetof(mp_allocated_t, u.mem))

void
mp_pool_init(void)
{
  EventAdd(NULL, "mp_pool_garbage_collect", &mp_pool_garbage_collect, NULL, 119*1000, 0);
}

/** Helper: Allocate and return a new memory chunk for <b>pool</b>.  Does not
 * link the chunk into any list. */
static mp_chunk_t *
mp_chunk_new(mp_pool_t *pool)
{
  size_t sz = pool->new_chunk_capacity * pool->item_alloc_size;
  mp_chunk_t *chunk = safe_alloc(CHUNK_OVERHEAD + sz);

#ifdef MEMPOOL_STATS
  ++pool->total_chunks_allocated;
#endif
  chunk->magic = MP_CHUNK_MAGIC;
  chunk->capacity = pool->new_chunk_capacity;
  chunk->mem_size = sz;
  chunk->next_mem = chunk->mem;
  chunk->pool = pool;
  return chunk;
}

/** Take a <b>chunk</b> that has just been allocated or removed from
 * <b>pool</b>'s empty chunk list, and add it to the head of the used chunk
 * list. */
static void
add_newly_used_chunk_to_used_list(mp_pool_t *pool, mp_chunk_t *chunk)
{
  chunk->next = pool->used_chunks;
  if (chunk->next)
    chunk->next->prev = chunk;
  pool->used_chunks = chunk;
  assert(!chunk->prev);
}

/** Return a newly allocated item from <b>pool</b>. */
void *
mp_pool_get(mp_pool_t *pool)
{
  mp_chunk_t *chunk;
  mp_allocated_t *allocated;

  if (pool->used_chunks != NULL) {
    /*
     * Common case: there is some chunk that is neither full nor empty. Use
     * that one. (We can't use the full ones, obviously, and we should fill
     * up the used ones before we start on any empty ones.
     */
    chunk = pool->used_chunks;

  } else if (pool->empty_chunks) {
    /*
     * We have no used chunks, but we have an empty chunk that we haven't
     * freed yet: use that. (We pull from the front of the list, which should
     * get us the most recently emptied chunk.)
     */
    chunk = pool->empty_chunks;

    /* Remove the chunk from the empty list. */
    pool->empty_chunks = chunk->next;
    if (chunk->next)
      chunk->next->prev = NULL;

    /* Put the chunk on the 'used' list*/
    add_newly_used_chunk_to_used_list(pool, chunk);

    assert(!chunk->prev);
    --pool->n_empty_chunks;
    if (pool->n_empty_chunks < pool->min_empty_chunks)
      pool->min_empty_chunks = pool->n_empty_chunks;
  } else {
    /* We have no used or empty chunks: allocate a new chunk. */
    chunk = mp_chunk_new(pool);

    /* Add the new chunk to the used list. */
    add_newly_used_chunk_to_used_list(pool, chunk);
  }

  assert(chunk->n_allocated < chunk->capacity);

  if (chunk->first_free) {
    /* If there's anything on the chunk's freelist, unlink it and use it. */
    allocated = chunk->first_free;
    chunk->first_free = allocated->u.next_free;
    allocated->u.next_free = NULL; /* For debugging; not really needed. */
    assert(allocated->in_chunk == chunk);
  } else {
    /* Otherwise, the chunk had better have some free space left on it. */
    assert(chunk->next_mem + pool->item_alloc_size <=
           chunk->mem + chunk->mem_size);

    /* Good, it did.  Let's carve off a bit of that free space, and use
     * that. */
    allocated = (void *)chunk->next_mem;
    chunk->next_mem += pool->item_alloc_size;
    allocated->in_chunk = chunk;
    allocated->u.next_free = NULL; /* For debugging; not really needed. */
  }

  ++chunk->n_allocated;
#ifdef MEMPOOL_STATS
  ++pool->total_items_allocated;
#endif

  if (chunk->n_allocated == chunk->capacity) {
    /* This chunk just became full. */
    assert(chunk == pool->used_chunks);
    assert(chunk->prev == NULL);

    /* Take it off the used list. */
    pool->used_chunks = chunk->next;
    if (chunk->next)
      chunk->next->prev = NULL;

    /* Put it on the full list. */
    chunk->next = pool->full_chunks;
    if (chunk->next)
      chunk->next->prev = chunk;
    pool->full_chunks = chunk;
  }
  /* And return the memory portion of the mp_allocated_t. */
  return A2M(allocated);
}

/** Return an allocated memory item to its memory pool. */
void
mp_pool_release(void *item)
{
  mp_allocated_t *allocated = (void *)M2A(item);
  mp_chunk_t *chunk = allocated->in_chunk;

  assert(chunk);
  assert(chunk->magic == MP_CHUNK_MAGIC);
  assert(chunk->n_allocated > 0);

  allocated->u.next_free = chunk->first_free;
  chunk->first_free = allocated;

  if (chunk->n_allocated == chunk->capacity) {
    /* This chunk was full and is about to be used. */
    mp_pool_t *pool = chunk->pool;
    /* unlink from the full list  */
    if (chunk->prev)
      chunk->prev->next = chunk->next;
    if (chunk->next)
      chunk->next->prev = chunk->prev;
    if (chunk == pool->full_chunks)
      pool->full_chunks = chunk->next;

    /* link to the used list. */
    chunk->next = pool->used_chunks;
    chunk->prev = NULL;
    if (chunk->next)
      chunk->next->prev = chunk;
    pool->used_chunks = chunk;
  } else if (chunk->n_allocated == 1) {
    /* This was used and is about to be empty. */
    mp_pool_t *pool = chunk->pool;

    /* Unlink from the used list */
    if (chunk->prev)
      chunk->prev->next = chunk->next;
    if (chunk->next)
      chunk->next->prev = chunk->prev;
    if (chunk == pool->used_chunks)
      pool->used_chunks = chunk->next;

    /* Link to the empty list */
    chunk->next = pool->empty_chunks;
    chunk->prev = NULL;
    if (chunk->next)
      chunk->next->prev = chunk;
    pool->empty_chunks = chunk;

    /* Reset the guts of this chunk to defragment it, in case it gets
     * used again. */
    chunk->first_free = NULL;
    chunk->next_mem = chunk->mem;

    ++pool->n_empty_chunks;
  }

  --chunk->n_allocated;
}

/** Allocate a new memory pool to hold items of size <b>item_size</b>. We'll
 * try to fit about <b>chunk_capacity</b> bytes in each chunk. */
mp_pool_t *
mp_pool_new(size_t item_size, size_t chunk_capacity)
{
  mp_pool_t *pool;
  size_t alloc_size, new_chunk_cap;

/*  assert(item_size < SIZE_T_CEILING);
  assert(chunk_capacity < SIZE_T_CEILING);
  assert(SIZE_T_CEILING / item_size > chunk_capacity);
*/
  pool = safe_alloc(sizeof(mp_pool_t));
  /*
   * First, we figure out how much space to allow per item. We'll want to
   * use make sure we have enough for the overhead plus the item size.
   */
  alloc_size = (size_t)(offsetof(mp_allocated_t, u.mem) + item_size);
  /*
   * If the item_size is less than sizeof(next_free), we need to make
   * the allocation bigger.
   */
  if (alloc_size < sizeof(mp_allocated_t))
    alloc_size = sizeof(mp_allocated_t);

  /* If we're not an even multiple of ALIGNMENT, round up. */
  if (alloc_size % ALIGNMENT) {
    alloc_size = alloc_size + ALIGNMENT - (alloc_size % ALIGNMENT);
  }
  if (alloc_size < ALIGNMENT)
    alloc_size = ALIGNMENT;
  assert((alloc_size % ALIGNMENT) == 0);

  /*
   * Now we figure out how many items fit in each chunk. We need to fit at
   * least 2 items per chunk. No chunk can be more than MAX_CHUNK bytes long,
   * or less than MIN_CHUNK.
   */
  if (chunk_capacity > MAX_CHUNK)
    chunk_capacity = MAX_CHUNK;

  /*
   * Try to be around a power of 2 in size, since that's what allocators like
   * handing out. 512K-1 byte is a lot better than 512K+1 byte.
   */
  chunk_capacity = (size_t) round_to_power_of_2(chunk_capacity);
  while (chunk_capacity < alloc_size * 2 + CHUNK_OVERHEAD)
    chunk_capacity *= 2;
  if (chunk_capacity < MIN_CHUNK)
    chunk_capacity = MIN_CHUNK;

  new_chunk_cap = (chunk_capacity-CHUNK_OVERHEAD) / alloc_size;
  assert(new_chunk_cap < INT_MAX);
  pool->new_chunk_capacity = (int)new_chunk_cap;

  pool->item_alloc_size = alloc_size;

  pool->next = mp_allocated_pools;
  mp_allocated_pools = pool;

  return pool;
}

/** Helper function for qsort: used to sort pointers to mp_chunk_t into
 * descending order of fullness. */
static int
mp_pool_sort_used_chunks_helper(const void *_a, const void *_b)
{
  mp_chunk_t *a = *(mp_chunk_t * const *)_a;
  mp_chunk_t *b = *(mp_chunk_t * const *)_b;
  return b->n_allocated - a->n_allocated;
}

/** Sort the used chunks in <b>pool</b> into descending order of fullness,
 * so that we preferentially fill up mostly full chunks before we make
 * nearly empty chunks less nearly empty. */
static void
mp_pool_sort_used_chunks(mp_pool_t *pool)
{
  int i, n = 0, inverted = 0;
  mp_chunk_t **chunks, *chunk;

  for (chunk = pool->used_chunks; chunk; chunk = chunk->next) {
    ++n;
    if (chunk->next && chunk->next->n_allocated > chunk->n_allocated)
      ++inverted;
  }

  if (!inverted)
    return;

  chunks = safe_alloc(sizeof(mp_chunk_t *) * n);

  for (i=0,chunk = pool->used_chunks; chunk; chunk = chunk->next)
    chunks[i++] = chunk;

  qsort(chunks, n, sizeof(mp_chunk_t *), mp_pool_sort_used_chunks_helper);
  pool->used_chunks = chunks[0];
  chunks[0]->prev = NULL;

  for (i = 1; i < n; ++i) {
    chunks[i - 1]->next = chunks[i];
    chunks[i]->prev = chunks[i - 1];
  }

  chunks[n - 1]->next = NULL;
  safe_free(chunks);
  mp_pool_assert_ok(pool);
}

/** If there are more than <b>n</b> empty chunks in <b>pool</b>, free the
 * excess ones that have been empty for the longest. If
 * <b>keep_recently_used</b> is true, do not free chunks unless they have been
 * empty since the last call to this function.
 **/
void
mp_pool_clean(mp_pool_t *pool, int n_to_keep, int keep_recently_used)
{
  mp_chunk_t *chunk, **first_to_free;

  mp_pool_sort_used_chunks(pool);
  assert(n_to_keep >= 0);

  if (keep_recently_used) {
    int n_recently_used = pool->n_empty_chunks - pool->min_empty_chunks;
    if (n_to_keep < n_recently_used)
      n_to_keep = n_recently_used;
  }

  assert(n_to_keep >= 0);

  first_to_free = &pool->empty_chunks;
  while (*first_to_free && n_to_keep > 0) {
    first_to_free = &(*first_to_free)->next;
    --n_to_keep;
  }
  if (!*first_to_free) {
    pool->min_empty_chunks = pool->n_empty_chunks;
    return;
  }

  chunk = *first_to_free;
  while (chunk) {
    mp_chunk_t *next = chunk->next;
    chunk->magic = 0xdeadbeef;
    safe_free(chunk);
#ifdef MEMPOOL_STATS
    ++pool->total_chunks_freed;
#endif
    --pool->n_empty_chunks;
    chunk = next;
  }

  pool->min_empty_chunks = pool->n_empty_chunks;
  *first_to_free = NULL;
}

/** Helper: Given a list of chunks, free all the chunks in the list. */
static void destroy_chunks(mp_chunk_t *chunk)
{
  mp_chunk_t *next;

  while (chunk) {
    chunk->magic = 0xd3adb33f;
    next = chunk->next;
    safe_free(chunk);
    chunk = next;
  }
}

/** Helper: make sure that a given chunk list is not corrupt. */
static int
assert_chunks_ok(mp_pool_t *pool, mp_chunk_t *chunk, int empty, int full)
{
  mp_allocated_t *allocated;
  int n = 0;

  if (chunk)
    assert(chunk->prev == NULL);

  while (chunk) {
    n++;
    assert(chunk->magic == MP_CHUNK_MAGIC);
    assert(chunk->pool == pool);
    for (allocated = chunk->first_free; allocated;
         allocated = allocated->u.next_free) {
      assert(allocated->in_chunk == chunk);
    }
    if (empty)
      assert(chunk->n_allocated == 0);
    else if (full)
      assert(chunk->n_allocated == chunk->capacity);
    else
      assert(chunk->n_allocated > 0 && chunk->n_allocated < chunk->capacity);

    assert(chunk->capacity == pool->new_chunk_capacity);

    assert(chunk->mem_size ==
           pool->new_chunk_capacity * pool->item_alloc_size);

    assert(chunk->next_mem >= chunk->mem &&
           chunk->next_mem <= chunk->mem + chunk->mem_size);

    if (chunk->next)
      assert(chunk->next->prev == chunk);

    chunk = chunk->next;
  }

  return n;
}

/** Fail with an assertion if <b>pool</b> is not internally consistent. */
void
mp_pool_assert_ok(mp_pool_t *pool)
{
  int n_empty;

  n_empty = assert_chunks_ok(pool, pool->empty_chunks, 1, 0);
  assert_chunks_ok(pool, pool->full_chunks, 0, 1);
  assert_chunks_ok(pool, pool->used_chunks, 0, 0);

  assert(pool->n_empty_chunks == n_empty);
}

void
mp_pool_garbage_collect(void *arg)
{
  mp_pool_t *pool = mp_allocated_pools;

  for (; pool; pool = pool->next)
    mp_pool_clean(pool, 0, 1);
}

/** Dump information about <b>pool</b>'s memory usage to the Tor log at level
 * <b>severity</b>. */
void
mp_pool_log_status(mp_pool_t *pool)
{
  unsigned long long bytes_used = 0;
  unsigned long long bytes_allocated = 0;
  unsigned long long bu = 0, ba = 0;
  mp_chunk_t *chunk;
  int n_full = 0, n_used = 0;

  assert(pool);

  for (chunk = pool->empty_chunks; chunk; chunk = chunk->next)
    bytes_allocated += chunk->mem_size;

  for (chunk = pool->used_chunks; chunk; chunk = chunk->next) {
    ++n_used;
    bu += chunk->n_allocated * pool->item_alloc_size;
    ba += chunk->mem_size;
  }

  bytes_used += bu;
  bytes_allocated += ba;
  bu = ba = 0;

  for (chunk = pool->full_chunks; chunk; chunk = chunk->next) {
    ++n_full;
    bu += chunk->n_allocated * pool->item_alloc_size;
    ba += chunk->mem_size;
  }

  bytes_used += bu;
  bytes_allocated += ba;
}
#endif
