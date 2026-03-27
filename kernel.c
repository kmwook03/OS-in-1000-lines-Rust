#include "kernel.h"
#include "common.h"

// 링커에서 정의된 심볼들
extern char __bss[], __bss_end[], __stack_top[];
extern char __free_ram[], __free_ram_end[];
extern char __kernel_base[];
extern char _binary_shell_bin_start[], _binary_shell_bin_size[];

void fs_flush(void);

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

long getchar(void) {
    struct sbiret ret = sbi_call(0, 0, 0, 0, 0, 0, 0, 2);
    return ret.error;
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


void map_page(uint32_t *table1, uint32_t vaddr, paddr_t paddr, uint32_t flags) {
    if (!is_aligned(vaddr, PAGE_SIZE))
        PANIC("unaligned vaddr %x", vaddr);

    if (!is_aligned(paddr, PAGE_SIZE))
        PANIC("unaligned paddr %x", paddr);
    
    uint32_t vpn1 = (vaddr >> 22) & 0x3FF; // 상위 10비트
    if ((table1[vpn1] && PAGE_V) == 0) { // 페이지 테이블이 아직 할당되지 않은 경우
        uint32_t pt_paddr = alloc_pages(1); // 페이지 테이블용 페이지 할당
        table1[vpn1] = ((pt_paddr / PAGE_SIZE) << 10) | PAGE_V; // 페이지 테이블 엔트리 설정
    }

    uint32_t vpn0 = (vaddr >> 12) & 0x3FF; // 하위 10비트
    uint32_t *table0 = (uint32_t *) ((table1[vpn1] >> 10) * PAGE_SIZE); // 페이지 테이블의 가상 주소 계산
    table0[vpn0] = ((paddr / PAGE_SIZE) << 10) | flags | PAGE_V; // 페이지 엔트리 설정
}

__attribute__((naked))
void user_entry(void) {
    __asm__ __volatile__(
        "csrw sepc, %[sepc]\n"
        "csrw sstatus, %[sstatus]\n"
        "sret\n"
        :
        : [sepc] "r" (USER_BASE), 
          [sstatus] "r" (SSTATUS_SPIE | SSTATUS_SUM)
    );
}

struct process procs[PROCS_MAX];

struct process *create_process(const void *image , size_t image_size) {
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
    *--sp = (uint32_t) user_entry;          // ra (처음 실행 시 점프할 주소)

    // Map kernel pages (identity mapping)
    uint32_t *page_table = (uint32_t *) alloc_pages(1);
    for (paddr_t paddr = (paddr_t) __kernel_base;
         paddr < (paddr_t) __free_ram_end; paddr += PAGE_SIZE) {
        map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);
    }
    map_page(page_table, VIRTIO_BLK_PADDR, VIRTIO_BLK_PADDR, PAGE_R | PAGE_W);

    // Map user pages
    for (uint32_t off = 0; off < image_size; off += PAGE_SIZE) {
        paddr_t page = alloc_pages(1);

        // 페이지 사이즈보다 작은 크기의 데이터가 복사되는 경우 처리
        size_t remaining = image_size - off;
        size_t copy_size = PAGE_SIZE <= remaining ? PAGE_SIZE : remaining;

        memcpy((void *) page, image + off, copy_size);
        map_page(page_table, USER_BASE + off, page,
                 PAGE_U | PAGE_R | PAGE_W | PAGE_X);
    }
    // 구조체 필드 초기화
    proc->pid = i + 1;
    proc->state = PROC_RUNNABLE;
    proc->sp = (uint32_t) sp;
    proc->page_table = page_table;
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
        "sfence.vma\n"
        "csrw satp, %[satp]\n"
        "sfence.vma\n"
        "csrw sscratch, %[sscratch]\n"
        :
        : [satp] "r" (SATP_SV32 | ((uint32_t) next->page_table / PAGE_SIZE)),
          [sscratch] "r" ((uint32_t) &next->stack[sizeof(next->stack)])
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

void sys_getchar(struct trap_frame *f) {
    while (1) {
        long ch = getchar();
        if (ch >= 0) {
            f->a0 = ch;
            return;
        }
        yield();
    }
}

void sys_putchar(struct trap_frame *f) {
    putchar(f->a0);
}

void sys_exit(struct trap_frame *f) {
    printf("process %d exited\n", current_proc->pid);
    current_proc->state = PROC_EXITED;
    yield();
    PANIC("unreachable");
}


struct file files[FILES_MAX];
uint8_t disk[DISK_MAX_SIZE];

struct file *fs_lookup(const char *filename) {
    for (int i=0; i<FILES_MAX; i++) {
        struct file *file = &files[i];
        if (!strcmp(file->name, filename))
            return file;
    }

    return NULL;
}

void sys_readfile(struct trap_frame *f) {
    const char *filename = (const char *) f->a0;
    char *buf = (char *) f->a1;
    int len = f->a2;

    struct file *file = fs_lookup(filename);
    if (!file) {
        f->a0 = -1; // 파일이 존재하지 않음
        return;
    }

    if (len > (int) sizeof(file->data))
        len = file->size; // 읽을 수 있는 최대 크기로 조정

    memcpy(buf, file->data, len);
    f->a0 = len; // 읽은 바이트 수 반환
    return;
}

void sys_writefile(struct trap_frame *f) {
    const char *filename = (const char *) f->a0;
    const char *buf = (const char *) f->a1;
    int len = f->a2;

    struct file *file = fs_lookup(filename);
    if (!file) {
        f->a0 = -1; // 파일이 존재하지 않음
        return;
    }

    if (len > (int) sizeof(file->data))
        len = sizeof(file->data); // 쓸 수 있는 최대 크기로 조정

    memcpy(file->data, buf, len);
    file->size = len; // 파일 크기 업데이트
    fs_flush();
    f->a0 = len; // 쓴 바이트 수 반환
    return;
}


typedef void (*syscall_fn_t) (struct trap_frame *);

syscall_fn_t syscall_table[MAX_SYSCALL] = {
    [SYS_PUTCHAR] = sys_putchar,
    [SYS_GETCHAR] = sys_getchar,
    [SYS_EXIT]    = sys_exit,
    [SYS_READFILE] = sys_readfile,
    [SYS_WRITEFILE] = sys_writefile,
};

void handle_syscall(struct trap_frame *f) {
    int sysno = f->a3;

    if (sysno >= 0 && sysno < MAX_SYSCALL)
        syscall_table[sysno](f);
    else
        PANIC("invalid syscall no %d", sysno);
}

void handle_trap(struct trap_frame *f) {
    uint32_t scause = READ_CSR(scause);
    uint32_t stval = READ_CSR(stval);
    uint32_t user_pc = READ_CSR(sepc);

    if (scause == SCAUSE_ECALL) {
        handle_syscall(f);
        user_pc += 4; // ecall 명령어 다음으로 이동
    } else {
        PANIC("unexpected trap scause=%x, stval=%x, sepc=%x\n", scause, stval, user_pc);
    }

    WRITE_CSR(sepc, user_pc);
}

// volatile 필수
uint32_t virtio_reg_read32(unsigned offset) {
    return *((volatile uint32_t *) (VIRTIO_BLK_PADDR + offset));
}

uint64_t virtio_reg_read64(unsigned offset) {
    return *((volatile uint64_t *) (VIRTIO_BLK_PADDR + offset));
}

void virtio_reg_write32(unsigned offset, uint32_t value) {
    *((volatile uint32_t *) (VIRTIO_BLK_PADDR + offset)) = value;
}

void virtio_reg_fetch_and_or32(unsigned offset, uint32_t value) {
    virtio_reg_write32(offset, virtio_reg_read32(offset) | value);
}

struct virtio_virtq *blk_request_vq;
struct virtio_blk_req *blk_req;
paddr_t blk_req_paddr;
uint64_t blk_capacity;

struct virtio_virtq *virtq_init(unsigned index) {
    paddr_t virtq_paddr = alloc_pages(align_up(sizeof(struct virtio_virtq), PAGE_SIZE) / PAGE_SIZE);
    struct virtio_virtq *vq = (struct virtio_virtq *) virtq_paddr;
    vq->queue_index = index;
    vq->used_index = (volatile uint16_t *) &vq->used.index;
    // 큐 선택: virtqueue 인덱스를 기록 (첫 번째 큐는 0)
    virtio_reg_write32(VIRTIO_REG_QUEUE_SEL, index);
    // 큐 크기 지정: 사용할 디스크립터 개수를 기록
    virtio_reg_write32(VIRTIO_REG_QUEUE_NUM, VIRTQ_ENTRY_NUM);
    // 큐의 페이지 프레임 번호를 기록 (물리 주소가 아님)
    virtio_reg_write32(VIRTIO_REG_QUEUE_PFN, virtq_paddr / PAGE_SIZE);
    return vq;
}

void virtio_blk_init(void) {
    if (virtio_reg_read32(VIRTIO_REG_MAGIC) != 0x74726976)
        PANIC("virtio: invalid magic value");
    if (virtio_reg_read32(VIRTIO_REG_VERSION) != 1)
        PANIC("virtio: invalid version");
    if (virtio_reg_read32(VIRTIO_REG_DEVICE_ID) != VIRTIO_DEVICE_BLK)
        PANIC("virtio: invalid device ID");

    virtio_reg_write32(VIRTIO_REG_DEVICE_STATUS, 0); // reset
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_ACK); // acknowledge
    virtio_reg_fetch_and_or32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER); // driver
    virtio_reg_write32(VIRTIO_REG_PAGE_SIZE, PAGE_SIZE); // 페이지 크기 설정
    blk_request_vq = virtq_init(0); // disk rw queue 초기화
    virtio_reg_write32(VIRTIO_REG_DEVICE_STATUS, VIRTIO_STATUS_DRIVER_OK); // driver ok

    blk_capacity = virtio_reg_read64(VIRTIO_REG_DEVICE_CONFIG + 0) * SECTOR_SIZE;
    printf("virtio block device capacity: %d bytes\n", (int)blk_capacity);

    blk_req_paddr = alloc_pages(align_up(sizeof(*blk_req), PAGE_SIZE) / PAGE_SIZE);
    blk_req = (struct virtio_blk_req *) blk_req_paddr;
}

