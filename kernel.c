#include "kernel.h"
#include "common.h"

// 링커에서 정의된 심볼들
extern char __bss[], __bss_end[], __stack_top[];
extern char __free_ram[], __free_ram_end[];

struct sbiret sbi_call(long arg0, long arg1, long arg2, long arg3,
                        long arg4, long arg5, long fid, long eid) {
    register long a0 __asm__("a0") = arg0;
    register long a1 __asm__("a1") = arg1;
    register long a2 __asm__("a2") = arg2;
    register long a3 __asm__("a3") = arg3;
    register long a4 __asm__("a4") = arg4;
    register long a5 __asm__("a5") = arg5;
    register long a6 __asm__("a6") = fid;
    register long a7 __asm__("a7") = eid;

    __asm__ __volatile__("ecall"
                        : "=r"(a0), "=r"(a1)
                        : "r"(a0), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5),
                          "r"(a6), "r"(a7)
                        : "memory");
    return (struct sbiret){.error = a0, .value = a1};
}

void putchar(char c) {
    sbi_call(c, 0, 0, 0, 0, 0, 1, 1); // SBI_CALL_1(SBI_CONSOLE_PUTCHAR, c)
}

void handle_trap(struct trap_frame *f) {
    uint32_t scause = READ_CSR(scause);
    uint32_t stval = READ_CSR(stval);
    uint32_t user_pc = READ_CSR(sepc);

    PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
}

// stvec 레지스터에 핸들러 진입점을 등록
__attribute__((naked))
__attribute__((aligned(4)))
void kernel_entry(void) {
    __asm__ __volatile__(
        "csrrw sp, sscratch, sp\n"       // sscratch 레지스터를 임시 저장소로 사용하여 예외 발생 지점 스택 포인터 저장
        "addi sp, sp, -4 * 31\n"
        "sw ra,  4 * 0(sp)\n"
        "sw gp,  4 * 1(sp)\n"
        "sw tp,  4 * 2(sp)\n"
        "sw t0,  4 * 3(sp)\n"
        "sw t1,  4 * 4(sp)\n"
        "sw t2,  4 * 5(sp)\n"
        "sw t3,  4 * 6(sp)\n"
        "sw t4,  4 * 7(sp)\n"
        "sw t5,  4 * 8(sp)\n"
        "sw t6,  4 * 9(sp)\n"
        "sw a0,  4 * 10(sp)\n"
        "sw a1,  4 * 11(sp)\n"
        "sw a2,  4 * 12(sp)\n"
        "sw a3,  4 * 13(sp)\n"
        "sw a4,  4 * 14(sp)\n"
        "sw a5,  4 * 15(sp)\n"
        "sw a6,  4 * 16(sp)\n"
        "sw a7,  4 * 17(sp)\n"
        "sw s0,  4 * 18(sp)\n"
        "sw s1,  4 * 19(sp)\n"
        "sw s2,  4 * 20(sp)\n"
        "sw s3,  4 * 21(sp)\n"
        "sw s4,  4 * 22(sp)\n"
        "sw s5,  4 * 23(sp)\n"
        "sw s6,  4 * 24(sp)\n"
        "sw s7,  4 * 25(sp)\n"
        "sw s8,  4 * 26(sp)\n"
        "sw s9,  4 * 27(sp)\n"
        "sw s10, 4 * 28(sp)\n"
        "sw s11, 4 * 29(sp)\n"

        // 예외 발생 시점의 sp를 sscratch 레지스터에 저장
        "csrr a0, sscratch\n"
        "sw a0, 4 * 30(sp)\n"

        // 커널 스택 초기화
        "addi a0, sp, 4 * 31\n"
        "csrw sscratch, a0\n"

        "mv a0, sp\n"
        "call handle_trap\n"

        "lw ra,  4 * 0(sp)\n"
        "lw gp,  4 * 1(sp)\n"
        "lw tp,  4 * 2(sp)\n"
        "lw t0,  4 * 3(sp)\n"
        "lw t1,  4 * 4(sp)\n"
        "lw t2,  4 * 5(sp)\n"
        "lw t3,  4 * 6(sp)\n"
        "lw t4,  4 * 7(sp)\n"
        "lw t5,  4 * 8(sp)\n"
        "lw t6,  4 * 9(sp)\n"
        "lw a0,  4 * 10(sp)\n"
        "lw a1,  4 * 11(sp)\n"
        "lw a2,  4 * 12(sp)\n"
        "lw a3,  4 * 13(sp)\n"
        "lw a4,  4 * 14(sp)\n"
        "lw a5,  4 * 15(sp)\n"
        "lw a6,  4 * 16(sp)\n"
        "lw a7,  4 * 17(sp)\n"
        "lw s0,  4 * 18(sp)\n"
        "lw s1,  4 * 19(sp)\n"
        "lw s2,  4 * 20(sp)\n"
        "lw s3,  4 * 21(sp)\n"
        "lw s4,  4 * 22(sp)\n"
        "lw s5,  4 * 23(sp)\n"
        "lw s6,  4 * 24(sp)\n"
        "lw s7,  4 * 25(sp)\n"
        "lw s8,  4 * 26(sp)\n"
        "lw s9,  4 * 27(sp)\n"
        "lw s10, 4 * 28(sp)\n"
        "lw s11, 4 * 29(sp)\n"
        "lw sp,  4 * 30(sp)\n"
        "sret\n"
    );
}

