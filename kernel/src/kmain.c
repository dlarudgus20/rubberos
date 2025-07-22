void kmain(void) {
    volatile short *video = (short *)0xb80a0;
    const char* str = "hello world";
    for (int i = 0; str[i] != 0; i++) {
        video[i] = 0x0f00 | str[i];
    }
    while (1) __asm__ __volatile__ ("hlt");
}
