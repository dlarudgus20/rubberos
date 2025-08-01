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

align 16
bootinfo: resb 0x1000

section .startup alloc exec progbits

bits 32
global _start
_start:
    cli
    mov esp, __stack_top_lba

    cmp eax, 0x36d76289 ; check multiboot2 bootloader
    jne panic

    cld
    mov esi, ebx
    mov edi, bootinfo - DISPLACEMENT
    mov ecx, [ebx]
    mov eax, 0x1000
    cmp ecx, eax
    cmova ecx, eax
    add ecx, 3
    shr ecx, 2
    rep movsd

    mov ecx, ebx
    add ecx, [ebx]
    add ebx, 8
.loop:
    cmp dword [ebx], 8
    jne .cont
    push dword [ebx+24] ; fb_height
    push dword [ebx+20] ; fb_width
    push dword [ebx+8]  ; fb_addr
    jmp init_long
.cont:
    add ebx, [ebx+4]
    add ebx, 7
    shr ebx, 3
    shl ebx, 3
    cmp ebx, ecx
    jb .loop

    jmp panic

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
    lea eax, [edi + 0x5000]    ; ffff ff80 __*
    or eax, 3
    mov [ebx], eax
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
    mov dword [ebx+0x80*8], 0x00800083 ; stack
    ; pdt (fb)
    lea ebx, [edi + 0x5000]
    mov eax, [esp]
    and eax, 0xffe00000
    or eax, 0x83
    mov ecx, 512
.pdt_loop:
    mov [ebx], eax
    add ebx, 8
    add eax, 0x200000
    jc .end_pdt
    dec ecx
    jnz .pdt_loop
.end_pdt:

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

    xor rdi, rdi
    mov ebx, [rsp-12]
    and rdi, 0x1fffff
    mov rax, 0xffffff8000000000
    add rdi, rax

    mov esi, [rsp-8]
    mov edx, [rsp-4]
    mov rcx, bootinfo
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