paddr_t alloc_pages(uint32_t n) {
    static paddr_t next_paddr = 0;
    if (next_paddr == 0)
        next_paddr = (paddr_t) __free_ram;

    paddr_t paddr = next_paddr;
    next_paddr += n * PAGE_SIZE;

    if (next_paddr > (paddr_t) __free_ram_end)
        PANIC("out of memory");
    
    memset((void *) paddr, 0, n * PAGE_SIZE); // 할당된 페이지를 0으로 초기화
    return paddr;
}

#define PROCS_MAX 8 // 최대 프로세스 수
#define PROC_UNUSED 0
#define PROC_RUNNABLE 1 // 실행 가능한 프로세스

struct process {
    int pid;
    int state;
    vaddr_t sp;
    uint8_t stack[8192]; // 커널 스택 (8KB)
};

__attribute__((naked)) void switch_context(uint32_t *prev_sp, uint32_t *next_sp) {
    __asm__ __volatile__(
        "addi sp, sp, -13 * 4\n"    // 13개 레지스터 공간 확보
        "sw ra, 0 * 4(sp)\n"        // callee-saved 레지스터 저장
        "sw s0,  1  * 4(sp)\n"
        "sw s1,  2  * 4(sp)\n"
        "sw s2,  3  * 4(sp)\n"
        "sw s3,  4  * 4(sp)\n"
        "sw s4,  5  * 4(sp)\n"
        "sw s5,  6  * 4(sp)\n"
        "sw s6,  7  * 4(sp)\n"
        "sw s7,  8  * 4(sp)\n"
        "sw s8,  9  * 4(sp)\n"
        "sw s9,  10 * 4(sp)\n"
        "sw s10, 11 * 4(sp)\n"
        "sw s11, 12 * 4(sp)\n"

        // 스택 포인터 교체
        "sw sp, (a0)\n" // 현재 스택 포인터 저장
        "lw sp, (a1)\n" // 다음 스택 포인터 로드

        // 다음 프로세스 스택에서 callee-saved 레지스터 복원
        "lw ra,  0  * 4(sp)\n"  
        "lw s0,  1  * 4(sp)\n"
        "lw s1,  2  * 4(sp)\n"
        "lw s2,  3  * 4(sp)\n"
        "lw s3,  4  * 4(sp)\n"
        "lw s4,  5  * 4(sp)\n"
        "lw s5,  6  * 4(sp)\n"
        "lw s6,  7  * 4(sp)\n"
        "lw s7,  8  * 4(sp)\n"
        "lw s8,  9  * 4(sp)\n"
        "lw s9,  10 * 4(sp)\n"
        "lw s10, 11 * 4(sp)\n"
        "lw s11, 12 * 4(sp)\n"
        "addi sp, sp, 13 * 4\n" 
        "ret\n"
    );
}

