/*
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
 * \file mempool.h
 * \brief Pooling allocator
 * \version $Id: mempool.h 1662 2012-11-17 20:11:33Z michael $
 */

#ifndef TOR_MEMPOOL_H
#define TOR_MEMPOOL_H

#ifdef _WIN32
  #define uint64_t unsigned __int64
#endif

/** A memory pool is a context in which a large number of fixed-sized
* objects can be allocated efficiently.  See mempool.c for implementation
* details. */
typedef struct mp_pool_t mp_pool_t;

extern void mp_pool_init(void);
extern void *mp_pool_get(mp_pool_t *);
extern void mp_pool_release(void *);
extern mp_pool_t *mp_pool_new(size_t, size_t);
extern void mp_pool_clean(mp_pool_t *, int, int);
extern void mp_pool_destroy(mp_pool_t *);
extern void mp_pool_assert_ok(mp_pool_t *);
extern void mp_pool_log_status(mp_pool_t *);
extern void mp_pool_garbage_collect(void *);

#define MEMPOOL_STATS

struct mp_pool_t {
  /** Next pool. A pool is usually linked into the mp_allocated_pools list. */
  mp_pool_t *next;

  /** Doubly-linked list of chunks in which no items have been allocated.
   * The front of the list is the most recently emptied chunk. */
  struct mp_chunk_t *empty_chunks;

  /** Doubly-linked list of chunks in which some items have been allocated,
   * but which are not yet full. The front of the list is the chunk that has
   * most recently been modified. */
  struct mp_chunk_t *used_chunks;

  /** Doubly-linked list of chunks in which no more items can be allocated.
   * The front of the list is the chunk that has most recently become full. */
  struct mp_chunk_t *full_chunks;

  /** Length of <b>empty_chunks</b>. */
  int n_empty_chunks;

  /** Lowest value of <b>empty_chunks</b> since last call to
   * mp_pool_clean(-1). */
  int min_empty_chunks;

  /** Size of each chunk (in items). */
  int new_chunk_capacity;

  /** Size to allocate for each item, including overhead and alignment
   * padding. */
  size_t item_alloc_size;
#ifdef MEMPOOL_STATS
  /** Total number of items allocated ever. */
  uint64_t total_items_allocated;

  /** Total number of chunks allocated ever. */
  uint64_t total_chunks_allocated;

  /** Total number of chunks freed ever. */
  uint64_t total_chunks_freed;
#endif
};

#endif
