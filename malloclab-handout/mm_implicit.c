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
    "implicit",
    /* First member's full name */
    "Yang",
    /* First member's email address */
    "2502869657@qq.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

#define NEXT_FIT
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

/* heap ptr */
static void *heap_ptr = 0;
#ifdef NEXT_FIT
static void *prev_ptr = 0;
#endif

/* helper function prototype */
static void *extend_heap(size_t bytes);
static void *coalesce(void *bp);
static void *find_fit(size_t size);
static void place(void *bp, size_t size);

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void) {
  if ((heap_ptr = mem_sbrk(4 * WSIZE)) == (void *)-1) {
    return -1;
  }

  PUT(heap_ptr, 0);
  PUT(heap_ptr + (1 * WSIZE), PACK(DSIZE, 1));
  PUT(heap_ptr + (2 * WSIZE), PACK(DSIZE, 1));
  PUT(heap_ptr + (3 * WSIZE), PACK(0, 1));
  heap_ptr += (2 * WSIZE);
#ifdef NEXT_FIT
  prev_ptr = heap_ptr;
#endif

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
    asize = 2 * DSIZE;
  } else {
    asize = ALIGN(size) + DSIZE; /* aligned size plus footer and header */
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
  memcpy(newptr, ptr, oldsize - DSIZE);

  mm_free(ptr);

  return newptr;
}

/*********************
 *  helper functions
 ********************/

/* extend heap with free block and return its ptr */
static void *extend_heap(size_t bytes) {
  void *bp;
  size_t asize = ALIGN(bytes);
  if ((bp = mem_sbrk(asize)) == (void *)-1) {
    return NULL;
  }

  PUT(HDRP(bp), PACK(asize, 0));
  PUT(FTRP(bp), PACK(asize, 0));
  PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

  return coalesce(bp);
}

static void *coalesce(void *bp) {
  size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  size_t size = GET_SIZE(HDRP(bp));

  if (prev_alloc && next_alloc) {
    return bp;
  } else if (prev_alloc && !next_alloc) {
    size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    PUT(HDRP(bp), PACK(size, 0));
  } else if (!prev_alloc && next_alloc) {
    size += GET_SIZE(FTRP(PREV_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    bp = PREV_BLKP(bp);
  } else {
    size += GET_SIZE(FTRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
    PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
    bp = PREV_BLKP(bp);
  }
#ifdef NEXT_FIT
  if ((prev_ptr > bp) && (prev_ptr < (void *)NEXT_BLKP(bp))) {
    prev_ptr = bp;
  }
#endif
  return bp;
}

static void *find_fit(size_t size) {
#ifdef NEXT_FIT
  void *oldptr = prev_ptr;

  for ( ; GET_SIZE(HDRP(prev_ptr)) > 0; prev_ptr = NEXT_BLKP(prev_ptr)) {
    if (!GET_ALLOC(HDRP(prev_ptr)) && GET_SIZE(HDRP(prev_ptr)) >= size) {
      return prev_ptr;
    }
  }
  for (prev_ptr = heap_ptr; prev_ptr < oldptr; prev_ptr = NEXT_BLKP(prev_ptr)) {
    if (!GET_ALLOC(HDRP(prev_ptr)) && GET_SIZE(HDRP(prev_ptr)) >= size) {
      return prev_ptr;
    }
  }
  return NULL;
#else
  void *bp;
  for (bp = heap_ptr; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    if (!GET_ALLOC(HDRP(bp)) && GET_SIZE(HDRP(bp)) >= size) {
      return bp;
    }
  }
  return NULL;
#endif
}

static void place(void *bp, size_t size) {
  size_t bsize, lsize;
  bsize = GET_SIZE(HDRP(bp));
  lsize = bsize - size;
  if (lsize >= 2 * DSIZE) {
    PUT(HDRP(bp), PACK(size, 1));
    PUT(FTRP(bp), PACK(size, 1));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(lsize, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(lsize, 0));
  } else {
    PUT(HDRP(bp), PACK(bsize, 1));
    PUT(FTRP(bp), PACK(bsize, 1));
  }
}