void virtq_kick(struct virtio_virtq *vq, int desc_index) {
    vq->avail.ring[vq->avail.index % VIRTQ_ENTRY_NUM] = desc_index; // 사용 가능한 디스크립터 인덱스를 avail.ring에 기록
    vq->avail.index++; // avail.index 증가
    __sync_synchronize(); // 메모리 동기화
    virtio_reg_write32(VIRTIO_REG_QUEUE_NOTIFY, vq->queue_index);
    vq->last_used_index++;
}

bool virtq_is_busy(struct virtio_virtq *vq) {
    return vq->last_used_index != *vq->used_index;
}

void read_write_disk(void *buf, unsigned sector, int is_write) {
    if (sector >= blk_capacity / SECTOR_SIZE) {
        printf("virtio: tried to read/write sector %d, but capacity is only %d bytes\n", sector, blk_capacity / SECTOR_SIZE);
        return;
    }

    blk_req->sector = sector;
    blk_req->type = is_write ? VIRTIO_BLK_T_OUT : VIRTIO_BLK_T_IN;
    if (is_write)
        memcpy(blk_req->data, buf, SECTOR_SIZE);
    
    struct virtio_virtq *vq = blk_request_vq;
    vq->descs[0].addr = blk_req_paddr;
    vq->descs[0].len = sizeof(uint32_t) * 2 + sizeof(uint64_t);
    vq->descs[0].flags = VIRTQ_DESC_F_NEXT;
    vq->descs[0].next = 1;

    vq->descs[1].addr = blk_req_paddr + offsetof(struct virtio_blk_req, data);
    vq->descs[1].len = SECTOR_SIZE;
    vq->descs[1].flags = VIRTQ_DESC_F_NEXT | (is_write ? 0 : VIRTQ_DESC_F_WRITE);
    vq->descs[1].next = 2;

    vq->descs[2].addr = blk_req_paddr + offsetof(struct virtio_blk_req, status);
    vq->descs[2].len = sizeof(uint8_t);
    vq->descs[2].flags = VIRTQ_DESC_F_WRITE;

    virtq_kick(vq, 0); // 디스크 요청을 큐에 추가

    while (virtq_is_busy(vq))
        ;
    
    if (blk_req->status != 0) {
        printf("virtio: warn: failed to read/write sector=%d, status=%d\n", sector, blk_req->status);
        return;
    }

    if (!is_write)
        memcpy(buf, blk_req->data, SECTOR_SIZE);
}

