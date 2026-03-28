#!/bin/bash
set -xue

QEMU=qemu-system-riscv32
OBJCOPY=llvm-objcopy

# 1. Build the user-space shell
(cd user && cargo build)

# 2. Convert user ELF to flat binary
$OBJCOPY --set-section-flags .bss=alloc,contents -O binary \
    target/riscv32imac-unknown-none-elf/debug/user-rust user.bin

# 3. Build the kernel (includes user.bin)
(cd kernel && cargo build)

# 4. Create disk image (from ver.c/disk if it exists)
if [ -d "../ver.c/disk" ]; then
    (cd ../ver.c/disk && tar cf ../../ver.rust/disk.tar --format=ustar *.txt)
else
    # Create an empty tar if disk dir doesn't exist
    touch empty.txt
    tar cf disk.tar --format=ustar empty.txt
    rm empty.txt
fi

# 5. Run in QEMU
$QEMU -machine virt -bios default -nographic -serial mon:stdio --no-reboot \
    -drive id=drive0,file=disk.tar,format=raw,if=none \
    -device virtio-blk-device,drive=drive0,bus=virtio-mmio-bus.0 \
    -kernel target/riscv32imac-unknown-none-elf/debug/os-rust
