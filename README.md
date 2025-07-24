# Requirements

 * make
 * nasm
 * gcc/binutils targeted for x86_64-elf
 * grub
   * grub-pc-bin (For Legacy BIOS)
   * grub-efi-amd64-bin (For UEFI)
   * grub-efi-ia32-bin (For 32bit UEFI)
 * qemu-system-x86_64 (For `make run`, `make debug`)
   * OVMF (For UEFI emulation)
   * gdb targeted for x86_64-elf (For `make gdb`)
 * bochs (For `make bochs`)
