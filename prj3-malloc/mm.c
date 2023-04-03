/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    /* Your student ID */
    "20171680",
    /* Your full name*/
    "Hosung Lee",
    /* Your email address */
    "kennyya@naver.com",
};

/* single word (4) or double word (8) alignment */

#define WSIZE       4       /* Word and header/footer size (bytes) */ //line:vm:mm:beginconst
#define DSIZE       8       /* Doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* Extend heap by this amount (bytes) */  //line:vm:mm:endconst 
#define INITCHUNKSIZE (1<<6)

#define ALIGNMENT 8
/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define MAX(x, y) ((x) > (y)? (x) : (y))  
#define MIN(x, y) ((x) < (y)? (x) : (y)) 

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc)  ((size) | (alloc)) //line:vm:mm:pack

/* Read and write a word at address p */
#define GET(p)       (*(unsigned int *)(p))            //line:vm:mm:get
#define PUT(p, val)  (*(unsigned int *)(p) = (val))    //line:vm:mm:put
/* Read the size and allocated fields from address p */
#define GET_SIZE(p)  (GET(p) & ~0x7)                   //line:vm:mm:getsize
#define GET_ALLOC(p) (GET(p) & 0x1)                    //line:vm:mm:getalloc


/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp)       ((char *)(bp) - WSIZE)                      //line:vm:mm:hdrp
#define FTRP(bp)       ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE) //line:vm:mm:ftrp

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp)  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE))) //line:vm:mm:nextblkp
#define PREV_BLKP(bp)  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) //line:vm:mm:prevblkp

#define SET_PTR(p, ptr) (*(unsigned int *)(p) = (unsigned int)(ptr))

#define NEXT_PTR(ptr) ((char *)(ptr))
#define PREV_PTR(ptr) ((char *)(ptr) + WSIZE)

#define LIST_NEXT(ptr) (*(char **)(ptr))
#define LIST_PREV(ptr) (*(char **)(PREV_PTR(ptr)))

#define REALLOC_BUFFER  (1<<7)

#define MM_CHECKINGx
/* $end mallocmacros */

/* Function prototypes for internal helper routines */
static void *extend_heap(size_t words);
static void insert_node(void* ptr, size_t size);
static void delete_node(void *ptr);
static void *coalesce(void *bp);
static void *place(void *bp, size_t asize);

#ifdef MM_CHECKING
static void mm_checker(void);
#endif
/* Global variables */

void *exp_listp = 0; // free list

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    char *heap_s;

    exp_listp = NULL;


    if ((heap_s = mem_sbrk(4*WSIZE)) == (void *)-1) //line:vm:mm:begininit
	    return -1;

   
    PUT(heap_s,0);                          //Align
    PUT(heap_s + (WSIZE * 1),PACK(DSIZE,1));  //Prologue header
    PUT(heap_s + (WSIZE * 2),PACK(DSIZE,1));  //Prologue footer
    PUT(heap_s + (WSIZE * 3),PACK(0,1));      //Epilogue header
    
    
    if (extend_heap(INITCHUNKSIZE) == NULL) 
	    return -1;
    return 0;
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize = ALIGN(size + SIZE_T_SIZE); // algined size
    size_t extendsize;
    void *ptr = NULL;
    
    if(size == 0)
        return NULL;
    
    if(size < DSIZE)
        asize = 2 * DSIZE;
    else
        asize = ALIGN(size + DSIZE);
    
    ptr = exp_listp;
    while((ptr != NULL) && ((asize > GET_SIZE(HDRP(ptr)))))
        ptr = LIST_NEXT(ptr);
    
    if(ptr == NULL){
        extendsize = MAX(asize,CHUNKSIZE);
        if((ptr = extend_heap(extendsize)) == NULL)
            return NULL;
    }

    ptr = place(ptr, asize);
    
    #ifdef MM_CHECKING
    mm_checker();
    #endif

    return ptr;

}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));
    PUT(HDRP(ptr), PACK(size,0));
    PUT(FTRP(ptr), PACK(size,0));
    
    insert_node(ptr,size);
    coalesce(ptr);

    return;
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *new_ptr = ptr;
    size_t asize = size;
    int remainder;
    int extendsize;

    if(size == 0)
        return NULL;

    if(asize <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = ALIGN(size + DSIZE);

    asize += REALLOC_BUFFER;

    if(GET_SIZE(HDRP(ptr)) < asize){
        if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr))) || !GET_SIZE(HDRP(NEXT_BLKP(ptr)))){
            remainder = GET_SIZE(HDRP(ptr)) + GET_SIZE(HDRP(NEXT_BLKP(ptr))) - asize;
            if(remainder < 0){
                extendsize = MAX(-remainder, CHUNKSIZE);
                if(extend_heap(extendsize) == NULL)
                    return NULL;
                remainder += extendsize;
            }

            delete_node(NEXT_BLKP(ptr));

            PUT(HDRP(ptr),PACK(asize + remainder,1));
            PUT(FTRP(ptr),PACK(asize + remainder,1));        
        }
        else{
            new_ptr = mm_malloc(asize - DSIZE);
            memcpy(new_ptr,ptr,MIN(size,asize));
            mm_free(ptr);
        }
    }
    
    #ifdef MM_CHECKING
    mm_checker();
    #endif

    return new_ptr;

}