int oct2int(const char *oct, int len) {
    int dec = 0;
    for (int i=0; i<len; i++) {
        if (oct[i] < '0' || oct[i] > '7')
            break;
        dec = dec * 8 + (oct[i] - '0');
    }
    return dec;
}

void fs_init(void) {
    for (unsigned sector = 0; sector < sizeof(disk) / SECTOR_SIZE; sector++)
        read_write_disk(&disk[sector * SECTOR_SIZE], sector, false);

    unsigned off = 0;
    for (int i = 0; i < FILES_MAX; i++) {
        struct tar_header *header = (struct tar_header *) &disk[off];
        if (header->name[0] == '\0')
            break;

        if (strcmp(header->magic, "ustar") != 0)
            PANIC("invalid tar header: magic=\"%s\"", header->magic);

        int filesz = oct2int(header->size, sizeof(header->size));
        struct file *file = &files[i];
        file->in_use = true;
        strcpy(file->name, header->name);
        memcpy(file->data, header->data, filesz);
        file->size = filesz;
        printf("file: %s, size=%d\n", file->name, file->size);

        off += align_up(sizeof(struct tar_header) + filesz, SECTOR_SIZE);
    }
}

void fs_flush(void) {
    // Copy all file contents into `disk` buffer.
    memset(disk, 0, sizeof(disk));
    unsigned off = 0;
    for (int file_i = 0; file_i < FILES_MAX; file_i++) {
        struct file *file = &files[file_i];
        if (!file->in_use)
            continue;

        struct tar_header *header = (struct tar_header *) &disk[off];
        memset(header, 0, sizeof(*header));
        strcpy(header->name, file->name);
        strcpy(header->mode, "000644");
        strcpy(header->magic, "ustar");
        strcpy(header->version, "00");
        header->type = '0';

        // Turn the file size into an octal string.
        int filesz = file->size;
        for (int i = sizeof(header->size); i > 0; i--) {
            header->size[i - 1] = (filesz % 8) + '0';
            filesz /= 8;
        }

        // Calculate the checksum.
        int checksum = ' ' * sizeof(header->checksum);
        for (unsigned i = 0; i < sizeof(struct tar_header); i++)
            checksum += (unsigned char) disk[off + i];

        for (int i = 5; i >= 0; i--) {
            header->checksum[i] = (checksum % 8) + '0';
            checksum /= 8;
        }

        // Copy file data.
        memcpy(header->data, file->data, file->size);
        off += align_up(sizeof(struct tar_header) + file->size, SECTOR_SIZE);
    }

    // Write `disk` buffer into the virtio-blk.
    for (unsigned sector = 0; sector < sizeof(disk) / SECTOR_SIZE; sector++)
        read_write_disk(&disk[sector * SECTOR_SIZE], sector, true);

    printf("wrote %d bytes to disk\n", sizeof(disk));
}

void kernel_main() {
    memset(__bss, 0, (size_t) __bss_end - (size_t) __bss); // BSS 섹션 초기화
    WRITE_CSR(stvec, (uint32_t) kernel_entry);

    virtio_blk_init();
    fs_init();

    char buf[SECTOR_SIZE];
    read_write_disk(buf, 0, false); // 첫 번째 섹터 읽기
    printf("first sector of disk: %s\n", buf);

    strcpy(buf, "Hello, virtio disk!\n");
    read_write_disk(buf, 0, true); // 첫 번째 섹터에 쓰기

    idle_proc = create_process(NULL, 0);
    idle_proc->pid = 0;
    current_proc = idle_proc;

    create_process(_binary_shell_bin_start, (size_t) _binary_shell_bin_size);
    
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
