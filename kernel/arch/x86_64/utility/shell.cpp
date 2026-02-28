#include <shell.h>
#include <console.h>

#include <vfs.h>
#include <task.h>
#include <liballoc.h>
#include <framebufferutil.h>
#include <timer.h>
#include <bootloader.h>
#include <utility.h>
#include <ramdisk.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>


Shell::Shell(Console* output_console)
    : console(output_console)
{
}


void Shell::print(const char* s)
{
    if (!s || !console) return;
    for (const char* p = s; *p; ++p)
        console->print_char(*p);
}

void Shell::println(const char* s)
{
    print(s);
    console->print_char('\n');
}


int Shell::tokenize(char* input, char* argv[], int max_args)
{
    int   argc = 0;
    char* p    = input;

    while (*p && argc < max_args)
    {
        while (*p == ' ') p++;          // skip leading spaces
        if (*p == '\0') break;

        argv[argc++] = p;               // start of token

        while (*p != ' ' && *p != '\0') p++;
        if (*p == ' ') *p++ = '\0';    // null-terminate token
    }

    return argc;
}


void Shell::execute(char* input)
{
    if (!input || !console) return;

    char* argv[100];
    int   argc = tokenize(input, argv, 100);
    if (argc == 0) return;

    if      (strcmp(argv[0], "HELP")  == 0) cmd_help();
    else if (strcmp(argv[0], "CLEAR") == 0) cmd_clear();
    else if (strcmp(argv[0], "ECHO")  == 0) cmd_echo(argc, argv);
    else if (strcmp(argv[0], "INFO")  == 0) cmd_info();
    else if (strcmp(argv[0], "PAGE")  == 0) cmd_page();
    else if (strcmp(argv[0], "MK")    == 0) cmd_mk(argc, argv);
    else if (strcmp(argv[0], "CAT")   == 0) cmd_cat(argc, argv);
    else if (strcmp(argv[0], "LS")    == 0) cmd_ls();
    else if (strcmp(argv[0], "RM")    == 0) cmd_rm(argc, argv);
    else if (strcmp(argv[0], "END")   == 0) cmd_end();
    else
    {
        print("Unknown command: ");
        println(argv[0]);
    }
}

// ─── Command implementations ───────────────────────────────────────────────

void Shell::cmd_help()
{
    println("Available commands:");
    println("  END    - Halt the CPU");
    println("  CLEAR  - Clear the screen");
    println("  PAGE   - Allocate memory");
    println("  MK     - Create file:  MK <name> <content>");
    println("  CAT    - Show file:    CAT <name>");
    println("  LS     - List files");
    println("  RM     - Delete file:  RM <name>");
    println("  ECHO   - Print text");
    println("  INFO   - System info");
}

void Shell::cmd_clear()
{
    console->clear_screen();
}

void Shell::cmd_echo(int argc, char* argv[])
{
    for (int i = 1; i < argc; i++)
    {
        print(argv[i]);
        if (i != argc - 1) print(" ");
    }
    console->print_char('\n');
}

void Shell::cmd_info()
{
    println("Nachtlauf v0.1 -- 64-bit long mode");

    // Print each field on its own line via print_char to avoid
    // pulling in a full printf with format strings here.
    char buf[128];

    println("Framebuffer info:");

    // pitch
    print("  address:  ");
    u64toa(tempframebuffer ? tempframebuffer->address : 0, buf, 10);
    println(buf);
    // pitch
    print("  pitch:    ");
    u64toa(tempframebuffer ? tempframebuffer->pitch : 0, buf, 10);
    println(buf);

    // width
    print("  width:    ");
    u64toa(tempframebuffer ? tempframebuffer->width : 0, buf, 10);
    println(buf);

    // height
    print("  height:   ");
    u64toa(tempframebuffer ? tempframebuffer->height : 0, buf, 10);
    println(buf);

    // framerate
    print("  FPS:      ");
    u64toa(frequency, buf, 10);
    println(buf);
}

void Shell::cmd_page()
{
    void* page = malloc(1000);
    char  buf[32];
    print("Allocated page at: 0x");
    u64toa((uint64_t)(uintptr_t)page, buf, 16);
    println(buf);
}

void Shell::cmd_mk(int argc, char* argv[])
{
    if (argc < 3)
    {
        println("Usage: MK <filename> <content>");
        return;
    }

    char content[512];
    content[0] = '\0';
    for (int i = 2; i < argc; i++)
    {
        strcat(content, argv[i]);
        if (i != argc - 1) strcat(content, " ");
    }

    if (fs_create(argv[1]) == 0)
    {
        fs_write(argv[1], content);
        println("File created.");
    }
    else
    {
        println("Error: could not create file.");
    }
}

void Shell::cmd_cat(int argc, char* argv[])
{
    if (argc < 2)
    {
        println("Usage: CAT <filename>");
        return;
    }

    char buf[512] = {0};
    if (fs_read(argv[1], buf) == 0)
    {
        println(buf);
    }
    else
    {
        println("Error: could not read file.");
    }
}

void Shell::cmd_ls()
{
    fs_list();      // TODO: fix fs_list printing directly
}

void Shell::cmd_rm(int argc, char* argv[])
{
    if (argc < 2)
    {
        println("Usage: RM <filename>");
        return;
    }

    if (fs_delete(argv[1]) == 0)
        println("File deleted.");
    else
        println("Error: could not delete file.");
}

void Shell::cmd_end()
{
    println("Stopping CPU...");
    Halt();
}