#pragma once

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef uint32_t size_t;
typedef uint32_t paddr_t;   // 물리 메모리 주소
typedef uint32_t vaddr_t;   // 가상 메모리 주소

#define true 1
#define false 0
#define NULL ((void *)0)
#define PAGE_SIZE 4096 // 4KB
// align은 2의 거듭제곱이어야 함
#define align_up(value, align) __builtin_align_up(value, align) // value를 align의 배수로 맞춰 올림
#define is_aligned(value, align) __builtin_is_aligned(value, align) // value가 align의 배수인지 확인
#define offsetof(type, member) __builtin_offsetof(type, member) // 구조체 내에서 특정 멤버가 시작되는 위치 반환
#define va_list  __builtin_va_list
#define va_start __builtin_va_start
#define va_end   __builtin_va_end
#define va_arg   __builtin_va_arg

void *memset(void *buf, char c, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
char *strcpy(char *dst, const char *src);
int strcmp(const char *s1, const char *s2);
void printf(const char *fmt, ...);