struct process procs[PROCS_MAX];

struct process *create_process(uint32_t pc) {
    // 미사용 프로세스 구조체 찾기
    struct process *proc = NULL;
    int i;
    for (i=0; i<PROCS_MAX; i++) {
        if (procs[i].state == PROC_UNUSED) {
            proc = &procs[i];
            break;
        }
    }

    if (!proc)
        PANIC("no free process slot");
    
    // 커널 스택에 callee-saved 레지스터 공간 준비
    uint32_t *sp = (uint32_t *) &proc->stack[sizeof(proc->stack)];
    *--sp = 0;                      // s11
    *--sp = 0;                      // s10
    *--sp = 0;                      // s9
    *--sp = 0;                      // s8
    *--sp = 0;                      // s7
    *--sp = 0;                      // s6
    *--sp = 0;                      // s5
    *--sp = 0;                      // s4
    *--sp = 0;                      // s3
    *--sp = 0;                      // s2
    *--sp = 0;                      // s1
    *--sp = 0;                      // s0
    *--sp = (uint32_t) pc;          // ra (처음 실행 시 점프할 주소)

    // 구조체 필드 초기화
    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->sp = (uint32_t) sp;
    return proc;
}


struct process *current_proc;
struct process *idle_proc;

void yield(void) {
    // 실행 가능한 프로세스 탐색
    struct process *next = idle_proc;
    for (int i=0; i<PROCS_MAX; i++) {
        struct process *proc = &procs[(current_proc->pid + i) % PROCS_MAX];
        if (proc->state == PROC_RUNNABLE && proc->pid > 0) {
            next = proc;
            break;
        }
    }

    if (next == current_proc)
        return; // 다른 실행 가능한 프로세스가 없으면 그대로 실행

    __asm__ __volatile__(
        "csrw sscratch, %[sscratch]\n"
        :
        : [sscratch] "r" ((uint32_t) &next->stack[sizeof(next->stack)])
    );

    // 컨텍스트 스위칭
    struct process *prev = current_proc;
    current_proc = next;
    switch_context(&prev->sp, &next->sp);
}

void delay(void) {
    for (int i=0; i<3000000; i++)
        __asm__ __volatile__("nop");
}

struct process *proc_a;
struct process *proc_b;

void proc_a_entry(void) {
    printf("starting proccess A\n");
    while (1) {
        printf("A");
        yield();
    }
}

void proc_b_entry(void) {
    printf("starting proccess B\n");
    while (1) {
        printf("B");
        yield();
    }
}

void kernel_main() {
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss); // BSS 섹션 초기화

    printf("\n\n");

    WRITE_CSR(stvec, (uint32_t) kernel_entry);

    idle_proc = create_process((uint32_t) NULL);
    idle_proc->pid = 0;
    current_proc = idle_proc;

    proc_a = create_process((uint32_t) proc_a_entry);
    proc_b = create_process((uint32_t) proc_b_entry);
    
    yield(); // 첫 번째 프로세스 실행
    PANIC("switched to idle process!");
}

// Entry point
__attribute__((section(".text.boot"))) // .text.boot 섹션에 위치
__attribute__((naked)) // prologue/epilogue 생략
void boot(void) {
    __asm__ __volatile__(
        "mv sp, %[stack_top]\n" // 스택 포인터 설정
        "j kernel_main\n"       // kernel_main 함수로 점프
        :
        : [stack_top] "r" (__stack_top) // __stack_top 레이블에 %[stack_top] 전달
    );
}
