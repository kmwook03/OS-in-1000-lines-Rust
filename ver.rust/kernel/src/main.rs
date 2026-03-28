#![no_std]
#![no_main]

use core::panic::PanicInfo;
use core::arch::asm;
use core::fmt::{self, Write};

struct Console;

impl Write for Console {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        for ch in s.chars() {
            putchar(ch);
        }
        Ok(())
    }
}

pub fn _print(args: fmt::Arguments) {
    let mut console = Console;
    console.write_fmt(args).unwrap();
}

#[macro_export]
macro_rules! print {
    ($($arg:tt)*) => {
        $crate::_print(format_args!($($arg)*));
    };
}

#[macro_export]
macro_rules! println {
    () => ($crate::print!("\n"));
    ($($arg:tt)*) => ({
        $crate::print!("{}\n", format_args!($($arg)*));
    })
}

#[panic_handler]
fn panic(info: &PanicInfo) -> ! {
    println!("{}", info);
    loop {}
}

pub const PAGE_SIZE: usize = 4096;

pub const SATP_SV32: u32 = 1 << 31;
pub const PAGE_V: u32 = 1 << 0;
pub const PAGE_R: u32 = 1 << 1;
pub const PAGE_W: u32 = 1 << 2;
pub const PAGE_X: u32 = 1 << 3;
pub const PAGE_U: u32 = 1 << 4;

pub const SSTATUS_SPIE: u32 = 1 << 5;
pub const SSTATUS_SUM: u32 = 1 << 18;
pub const SCAUSE_ECALL: u32 = 8;

macro_rules! read_csr {
    ($reg:expr) => {{
        let tmp: usize;
        unsafe {
            asm!(concat!("csrr {0}, ", $reg), out(reg) tmp);
        }
        tmp
    }};
}

macro_rules! write_csr {
    ($reg:expr, $val:expr) => {
        let val = $val as usize;
        unsafe {
            asm!(concat!("csrw ", $reg, ", {0}"), in(reg) val);
        }
    };
}

pub struct SbiRet {
    pub error: isize,
    pub value: isize,
}

#[inline]
pub fn sbi_call(
    arg0: isize,
    arg1: isize,
    arg2: isize,
    arg3: isize,
    arg4: isize,
    arg5: isize,
    fid: isize,
    eid: isize,
) -> SbiRet {
    let mut a0 = arg0;
    let mut a1 = arg1;
    unsafe {
        asm!(
            "ecall",
            inout("x10") a0,
            inout("x11") a1,
            in("x12") arg2,
            in("x13") arg3,
            in("x14") arg4,
            in("x15") arg5,
            in("x16") fid,
            in("x17") eid,
        );
    }
    SbiRet { error: a0, value: a1 }
}

pub fn putchar(ch: char) {
    sbi_call(ch as isize, 0, 0, 0, 0, 0, 1, 1);
}

#[unsafe(no_mangle)]
#[unsafe(link_section = ".text.boot")]
pub unsafe extern "C" fn boot() -> ! {
    unsafe {
        asm!(
            "mv sp, {stack_top}",
            "j kernel_main",
            stack_top = in(reg) stack_top(),
        );
    }
    loop {}
}

unsafe extern "C" {
    static __stack_top: u8;
    static mut __bss: u8;
    static mut __bss_end: u8;
    static __free_ram: u8;
    static __free_ram_end: u8;
    static __kernel_base: u8;
}

static mut NEXT_PADDR: usize = 0;

fn is_aligned(value: usize, align: usize) -> bool {
    (value & (align - 1)) == 0
}

pub fn alloc_pages(n: usize) -> usize {
    unsafe {
        if NEXT_PADDR == 0 {
            NEXT_PADDR = &__free_ram as *const u8 as usize;
        }

        let paddr = NEXT_PADDR;
        NEXT_PADDR += n * PAGE_SIZE;

        if NEXT_PADDR > &__free_ram_end as *const u8 as usize {
            panic!("out of memory");
        }

        core::ptr::write_bytes(paddr as *mut u8, 0, n * PAGE_SIZE);
        paddr
    }
}

