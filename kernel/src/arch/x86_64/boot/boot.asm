%define MAGIC 0xe85250d6
%define DISPLACEMENT (0xffffffff80000000 - 0x00200000)

extern __stack_top
extern __stack_top_lba
extern kmain_arch

section .multiboot

align 8
multiboot:
; header
dd MAGIC
dd 0
dd .end - multiboot
dd -(MAGIC + .end - multiboot)
; framebuffer
align 8, db 0
dw 5, 0
dd 20, 1024, 768, 32
; terminating tag
align 8, db 0
dd 0, 8
.end:

section .bss

align 4096
tmp_page: resb 0x6000

%define BOOTINFO_CMD_MAXLEN 256
%define BOOTINFO_MMAP_MAXLEN 128

align 16
global g_bootinfo_arch
g_bootinfo_arch:
bootinfo_cmd: resb BOOTINFO_CMD_MAXLEN
bootinfo_ptr_mmap: resb 8
bootinfo_fb: resb 24

align 8
bootinfo_mmap: resb 8 + 24 * BOOTINFO_MMAP_MAXLEN

section .startup alloc exec progbits

bits 32
global _start
_start:
    cli
    cld
    mov esp, __stack_top_lba

    ; check multiboot2 bootloader
    cmp eax, 0x36d76289
    jne panic

    ; copy required multiboot2 boot information
    mov ebp, ebx
    add ebp, [ebx]
    add ebx, 8
mbi_loop:
    mov eax, [ebx]
    cmp eax, 1  ; cmd
    je mbi_cmd
    cmp eax, 6  ; memmap
    je mbi_mmap
    cmp eax, 8  ; framebuffer
    je mbi_fb

mbi_cont:
    add ebx, [ebx+4]
    add ebx, 7
    and ebx, ~7
    cmp ebx, ebp
    jb mbi_loop
    jmp mbi_done

mbi_cmd:
    lea esi, [ebx+8]    ; cmd
    mov edi, bootinfo_cmd - DISPLACEMENT
    mov ecx, [ebx+4]    ; size
    sub ecx, 8
    mov eax, BOOTINFO_CMD_MAXLEN - 1
    cmp ecx, eax
    cmovg ecx, eax
    xor edx, edx
.loop:
    cmp edx, ecx
    jge mbi_cont
    lodsb
    test al, al
    jz mbi_cont
    stosb
    inc edx
    jmp .loop

mbi_mmap:
    mov edi, bootinfo_mmap - DISPLACEMENT + 8
    lea esi, [ebx+16]   ; entries
    mov edx, [ebx+4]    ; size
    add edx, ebx        ; end
    xor ecx, ecx
.loop:
    mov eax, esi
    add eax, [ebx+8]    ; entry size
    cmp eax, edx
    jae .done
    times 6 movsd
    mov esi, eax
    inc ecx
    cmp ecx, BOOTINFO_MMAP_MAXLEN
    jb .loop
.done:
    mov dword [bootinfo_mmap - DISPLACEMENT], ecx
    jmp mbi_cont

mbi_fb:
    mov edi, bootinfo_fb - DISPLACEMENT
    mov eax, [ebx+8]    ; fb_addr
    mov [edi], eax
    mov eax, [ebx+16]   ; fb_pitch
    mov [edi+4], eax
    mov eax, [ebx+20]   ; fb_width
    mov [edi+8], eax
    mov eax, [ebx+24]   ; fb_height
    mov [edi+12], eax
    mov ax, [ebx+28]    ; fb_bpp / fb_type
    mov [edi+16], ax
    jmp mbi_cont
mbi_done:

init_long:
    lgdt [gdtr]
    jmp 0x18:.gdt_update
.gdt_update:
    mov eax, 0x20
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    mov ss, eax
    cld

    ; cpuid check
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 1 << 21
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    xor ecx, eax
    jz panic

    ; long mode check
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb panic
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29
    jz panic

    ; set up paging
    ; pml4
    mov edi, tmp_page - DISPLACEMENT
    mov ebx, edi
    lea eax, [edi + 0x1000]    ; 0000 0*
    or eax, 3
    mov [ebx], eax
    lea eax, [edi + 0x2000]    ; ffff ff8*
    or eax, 3
    mov [ebx+0xff8], eax
    ; pdpt (lower)
    lea ebx, [edi + 0x1000]
    lea eax, [edi + 0x3000]
    or eax, 3
    mov [ebx], eax
    ; pdpt (higher)
    lea ebx, [edi + 0x2000]
    lea eax, [edi + 0x4000]    ; ffff ffff 8*
    or eax, 3
    mov [ebx+0xff0], eax
    ; pdt (lower)
    lea ebx, [edi + 0x3000]
    mov dword [ebx+0x08], 0x00200083
    mov dword [ebx+0x10], 0x00400083
    mov dword [ebx+0x18], 0x00600083
    ; pdt (kernel)
    lea ebx, [edi + 0x4000]
    mov dword [ebx], 0x00200083     ; kernel 4mb
    mov dword [ebx+8], 0x00400083
    mov dword [ebx+0x78*8], 0x00600083 ; stack

    mov cr3, edi
    mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

    ; enter long mode
    mov ecx, 0xc0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    jmp 0x08:lm_start

panic:
    hlt
    jmp $-2

bits 64
lm_start:
    mov rax, 0x10
    mov ds, rax
    mov es, rax
    mov fs, rax
    mov gs, rax
    mov ss, rax
    mov rsp, __stack_top
    mov rbp, rsp
    cld

    mov rax, bootinfo_mmap
    mov [bootinfo_ptr_mmap], rax

    mov rax, kmain_arch
    call rax

    cli
    hlt
    jmp $-2

gdtr:
    dw .gdt_end - .gdt - 1
    dd .gdt
.gdt:
    dd 0, 0
    dd 0x0000ffff, 0x00af9a00
    dd 0x0000ffff, 0x00af9200
    dd 0x0000ffff, 0x00cf9a00
    dd 0x0000ffff, 0x00cf9200
.gdt_end:
