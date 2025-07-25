%define MAGIC1 0x1badb002
%define FLAGS1 7

%define MAGIC2 0xe85250d6

section .multiboot
multiboot2:
align 8
dd MAGIC2
dd 0
dd .end - multiboot2
dd -(MAGIC2 + .end - multiboot2)
align 8, db 0
dw 5, 0
dd 20, 1024, 768, 32
align 8, db 0
dd 0, 0
.end:

align 4
dd MAGIC1
dd FLAGS1
dd -(MAGIC1 + FLAGS1)
dd 0, 0, 0, 0, 0
dd 0, 1024, 768, 32
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
    mov esp, stack_top
    cmp eax, 0x36d76289
    je mb2
    cmp eax, 0x2badb002
    je mb1
    jmp panic

mb2:
    mov ecx, ebx
    add ecx, [ebx]
    add ebx, 8
.loop:
    cmp dword [ebx], 8
    jne .cont
    mov edi, [ebx+8]
    sub esp, 8
    mov eax, [ebx+20]
    mov [esp], eax
    mov eax, [ebx+24]
    mov [esp+4], eax
    jmp draw
.cont:
    add ebx, [ebx+4]
    add ebx, 7
    shr ebx, 3
    shl ebx, 3
    cmp ebx, ecx
    jb .loop
    jmp panic

mb1:
    mov edi, [ebx+88]
    sub esp, 8
    mov eax, [ebx+100]
    mov [esp], eax
    mov eax, [ebx+104]
    mov [esp+4], eax

draw:
    xor edx, edx
.yloop:
    xor ecx, ecx
.xloop:
    mov eax, ecx
    xor eax, edx
    mov [edi], eax
    add edi, 4
    inc ecx
    cmp ecx, [esp]
    jb .xloop
    inc edx
    cmp edx, [esp+4]
    jb .yloop
    hlt
    jmp $-2

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

section .data

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

panic_msg: db "cannot enable long mode", 0
