#include "mem_alloc.h"
#include <string.h>

#ifdef MEM_TESTING
#include <assert.h>
#define ASSERT(x) assert(x)
#else
#define ASSERT(x)
#endif

typedef __xdata struct mem_block* block_ptr;
typedef __xdata uint8_t* byte_ptr;

static block_ptr block_end;
static block_ptr first_free;

#define block_offset(p,o) ((block_ptr)(((byte_ptr)p)+(o)))

#define MEM_BLOCK(p) (&(((block_ptr)p)[-1]))

#define SMALLEST_FREE_BLOCK (sizeof(struct mem_block) + 4)

#ifdef MEM_TESTING

static void
check_integrity(void)
{
  int found_first_free = 0;
  block_ptr block = (block_ptr)mem_heap_start;
  while(block != block_end) {
    if (block->ptr) {
      ASSERT(MEM_BLOCK(*block->ptr) == block);
    } else {
      if (!found_first_free) {
	ASSERT(first_free == block);
      }
      found_first_free = 1;
    }
    block = block_offset(block, block->len);
  }
}

#define CHECK_INTERGRITY() check_integrity()
#else
#define CHECK_INTERGRITY()
#endif

void
mem_init(void)
{
  block_end = (block_ptr)mem_heap_start;
  first_free = (block_ptr)mem_heap_start;
}


static __xdata void *
allocate_in_block(block_ptr free, __xdata void * __xdata *ptr, mem_size_t len)
{
  block_ptr block = free;
  /* Allocate block */
  block->ptr = ptr;
  *ptr = block_offset(block,sizeof(struct mem_block));
  /* Should we split the block or not? */
  if (block->len - len >= SMALLEST_FREE_BLOCK) {
    free = block_offset(free, len);
    free->len = block->len - len;
    free->ptr = 0;
    block->len = len;
    if (block == first_free) first_free = free;
  } else {
    if (block == first_free) {
      /* Find first free block */
      while(first_free != block_end) {
	if (!first_free->ptr) break;
	first_free = block_offset(first_free, first_free->len);
      }
    }
  }
  CHECK_INTERGRITY();
  return *ptr;
}

__xdata void *
mem_alloc(__xdata void * __xdata *ptr, mem_size_t len)
{
  len +=  sizeof(struct mem_block); /* Add room for header */
  if (first_free != block_end) {
    block_ptr prev_free = first_free;
    
    if (first_free->len >= len) {
      return allocate_in_block(first_free, ptr, len);
    }

    /* Only look through the blocks if there's not enough space at the end */
    if (mem_heap_end - (byte_ptr)block_end < len) {
      /* Look for a free block big enough to hold the allocation.
	 Compacting while looking.
      */
      /* First block has already been tried */
      block_ptr block = block_offset(first_free, first_free->len);
      while(block != block_end) {
	if (block->ptr == 0) {
	  ASSERT(prev_free);
	  /* Merge free blocks */
	  prev_free->len += block->len;
	  /* Check if we've found a block that's big enough */
	  if (prev_free->len >= len) {
	   return allocate_in_block(first_free, ptr, len);
	  }
	} else {
	  if (prev_free) {
	    /* Move block down */
	    mem_size_t free_len = prev_free->len;
	    memmove(prev_free, block, block->len);
	    block = prev_free;
	    *block->ptr = block_offset(block,sizeof(struct mem_block));
	    prev_free = block_offset(block, block->len);
	    prev_free->ptr = 0;
	    prev_free->len = free_len;
	    if (block == first_free) {
	      first_free = prev_free;
	    }
	    block = prev_free;
	  }
	}
	block = block_offset(block, block->len);
      }
      if (prev_free) { /* If the last block was free then move block_end down */
	block_end = prev_free;
      }
    }
  }
  if (mem_heap_end - (byte_ptr)block_end >= len) {
    block_ptr block = block_end;
    block_end = block_offset(block_end, len);
    if (first_free == block) {
      first_free = block_end;
    }
    block->ptr = ptr;
    block->len = len;
    *ptr =  block_offset(block,sizeof(struct mem_block));
    CHECK_INTERGRITY();
    return *ptr;
  }
  return 0;
}

void
mem_free(__xdata void *  __xdata *ptr)
{
  block_ptr block = MEM_BLOCK(*ptr);
  ASSERT(block->ptr == ptr);
  block->ptr = 0;
  *ptr = 0;
  if (block < first_free) {
    first_free = block;
  }	   
  if (block_offset(block, block->len) == block_end) {
    block_end = block;
  }
  CHECK_INTERGRITY();
}

int
mem_compact_one()
{
  if (first_free != block_end) { /* Check if compacting is needed */
    block_ptr block = block_offset(first_free, first_free->len);
    while(block != block_end && !block->ptr) { /* Merge free blocks */
      first_free->len += block->len;
      block = block_offset(block, block->len);
    }
    if (block != block_end) {
      /* Move block down */
      mem_size_t free_len = first_free->len;
      memmove(first_free, block, block->len);
      block = first_free;
      *block->ptr = block_offset(block,sizeof(struct mem_block));
      first_free = block_offset(block, block->len);
      first_free->ptr = 0;
      first_free->len = free_len;
    } else {
      block_end = first_free;
    }
    CHECK_INTERGRITY();
    return 1;
  } else {
    return 0;
  }
}
