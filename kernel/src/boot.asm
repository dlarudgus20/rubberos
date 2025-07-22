%define FLAGS 3
%define MAGIC 0x1badb002

section .multiboot

align 4
dd MAGIC
dd FLAGS
dd -(MAGIC + FLAGS)

section .bss

align 0x1000
page_pml4: resb 0x1000
page_pdpt: resb 0x1000
page_pdt: resb 0x1000
page_pt: resb 0x10000

align 16
stack_bottom: resb 16384
stack_top:

section .text

extern kmain

bits 32
global _start
_start:
    ; gdtr update
    lgdt [gdtr]
    jmp 0x18:.gdt_update
.gdt_update:
    mov eax, 0x20
    mov ds, eax
    mov es, eax
    mov fs, eax
    mov gs, eax
    mov ss, eax
    mov esp, stack_top
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
    mov ebx, page_pml4
    mov eax, page_pdpt
    or eax, 3
    mov [ebx], eax
    mov ebx, page_pdpt
    mov eax, page_pdt
    or eax, 3
    mov [ebx], eax
    mov ebx, page_pdt
    mov eax, page_pt
    or eax, 3
    mov ecx, 16
.pdt_loop:
    mov [ebx], eax
    add eax, 0x1000
    add ebx, 8
    dec ecx
    jnz .pdt_loop
    mov ebx, page_pt
    mov eax, 3
    mov ecx, 0x2000
.pt_loop:
    mov [ebx], eax
    add eax, 0x1000
    add ebx, 8
    dec ecx
    jnz .pt_loop

    mov eax, page_pml4
    mov cr3, eax
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
    mov esi, panic_msg
    mov edi, 0xb80a0
    mov ah, 0x07
    cld
.loop:
    lodsb
    test al, al
    jz .end
    stosw
    jmp .loop
.end:
    hlt
    jmp $-2

panic_msg: db "cannot enable long mode", 0

bits 64
lm_start:
    mov rax, 0x10
    mov ds, rax
    mov es, rax
    mov fs, rax
    mov gs, rax
    mov ss, rax
    mov rsp, stack_top
    mov rbp, rsp
    cld

    call kmain
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
