#pragma once
#include "common.h"

// 패닉이 발생한 소스 파일과 줄 번호를 출력한 후 무한 루프에 빠지는 패닉 처리 매크로
#define PANIC(fmt, ...)                                                         \
    do {                                                                        \
        printf("PANIC: %s:%d: " fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__);   \
        while (1) {}                                                            \
    } while (0)

struct sbiret {
    long error;
    long value;
};

// kernel_entry에서 저장한 프로그램 상태를 나타내는 구조체
struct trap_frame {
    uint32_t ra;
    uint32_t gp;
    uint32_t tp;
    uint32_t t0;
    uint32_t t1;
    uint32_t t2;
    uint32_t t3;
    uint32_t t4;
    uint32_t t5;
    uint32_t t6;
    uint32_t a0;
    uint32_t a1;
    uint32_t a2;
    uint32_t a3;
    uint32_t a4;
    uint32_t a5;
    uint32_t a6;
    uint32_t a7;
    uint32_t s0;
    uint32_t s1;
    uint32_t s2;
    uint32_t s3;
    uint32_t s4;
    uint32_t s5;
    uint32_t s6;
    uint32_t s7;
    uint32_t s8;
    uint32_t s9;
    uint32_t s10;
    uint32_t s11;
    uint32_t sp;
} __attribute__((packed));

#define READ_CSR(reg)                                           \
    ({                                                          \
        unsigned long __tmp;                                    \
        __asm__ __volatile__("csrr %0, " #reg : "=r"(__tmp));   \
        __tmp;                                                  \
    })

#define WRITE_CSR(reg, val)                                         \
    do {                                                            \
        uint32_t __tmp = (val);                                     \
        __asm__ __volatile__("csrw " #reg ", %0" :: "r"(__tmp));    \
    } while (0)

#define SATP_SV32 (1u << 31)
#define PAGE_V  (1 << 0) // valid bit
#define PAGE_R  (1 << 1) // read permission
#define PAGE_W  (1 << 2) // write permission
#define PAGE_X  (1 << 3) // execute permission
#define PAGE_U  (1 << 4) // user-accessible bit

#define PROCS_MAX 8 // 최대 프로세스 수
#define PROC_UNUSED 0
#define PROC_RUNNABLE 1 // 실행 가능한 프로세스

struct process {
    int pid;
    int state;
    vaddr_t sp;
    uint32_t *page_table; // 페이지 테이블의 물리 주소
    uint8_t stack[8192]; // 커널 스택 (8KB)
};

