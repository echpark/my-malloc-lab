#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************
 * 팀 정보 
 **********************************/

team_t team = {
    /* 팀명 */
    "Team6",

    /* 팀원 1 */
    "Hyeonho Cho",
    "joho0504@gmail.com",

    /* 팀원 2 */
    "Harin Lee",
    "gbs1823@gmail.com",

    /* 팀원 3 */
    "Eunchae Park",
    "ghkqh09@gmail.com"
};


/*********************************
 * 정렬 및 크기 관련 매크로
 **********************************/
#define ALIGNMENT 8                                     /* 8바이트 정렬 기준 */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)   /* size를 8바이트 배수로 올림 */
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))             /* size_t 크기도 8바이트 정렬 */

#define WSIZE      4                                    /* 워드 크기 (헤더/풋터 크기) */
#define DSIZE      8                                    /* 더블 워드 크기 */
#define CHUNKSIZE (1<<12)                               /* 힙 확장 단위: 4KB */

#define MAX(x,y) ((x) > (y) ? (x) : (y))                /* 두 값 중 큰 값 반환 */

/*********************************
 * 헤더/풋터 조작 매크로
 **********************************/
#define PACK(size, alloc)   ((size) | (alloc))                             /* 크기와 할당 비트를 합침 */
#define GET(p)              (*(unsigned int *)(p))                         /* p 주소의 워드 읽기 */
#define PUT(p, val)         (*(unsigned int *)(p) = (val))                 /* p 주소에 워드 쓰기 */

#define GET_SIZE(p)  (GET(p) & ~0x7)                                       /* 상위 비트: 블록 크기 */
#define GET_ALLOC(p) (GET(p) & 0x1)                                        /* 하위 1비트: 할당 여부 */

#define HDRP(bp) ((char *)(bp) - WSIZE)                                    /* bp 블록의 헤더 주소 */
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)               /* bp 블록의 풋터 주소 */

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))    /* 다음 블록의 bp */
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))    /* 이전 블록의 bp */

/* 프롤로그 블록의 payload 시작을 가리킴 */
static char *heap_listp = 0;

/* 내부 함수 선언 */
static void  *extend_heap(size_t words);
static void  *coalesce(void *bp);
static void  *find_fit(size_t asize);
static void   place(void *bp, size_t asize);

/*
 * mm_init - 할당기 초기화
 *           성공 시 0, 실패 시 -1 반환
 */
int mm_init(void)
{
    /* 힙의 시작 부분에 4 워드(16바이트) 확보 */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;
    PUT(heap_listp, 0);                            /* 패딩 워드 */
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1));       /* 프롤로그 헤더 */
    PUT(heap_listp + 2*WSIZE, PACK(DSIZE, 1));     /* 프롤로그 풋터 */
    PUT(heap_listp + 3*WSIZE, PACK(0, 1));         /* 에필로그 헤더 */
    heap_listp += 2*WSIZE;                         /* heap_listp를 프로로그 payload 위치로 이동 */

    /* CHUNKSIZE 만큼 힙 확장 (첫 번째 free 블록 생성) */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    return 0;
}

/*
 * extend_heap - 힙을 words 워드만큼 확장하고 새 free 블록 생성
 */
static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* 8바이트 정렬을 위해 워드 수를 짝수로 조정 */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((bp = mem_sbrk(size)) == (void *)-1)
        return NULL;

    /* 새 free 블록의 헤더/풋터 초기화 */
    PUT(HDRP(bp), PACK(size, 0));       /* free 헤더 */
    PUT(FTRP(bp), PACK(size, 0));       /* free 풋터 */
    
    /* 새 에필로그 헤더 설정 */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    /* 이전 블록이 free라면 합치기 */
    return coalesce(bp);
}

/*
 * coalesce - 인접한 free 블록과 병합
 */
static void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp))); /* 이전 블록 할당 여부 */
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp))); /* 다음 블록 할당 여부 */
    size_t size       = GET_SIZE(HDRP(bp));             /* 현재 블록 크기 */

    if (prev_alloc && next_alloc) {
        /* Case 1: 앞뒤 모두 할당 */
        return bp;
    }
    else if (prev_alloc && !next_alloc) {
        /* Case 2: 뒤 블록만 free */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size,0));
        PUT(FTRP(bp), PACK(size,0));
    }
    else if (!prev_alloc && next_alloc) {
        /* Case 3: 앞 블록만 free */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    else {
        /* Case 4: 앞뒤 모두 free */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)))
              + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    return bp;
}

/*
 * mm_malloc - size 바이트 크기의 블록을 할당
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* 조정된 블록 크기 */
    size_t extendsize; /* 확장할 크기 */
    char *bp;

    if (size == 0)
        return NULL;

    /* 블록 크기를 DSIZE 배수로 맞춤 */
    if (size <= DSIZE)
        asize = 2*DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE)+(DSIZE-1)) / DSIZE);

    /* first-fit 탐색 */
    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    /* 적당한 블록 없으면 힙 확장 */
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE)) == NULL)
        return NULL;

    place(bp, asize);
    return bp;
}

/*
 * mm_free - ptr이 가리키는 블록 해제
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));   /* 블록 크기 가져오기 */

    PUT(HDRP(ptr), PACK(size,0));        /* 헤더에 free 표시 */
    PUT(FTRP(ptr), PACK(size,0));        /* 풋터에 free 표시 */
    coalesce(ptr);                       /* 인접 free 블록과 병합 */
}

/*
 * mm_realloc - 간단히 mm_malloc + mm_free로 구현
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (ptr == NULL)                      /* NULL realloc은 malloc과 동등 */
        return mm_malloc(size);
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    /* 새 블록 할당 후, 데이터 복사하고 이전 블록 해제 */
    void *newptr = mm_malloc(size);
    if (newptr == NULL)
        return NULL;
    size_t oldsize = GET_SIZE(HDRP(ptr));
    if (size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);
    mm_free(ptr);
    return newptr;
}

/*
 * find_fit - 묵시적 리스트에서 first-fit 탐색
 *            헤더 크기가 0이 될 때까지 순차 탐색
 */
static void *find_fit(size_t asize)
{
    char *bp = heap_listp;

    while (GET_SIZE(HDRP(bp)) != 0) {
        if (!GET_ALLOC(HDRP(bp)) && (asize <= GET_SIZE(HDRP(bp)))) {
            return bp;
        }
        bp = NEXT_BLKP(bp);
    }
    return NULL;  /* 적합 블록 없음 */
}

/*
 * place - bp 위치에 asize 크기로 블록 배치, 분할 가능 시 나머지 free 블록 생성
 */
static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));   /* 현재 블록 전체 크기 */

    if ((csize - asize) >= (2*DSIZE)) {
        /* 분할 가능한 경우 */
        PUT(HDRP(bp), PACK(asize,1));             /* 할당된 블록 헤더 */
        PUT(FTRP(bp), PACK(asize,1));             /* 할당된 블록 풋터 */
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize,0));       /* 남은 공간 free 블록 헤더 */
        PUT(FTRP(bp), PACK(csize-asize,0));       /* 남은 공간 free 블록 풋터 */
    } else {
        /* 분할 불가 시 전체 할당 */
        PUT(HDRP(bp), PACK(csize,1));
        PUT(FTRP(bp), PACK(csize,1));
    }
}