pub fn map_page(table1: *mut u32, vaddr: usize, paddr: usize, flags: u32) {
    if !is_aligned(vaddr, PAGE_SIZE) {
        panic!("unaligned vaddr {:x}", vaddr);
    }
    if !is_aligned(paddr, PAGE_SIZE) {
        panic!("unaligned paddr {:x}", paddr);
    }

    let vpn1 = (vaddr >> 22) & 0x3FF;
    unsafe {
        if (*table1.add(vpn1) & PAGE_V) == 0 {
            let pt_paddr = alloc_pages(1);
            *table1.add(vpn1) = (((pt_paddr / PAGE_SIZE) << 10) as u32) | PAGE_V;
        }

        let vpn0 = (vaddr >> 12) & 0x3FF;
        let table0 = ((*table1.add(vpn1) >> 10) as usize * PAGE_SIZE) as *mut u32;
        *table0.add(vpn0) = (((paddr / PAGE_SIZE) << 10) as u32) | flags | PAGE_V;
    }
}

#[repr(C, packed)]
pub struct TrapFrame {
    pub ra: u32, pub gp: u32, pub tp: u32, pub t0: u32, pub t1: u32, pub t2: u32,
    pub t3: u32, pub t4: u32, pub t5: u32, pub t6: u32, pub a0: u32, pub a1: u32,
    pub a2: u32, pub a3: u32, pub a4: u32, pub a5: u32, pub a6: u32, pub a7: u32,
    pub s0: u32, pub s1: u32, pub s2: u32, pub s3: u32, pub s4: u32, pub s5: u32,
    pub s6: u32, pub s7: u32, pub s8: u32, pub s9: u32, pub s10: u32, pub s11: u32,
    pub sp: u32,
}

pub const PROCS_MAX: usize = 8;
pub const PROC_UNUSED: i32 = 0;
pub const PROC_RUNNABLE: i32 = 1;
pub const PROC_EXITED: i32 = 2;

