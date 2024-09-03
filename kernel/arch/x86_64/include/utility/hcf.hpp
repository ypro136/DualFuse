

static void Halt(void) {
    for (;;) {
        asm ("hlt");
    }
}