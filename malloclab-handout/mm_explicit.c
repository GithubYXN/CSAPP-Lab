/*
 * mm-implicit.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"
#include "mm.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "explicit",
    /* First member's full name */
    "Yang",
    /* First member's email address */
    "2502869657@qq.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

// #define NEXT_FIT
/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* basic constants and macros */
#define WSIZE 4
#define DSIZE 8
#define CHUNKSIZE (1 << 12)

#define MAX(x, y) ((x) > (y) ? (x) : (y))

/* pack size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* read and write a word at address p */
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* get block size and allocated bit with address p */
#define GET_SIZE(p) (GET(p) & (~0x7))
#define GET_ALLOC(p) (GET(p) & 0x1)

/* given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* given block ptr bp, compute address of its next and prev block */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE((char *)(bp) - WSIZE))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE))

/* given block ptr bp, compute address of its prev and next ptr */
#define PRECEED(bp) (bp)
#define SUCCEED(bp) ((char *)(bp) + WSIZE)

/* read and write a ptr at address p */
#define GET_PTR(p) (*(unsigned int **)(p))
#define PUT_PTR(p, val) (*(unsigned int **)(p) = (unsigned int *)(val))

/* heap ptr */
static void *heap_ptr = 0;

/* helper function prototype */
static void *extend_heap(size_t bytes);
static void *coalesce(void *bp);
static void *find_fit(size_t size);
static void place(void *bp, size_t size);
static inline void insert_into_list(void *bp);
static inline void remove_from_list(void *bp);
static void mm_checkheap(void *bp);

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
  if ((heap_ptr = mem_sbrk(3 * DSIZE)) == (void *)-1) {
    return -1;
  }

  PUT(heap_ptr, 0);
  PUT(heap_ptr + (1 * WSIZE), PACK(2 * DSIZE, 1));
  PUT_PTR(heap_ptr + (2 * WSIZE), NULL);
  PUT_PTR(heap_ptr + (3 * WSIZE), NULL);
  PUT(heap_ptr + (4 * WSIZE), PACK(2 * DSIZE, 1));
  PUT(heap_ptr + (5 * WSIZE), PACK(0, 1));
  heap_ptr += 2 * WSIZE;

  if (extend_heap(CHUNKSIZE) == NULL) {
    return -1;
  }

  return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size) {
  size_t asize, extendsize;
  void *bp;

  if (size == 0) {
    return NULL;
  }

  if (size <= DSIZE) {
    asize = 3 * DSIZE;
  } else {
    asize = ALIGN(size) + 2 * DSIZE; /* aligned size plus footer and header */
  }

  if ((bp = find_fit(asize)) != NULL) {
    place(bp, asize);
    return bp;
  }

  extendsize = MAX(asize, CHUNKSIZE);
  if ((bp = extend_heap(extendsize)) == NULL) {
    return NULL;
  }
  place(bp, asize);
  return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp) {
  if (bp == NULL) {
    return;
  }

  size_t size = GET_SIZE(HDRP(bp));

  PUT(HDRP(bp), PACK(size, 0));
  PUT(FTRP(bp), PACK(size, 0));

  coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size) {
  size_t oldsize, newsize;
  void *newptr;

  if (size == 0) {
    mm_free(ptr);
    return NULL;
  }
  if (ptr == NULL) {
    return mm_malloc(size);
  }

  newptr = mm_malloc(size);
  if (newptr == NULL) {
    return NULL;
  }

  oldsize = GET_SIZE(HDRP(ptr));
  newsize = GET_SIZE(HDRP(newptr));
  if (newsize < oldsize) {
    oldsize = newsize;
  }
  memcpy(newptr, ptr, oldsize - 2 * DSIZE);

  mm_free(ptr);

  return newptr;
}

/*********************
 *  helper functions
 ********************/

/* extend heap with free block and return its ptr */
static void *extend_heap(size_t bytes) {
  void *bp;
  if ((bp = mem_sbrk(bytes)) == (void *)-1) {
    return NULL;
  }

  PUT(HDRP(bp), PACK(bytes, 0));
  PUT(FTRP(bp), PACK(bytes, 0));
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

  return coalesce(bp);
}

