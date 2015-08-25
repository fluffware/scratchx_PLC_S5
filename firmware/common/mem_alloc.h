#ifndef __MEM_ALLOC_H__LA5D9SP98Z__
#define __MEM_ALLOC_H__LA5D9SP98Z__

#include <stdint.h>
#ifndef __SDCC
#define __xdata
#endif

typedef uint16_t mem_size_t;

/* Memory block header */
struct mem_block
{
  __xdata void * __xdata *ptr; /* Points to the pointer that points to
				  this block*/
  mem_size_t len; /* Length includes this header */
};

extern __xdata uint8_t *mem_heap_start;
extern __xdata uint8_t *mem_heap_end;

void
mem_init(void);

/* Returns a memory block at least as big as len. */
__xdata void *
mem_alloc(__xdata void * __xdata *ptr, mem_size_t len);

__xdata void *
mem_realloc(__xdata void *  __xdata *ptr, mem_size_t len);

void
mem_free(__xdata void *  __xdata *ptr);

/* Compact one memory block. Returns true if compacting was needed. */
int
mem_compact_one();

/* Actual size of memory block */
#define MEM_LEN(p) ((((struct mem_block*)p)[-1]).len - sizeof(struct mem_block))

#endif /* __MEM_ALLOC_H__LA5D9SP98Z__ */
