/*
 * memlib.c - a module that simulates the memory system.  Needed because it 
 *            allows us to interleave calls from the student's malloc package 
 *            with the system's malloc package in libc.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>

#include "memlib.h"
#include "config.h"

/* private variables */
static char *mem_start_brk;  /* 힙의 첫 번째 바이트를 가리키는 포인터 */
static char *mem_brk;        /* 현재 힙의 끝(마지막 바이트 + 1)을 가리키는 포인터 */
static char *mem_max_addr;   /* 허용 가능한 힙 최대 주소 + 1  */ 

/* 
 * mem_init - 메모리 시스템 모델 초기화
 */
void mem_init(void)
{
    /* allocate the storage we will use to model the available VM */
    if ((mem_start_brk = (char *)malloc(MAX_HEAP)) == NULL) {
        fprintf(stderr, "mem_init_vm: malloc error\n");
        exit(1);
    }

    mem_max_addr = mem_start_brk + MAX_HEAP;  /* 힙 최대 주소 설정 */
    mem_brk = mem_start_brk;                  /* 힙 끝을 힙 시작과 동일하게 초기화 */
}

/* 
 * mem_deinit - free the storage used by the memory system model
 */
void mem_deinit(void)
{
    free(mem_start_brk);
}

/*
 * mem_reset_brk - reset the simulated brk pointer to make an empty heap
 */
void mem_reset_brk()
{
    mem_brk = mem_start_brk;
}

/* 
 * mem_sbrk - sbrk 함수의 간단한 모델
 *            힙을 incr 바이트만큼 확장하고,
 *            새 영역의 시작 주소를 반환한다.
 *            이 모델에서는 힙을 줄일 수 없다.
 */
void *mem_sbrk(int incr) 
{
    char *old_brk = mem_brk;

    if ((incr < 0) || ((mem_brk + incr) > mem_max_addr)) {
        errno = ENOMEM; /* 메모리 부족 에러 설정 */
        fprintf(stderr, "ERROR: mem_sbrk failed. Ran out of memory...\n");
        return (void *)-1; /* 실패 시 -1 반환 */
    }

    mem_brk += incr; /* 힙 포인터를 incr만큼 증가 */
    return (void *)old_brk; /* 확장 전 힙 포인터 반환 */
}

/*
 * mem_heap_lo - return address of the first heap byte
 */
void *mem_heap_lo()
{
    return (void *)mem_start_brk;
}

/* 
 * mem_heap_hi - return address of last heap byte
 */
void *mem_heap_hi()
{
    return (void *)(mem_brk - 1);
}

/*
 * mem_heapsize() - returns the heap size in bytes
 */
size_t mem_heapsize() 
{
    return (size_t)(mem_brk - mem_start_brk);
}

/*
 * mem_pagesize() - returns the page size of the system
 */
size_t mem_pagesize()
{
    return (size_t)getpagesize();
}
