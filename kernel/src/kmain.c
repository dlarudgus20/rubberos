void kmain(volatile int* video, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int color = x ^ y;
            if (y < 10 || y >= height - 10 || x < 10 || x >= width - 10) {
                color = 0x0000ff00;
            }
            video[x + y * width] = color;
        }
    }
    while (1) __asm__ __volatile__ ("hlt");
}