static void *extend_heap(size_t size){
    void *bp;
    size_t asize;

    asize = ALIGN(size);

    if((bp = mem_sbrk(asize)) == (void *)-1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(asize, 0));         /* Free block header */   //line:vm:mm:freeblockhdr
    PUT(FTRP(bp), PACK(asize, 0));         /* Free block footer */   //line:vm:mm:freeblockftr
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */ //line:vm:mm:newepihdr
    
    insert_node(bp,asize);
    return coalesce(bp);
}

static void insert_node(void* ptr, size_t size){
    void *cur_ptr = ptr;
    void *prev_ptr = NULL;

    // 오름차순으로 정렬
    cur_ptr = exp_listp;
    while ((cur_ptr != NULL) && (size > GET_SIZE(HDRP(cur_ptr)))) {
        prev_ptr = cur_ptr;
        cur_ptr = LIST_NEXT(cur_ptr);
    }

    // 전,후 4가지 경우
    if (cur_ptr != NULL) { // list 맨 뒤 아님
        if (prev_ptr != NULL) { //  list 중간
            SET_PTR(NEXT_PTR(ptr), cur_ptr);
            SET_PTR(PREV_PTR(cur_ptr), ptr);
            SET_PTR(PREV_PTR(ptr), prev_ptr);
            SET_PTR(NEXT_PTR(prev_ptr), ptr);
        } else { // list 처음
            SET_PTR(NEXT_PTR(ptr), cur_ptr);
            SET_PTR(PREV_PTR(cur_ptr), ptr);
            SET_PTR(PREV_PTR(ptr), NULL);
            exp_listp = ptr;
        }
    } else { // list 맨 끝
        if (prev_ptr != NULL) { // 기존 list에 원소 있다
            SET_PTR(NEXT_PTR(ptr), NULL);
            SET_PTR(PREV_PTR(ptr), prev_ptr);
            SET_PTR(NEXT_PTR(prev_ptr), ptr);
        } else { // 기존 list에 원소 없다.
            SET_PTR(NEXT_PTR(ptr), NULL);
            SET_PTR(PREV_PTR(ptr), NULL);
            exp_listp = ptr;
        }
    }
    return;
}

static void delete_node(void* ptr) {

    if (LIST_NEXT(ptr) != NULL) { // next node 존재
        if (LIST_PREV(ptr) != NULL) { //중간 node 삭제
            SET_PTR(PREV_PTR(LIST_NEXT(ptr)), LIST_PREV(ptr));
            SET_PTR(NEXT_PTR(LIST_PREV(ptr)), LIST_NEXT(ptr));
        }
        else { // 처음 node 삭제
            SET_PTR(PREV_PTR(LIST_NEXT(ptr)), NULL);
            exp_listp = LIST_NEXT(ptr);
        }
    }
    else { // next node 없음
        if (LIST_PREV(ptr) != NULL) { // 마지막 node 삭제
            SET_PTR(NEXT_PTR(LIST_PREV(ptr)), NULL);
        }
        else { // 유일 node 삭제
            exp_listp = NULL;
        }
    }
    return;
}

static void *coalesce(void *bp){
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    
    if (prev_alloc && next_alloc) {                         // both alloc
        return bp;
    }
    else if (prev_alloc && !next_alloc) {                   // prev alloc next free
        delete_node(bp);
        delete_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    else if (!prev_alloc && next_alloc) {                 // prev free next alloc
        delete_node(bp);
        delete_node(PREV_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        
        bp = PREV_BLKP(bp);
    }
    else {                                                // both free
        delete_node(bp);
        delete_node(PREV_BLKP(bp));
        delete_node(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    
    insert_node(bp,size);

    return bp;
}

static void* place(void* bp, size_t asize) {
    size_t bp_size = GET_SIZE(HDRP(bp));
    size_t remainder = bp_size - asize;

    delete_node(bp);

    if (remainder <= DSIZE * 2) {
        PUT(HDRP(bp), PACK(bp_size, 1));
        PUT(FTRP(bp), PACK(bp_size, 1));
    }
    else if (asize >= 100) {
        PUT(HDRP(bp), PACK(remainder, 0));
        PUT(FTRP(bp), PACK(remainder, 0));
        
        PUT(HDRP(NEXT_BLKP(bp)), PACK(asize, 1));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(asize, 1));
        insert_node(bp, remainder); // 남은 free block은 free list
        return NEXT_BLKP(bp); // 할당한 block 반환
    }
    else {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        PUT(HDRP(NEXT_BLKP(bp)), PACK(remainder, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(remainder, 0));
        insert_node(NEXT_BLKP(bp), remainder);
    }

    return bp;
}
#ifdef MM_CHECKING
static void mm_checker(void){
    void *ptr;
    ptr = exp_listp;
    //printf("mm_checking@@@@@@@@@@@@@@@@@@\n");
    // Is every block in the free list marked as free?
    while(ptr!=NULL){
        if(GET_ALLOC(ptr)){
            printf("Allocated block in free list\n");
            exit(-1);
        }
        ptr = LIST_NEXT(ptr); 
    }

}
#endif