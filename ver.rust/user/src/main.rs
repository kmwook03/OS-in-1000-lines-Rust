#![no_std]
#![no_main]

use core::panic::PanicInfo;
use core::arch::{asm, naked_asm};
use core::fmt::{self, Write};

#[panic_handler]
fn panic(_info: &PanicInfo) -> ! {
    exit();
}

pub fn syscall(sysno: u32, arg0: usize, arg1: usize, arg2: usize) -> isize {
    let mut a0 = arg0 as isize;
    unsafe {
        asm!(
            "ecall",
            inout("x10") a0,
            in("x11") arg1,
            in("x12") arg2,
            in("x13") sysno,
        );
    }
    a0
}

#[unsafe(no_mangle)]
pub fn exit() -> ! {
    syscall(3, 0, 0, 0);
    loop {}
}

pub fn putchar(ch: char) {
    syscall(1, ch as usize, 0, 0);
}

pub fn getchar() -> char {
    loop {
        let ch = syscall(2, 0, 0, 0);
        if ch >= 0 {
            return ch as u8 as char;
        }
    }
}

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

unsafe extern "C" {
    static __stack_top: u8;
}

#[unsafe(no_mangle)]
#[unsafe(link_section = ".text.start")]
#[unsafe(naked)]
pub unsafe extern "C" fn start() -> ! {
    unsafe {
        naked_asm!(
            "la sp, __stack_top",
            "j main",
        );
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn main() {
    println!("Hello from Rust user space!");
    loop {
        print!("> ");
        let mut cmd = [0u8; 128];
        let mut i = 0;
        loop {
            let ch = getchar();
            putchar(ch);
            if ch == '\r' || ch == '\n' {
                println!();
                break;
            }
            if i < cmd.len() - 1 {
                cmd[i] = ch as u8;
                i += 1;
            }
        }
        let s = core::str::from_utf8(&cmd[..i]).unwrap_or("");
        if s == "hello" {
            println!("Hello world from shell!");
        } else if s == "exit" {
            exit();
        } else {
            println!("unknown command: {}", s);
        }
    }
}