static void *coalesce(void *bp) {
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));
  void *prev_blk = PREV_BLKP(bp);
  void *next_blk = NEXT_BLKP(bp);

  if (prev_alloc && next_alloc) {
    insert_into_list(bp);
    return bp;
  } else if (prev_alloc && !next_alloc) {
    remove_from_list(next_blk);
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    PUT(HDRP(bp), PACK(size, 0));
    insert_into_list(bp);
  } else if (!prev_alloc && next_alloc) {
    remove_from_list(prev_blk);
    size += GET_SIZE(FTRP(PREV_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    bp = PREV_BLKP(bp);
    insert_into_list(bp);
  } else {
    remove_from_list(prev_blk);
    remove_from_list(next_blk);
    size += GET_SIZE(FTRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
    insert_into_list(bp);
  }
  return bp;
}

static void *find_fit(size_t size) {
  void *bp;
  for (bp = heap_ptr; bp && SUCCEED(bp); bp = GET_PTR(SUCCEED(bp))) {
    if (GET_SIZE(HDRP(bp)) >= size) {
      return bp;
    }
  }
  return NULL;
}

static void place(void *bp, size_t size) {
  size_t bsize, lsize;
  bsize = GET_SIZE(HDRP(bp));
  lsize = bsize - size;

  remove_from_list(bp);
  if (lsize >= 3 * DSIZE) {
    PUT(HDRP(bp), PACK(size, 1));
    PUT(FTRP(bp), PACK(size, 1));
    bp = NEXT_BLKP(bp);
    PUT(HDRP(bp), PACK(lsize, 0));
    PUT(FTRP(bp), PACK(lsize, 0));
    insert_into_list(bp);
  } else {
    PUT(HDRP(bp), PACK(bsize, 1));
    PUT(FTRP(bp), PACK(bsize, 1));
  }
}

static inline void insert_into_list(void *bp) {
#ifdef LIFO
  void *heap_next_blk = GET_PTR(SUCCEED(heap_ptr));
  if (heap_next_blk) {
    PUT_PTR(SUCCEED(heap_ptr), bp);
    PUT_PTR(PRECEED(heap_next_blk), bp);
    PUT_PTR(PRECEED(bp), heap_ptr);
    PUT_PTR(SUCCEED(bp), heap_next_blk);
  } else {
    PUT_PTR(SUCCEED(heap_ptr), bp);
    PUT_PTR(PRECEED(bp), heap_ptr);
    PUT_PTR(SUCCEED(bp), NULL);
  }
#else
  void *ptr;
  void *ptr_prev;
  for (ptr = heap_ptr; ptr && SUCCEED(ptr); ptr = GET_PTR(SUCCEED(ptr))) {
    if (GET(bp) < GET(ptr)) {
      break;
    }
    ptr_prev = ptr;
  }
  if (ptr) {
    PUT_PTR(PRECEED(bp), ptr_prev);
    PUT_PTR(SUCCEED(bp), ptr);
    PUT_PTR(PRECEED(ptr), bp);
    PUT_PTR(SUCCEED(ptr_prev), bp);
  } else {
    PUT_PTR(PRECEED(bp), ptr_prev);
    PUT_PTR(SUCCEED(bp), NULL);
    PUT_PTR(SUCCEED(ptr_prev), bp);
  }
#endif
}

static inline void remove_from_list(void *bp) {
  void *bp_prev_blk = GET_PTR(PRECEED(bp));
  void *bp_next_blk = GET_PTR(SUCCEED(bp));
  if (bp_next_blk) {
    PUT_PTR(SUCCEED(bp_prev_blk), bp_next_blk);
    PUT_PTR(PRECEED(bp_next_blk), bp_prev_blk);
  } else {
    PUT_PTR(SUCCEED(bp_prev_blk), NULL);
  }
}

static void mm_checkheap(void *bp) {
  printf("bp is: %p\n", bp);
  printf("bp_prev_ptr is: %p\n", PRECEED(bp));
  printf("bp_next_ptr is: %p\n", SUCCEED(bp));
  printf("bp_prev_blk is: %p\n", GET_PTR(PRECEED(bp)));
  printf("bp_next_blk is: %p\n", GET_PTR(SUCCEED(bp)));
}