#[repr(C)]
pub struct Process {
    pub pid: i32,
    pub state: i32,
    pub sp: u32,
    pub page_table: *mut u32,
    pub stack: [u8; 8192],
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn handle_trap(f: *mut TrapFrame) {
    let scause = read_csr!("scause");
    let stval = read_csr!("stval");
    let sepc = read_csr!("sepc");

    if scause == SCAUSE_ECALL as usize {
        handle_syscall(f);
        write_csr!("sepc", sepc + 4);
    } else {
        panic!("unexpected trap scause={:x}, stval={:x}, sepc={:x}", scause, stval, sepc);
    }
}

pub fn handle_syscall(f: *mut TrapFrame) {
    let f = unsafe { &mut *f };
    let sysno = f.a3;

    match sysno {
        SYS_PUTCHAR => {
            putchar(f.a0 as u8 as char);
        }
        SYS_GETCHAR => {
            loop {
                let ret = sbi_call(0, 0, 0, 0, 0, 0, 0, 2);
                let ch = ret.error;
                if ch >= 0 {
                    f.a0 = ch as u32;
                    return;
                }
                yield_proc();
            }
        }
        SYS_EXIT => {
            unsafe {
                println!("process {} exited", (*CURRENT_PROC).pid);
                (*CURRENT_PROC).state = PROC_EXITED;
            }
            yield_proc();
            panic!("unreachable");
        }
        _ => {
            panic!("invalid syscall no {}", sysno);
        }
    }
}

pub const SYS_PUTCHAR: u32 = 1;
pub const SYS_GETCHAR: u32 = 2;
pub const SYS_EXIT: u32 = 3;

#[unsafe(no_mangle)]
#[unsafe(link_section = ".text")]
pub unsafe extern "C" fn kernel_entry() {
    unsafe {
        asm!(
            ".align 4",
            "csrrw sp, sscratch, sp",
            "addi sp, sp, -4 * 31",
            "sw ra,  4 * 0(sp)",
            "sw gp,  4 * 1(sp)",
            "sw tp,  4 * 2(sp)",
            "sw t0,  4 * 3(sp)",
            "sw t1,  4 * 4(sp)",
            "sw t2,  4 * 5(sp)",
            "sw t3,  4 * 6(sp)",
            "sw t4,  4 * 7(sp)",
            "sw t5,  4 * 8(sp)",
            "sw t6,  4 * 9(sp)",
            "sw a0,  4 * 10(sp)",
            "sw a1,  4 * 11(sp)",
            "sw a2,  4 * 12(sp)",
            "sw a3,  4 * 13(sp)",
            "sw a4,  4 * 14(sp)",
            "sw a5,  4 * 15(sp)",
            "sw a6,  4 * 16(sp)",
            "sw a7,  4 * 17(sp)",
            "sw s0,  4 * 18(sp)",
            "sw s1,  4 * 19(sp)",
            "sw s2,  4 * 20(sp)",
            "sw s3,  4 * 21(sp)",
            "sw s4,  4 * 22(sp)",
            "sw s5,  4 * 23(sp)",
            "sw s6,  4 * 24(sp)",
            "sw s7,  4 * 25(sp)",
            "sw s8,  4 * 26(sp)",
            "sw s9,  4 * 27(sp)",
            "sw s10, 4 * 28(sp)",
            "sw s11, 4 * 29(sp)",

            "csrr a0, sscratch",
            "sw a0, 4 * 30(sp)",

            "addi a0, sp, 4 * 31",
            "csrw sscratch, a0",

            "mv a0, sp",
            "call handle_trap",

            "lw ra,  4 * 0(sp)",
            "lw gp,  4 * 1(sp)",
            "lw tp,  4 * 2(sp)",
            "lw t0,  4 * 3(sp)",
            "lw t1,  4 * 4(sp)",
            "lw t2,  4 * 5(sp)",
            "lw t3,  4 * 6(sp)",
            "lw t4,  4 * 7(sp)",
            "lw t5,  4 * 8(sp)",
            "lw t6,  4 * 9(sp)",
            "lw a0,  4 * 10(sp)",
            "lw a1,  4 * 11(sp)",
            "lw a2,  4 * 12(sp)",
            "lw a3,  4 * 13(sp)",
            "lw a4,  4 * 14(sp)",
            "lw a5,  4 * 15(sp)",
            "lw a6,  4 * 16(sp)",
            "lw a7,  4 * 17(sp)",
            "lw s0,  4 * 18(sp)",
            "lw s1,  4 * 19(sp)",
            "lw s2,  4 * 20(sp)",
            "lw s3,  4 * 21(sp)",
            "lw s4,  4 * 22(sp)",
            "lw s5,  4 * 23(sp)",
            "lw s6,  4 * 24(sp)",
            "lw s7,  4 * 25(sp)",
            "lw s8,  4 * 26(sp)",
            "lw s9,  4 * 27(sp)",
            "lw s10, 4 * 28(sp)",
            "lw s11, 4 * 29(sp)",
            "lw sp,  4 * 30(sp)",
            "sret",
            options(noreturn)
        );
    }
}

#[unsafe(no_mangle)]
pub unsafe extern "C" fn switch_context(prev_sp: *mut u32, next_sp: *const u32) {
    unsafe {
        asm!(
            "addi sp, sp, -13 * 4",
            "sw ra,  0  * 4(sp)",
            "sw s0,  1  * 4(sp)",
            "sw s1,  2  * 4(sp)",
            "sw s2,  3  * 4(sp)",
            "sw s3,  4  * 4(sp)",
            "sw s4,  5  * 4(sp)",
            "sw s5,  6  * 4(sp)",
            "sw s6,  7  * 4(sp)",
            "sw s7,  8  * 4(sp)",
            "sw s8,  9  * 4(sp)",
            "sw s9,  10 * 4(sp)",
            "sw s10, 11 * 4(sp)",
            "sw s11, 12 * 4(sp)",
            "sw sp, (x10)",
            "lw sp, (x11)",
            "lw ra,  0  * 4(sp)",
            "lw s0,  1  * 4(sp)",
            "lw s1,  2  * 4(sp)",
            "lw s2,  3  * 4(sp)",
            "lw s3,  4  * 4(sp)",
            "lw s4,  5  * 4(sp)",
            "lw s5,  6  * 4(sp)",
            "lw s6,  7  * 4(sp)",
            "lw s7,  8  * 4(sp)",
            "lw s8,  9  * 4(sp)",
            "lw s9,  10 * 4(sp)",
            "lw s10, 11 * 4(sp)",
            "lw s11, 12 * 4(sp)",
            "addi sp, sp, 13 * 4",
            "ret",
            in("x10") prev_sp,
            in("x11") next_sp,
        );
    }
}

pub static mut PROCS: [Process; PROCS_MAX] = unsafe { core::mem::zeroed() };
pub static mut CURRENT_PROC: *mut Process = core::ptr::null_mut();
pub static mut IDLE_PROC: *mut Process = core::ptr::null_mut();

pub const USER_BASE: usize = 0x10000000;

#[unsafe(no_mangle)]
pub unsafe extern "C" fn user_entry() {
    unsafe {
        asm!(
            "csrw sepc, {sepc}",
            "csrw sstatus, {sstatus}",
            "sret",
            sepc = in(reg) USER_BASE,
            sstatus = in(reg) (SSTATUS_SPIE | SSTATUS_SUM),
            options(noreturn)
        );
    }
}

pub fn create_process(image: *const u8, image_size: usize) -> *mut Process {
    unsafe {
        let mut proc: *mut Process = core::ptr::null_mut();
        for i in 0..PROCS_MAX {
            if PROCS[i].state == PROC_UNUSED {
                proc = &mut PROCS[i];
                break;
            }
        }

        if proc.is_null() {
            panic!("no free process slot");
        }

        let p = &mut *proc;
        let mut sp = (p.stack.as_ptr() as usize + p.stack.len()) as *mut u32;
        
        sp = sp.offset(-1); *sp = 0; // s11
        sp = sp.offset(-1); *sp = 0; // s10
        sp = sp.offset(-1); *sp = 0; // s9
        sp = sp.offset(-1); *sp = 0; // s8
        sp = sp.offset(-1); *sp = 0; // s7
        sp = sp.offset(-1); *sp = 0; // s6
        sp = sp.offset(-1); *sp = 0; // s5
        sp = sp.offset(-1); *sp = 0; // s4
        sp = sp.offset(-1); *sp = 0; // s3
        sp = sp.offset(-1); *sp = 0; // s2
        sp = sp.offset(-1); *sp = 0; // s1
        sp = sp.offset(-1); *sp = 0; // s0
        sp = sp.offset(-1); *sp = user_entry as *const () as u32; // ra

        let page_table = alloc_pages(1) as *mut u32;

        // Identity map kernel
        let mut paddr = &__kernel_base as *const u8 as usize;
        while paddr < NEXT_PADDR {
            map_page(page_table, paddr, paddr, PAGE_R | PAGE_W | PAGE_X);
            paddr += PAGE_SIZE;
        }

        // Map user pages
        let mut off = 0;
        while off < image_size {
            let page = alloc_pages(1);
            let remaining = image_size - off;
            let copy_size = if PAGE_SIZE <= remaining { PAGE_SIZE } else { remaining };

            core::ptr::copy_nonoverlapping(image.add(off), page as *mut u8, copy_size);
            map_page(page_table, USER_BASE + off, page, PAGE_U | PAGE_R | PAGE_W | PAGE_X);
            off += PAGE_SIZE;
        }

        p.pid = (proc as usize - core::ptr::addr_of_mut!(PROCS) as usize) as i32 / core::mem::size_of::<Process>() as i32 + 1;
        p.state = PROC_RUNNABLE;
        p.sp = sp as u32;
        p.page_table = page_table;
        proc
    }
}

pub fn yield_proc() {
    unsafe {
        let mut next = IDLE_PROC;
        for i in 0..PROCS_MAX {
            let proc = &mut PROCS[((*CURRENT_PROC).pid as usize + i) % PROCS_MAX];
            if proc.state == PROC_RUNNABLE && proc.pid > 0 {
                next = proc;
                break;
            }
        }

        if next == CURRENT_PROC {
            return;
        }

        asm!(
            "sfence.vma",
            "csrw satp, {satp}",
            "sfence.vma",
            "csrw sscratch, {sscratch}",
            satp = in(reg) (SATP_SV32 | (((*next).page_table as usize / PAGE_SIZE) as u32)),
            sscratch = in(reg) ((*next).stack.as_ptr() as usize + (*next).stack.len()),
        );

        let prev = CURRENT_PROC;
        CURRENT_PROC = next;
        switch_context(&mut (*prev).sp, &(*next).sp);
    }
}

fn stack_top() -> usize {
    unsafe { &__stack_top as *const u8 as usize }
}

#[unsafe(no_mangle)]
pub extern "C" fn kernel_main() {
    unsafe {
        let bss = &raw mut __bss;
        let bss_end = &raw mut __bss_end;
        let len = bss_end as usize - bss as usize;
        core::ptr::write_bytes(bss, 0, len);
    }
    
    write_csr!("stvec", kernel_entry as *const () as usize);

    println!("Hello, OS in Rust!");
    
    unsafe {
        IDLE_PROC = create_process(core::ptr::null(), 0);
        (*IDLE_PROC).pid = 0;
        CURRENT_PROC = IDLE_PROC;

        static SHELL_BIN: &[u8] = include_bytes!("../../user.bin");
        create_process(SHELL_BIN.as_ptr(), SHELL_BIN.len());
    }

    yield_proc();
    panic!("switched to idle process!");
}
