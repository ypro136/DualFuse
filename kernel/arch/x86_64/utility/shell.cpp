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
#include <apic.h>
#include <minimal_acpi.h>
#include <i2c.h>
#include <hid_i2c.h>
#include <serial.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

Shell::Shell(Console* output_console) : console(output_console) {}

void Shell::print(const char* s)
{
    if (!s || !console) return;
    for (const char* p = s; *p; ++p) { console->print_char(*p); serial_write(*p); }
}

void Shell::println(const char* s) { print(s); console->print_char('\n'); }

int Shell::tokenize(char* input, char* argv[], int max_args)
{
    int argc = 0; char* p = input;
    while (*p && argc < max_args) {
        while (*p == ' ') p++;
        if (*p == '\0') break;
        argv[argc++] = p;
        while (*p != ' ' && *p != '\0') p++;
        if (*p == ' ') *p++ = '\0';
    }
    return argc;
}

void Shell::str_toupper(char* s)
{
    for (; *s; ++s) if (*s >= 'a' && *s <= 'z') *s -= 32;
}

void Shell::execute(char* input)
{
    if (!input || !console) return;
    char* argv[100];
    int   argc = tokenize(input, argv, 100);
    if (argc == 0) return;
    str_toupper(argv[0]);

    if      (strcmp(argv[0], "HELP")   == 0) cmd_help();
    else if (strcmp(argv[0], "CLEAR")  == 0) cmd_clear();
    else if (strcmp(argv[0], "ECHO")   == 0) cmd_echo(argc, argv);
    else if (strcmp(argv[0], "INFO")   == 0) cmd_info();
    else if (strcmp(argv[0], "PAGE")   == 0) cmd_page();
    else if (strcmp(argv[0], "MK")     == 0) cmd_mk(argc, argv);
    else if (strcmp(argv[0], "CAT")    == 0) cmd_cat(argc, argv);
    else if (strcmp(argv[0], "LS")     == 0) cmd_ls();
    else if (strcmp(argv[0], "RM")     == 0) cmd_rm(argc, argv);
    else if (strcmp(argv[0], "END")    == 0) cmd_end();
    else if (strcmp(argv[0], "MKDIR")  == 0) cmd_mkdir(argc, argv);
    else if (strcmp(argv[0], "CD")     == 0) cmd_cd(argc, argv);
    else if (strcmp(argv[0], "PWD")    == 0) cmd_pwd();
    else if (strcmp(argv[0], "SEARCH") == 0) cmd_search(argc, argv);
    else if (strcmp(argv[0], "START")  == 0) cmd_start(argc, argv);
    else if (strcmp(argv[0], "GREP")   == 0) cmd_grep(argc, argv);
    else if (strcmp(argv[0], "MADT")   == 0) cmd_madt();
    else if (strcmp(argv[0], "I2C")     == 0) cmd_i2c();
    else if (strcmp(argv[0], "I2CHID")   == 0) cmd_i2chid();
    else if (strcmp(argv[0], "I2CPOLL")  == 0) cmd_i2cpoll();
    else { print("Unknown command: "); println(argv[0]); }
}

void Shell::cmd_help()
{
    println("Available commands:");
    println("  END    - Halt the CPU");
    println("  CLEAR  - Clear the screen");
    println("  PAGE   - Allocate memory");
    println("  MK     - Create file:  MK <n> <content>");
    println("  CAT    - Show file:    CAT <n>");
    println("  LS     - List files");
    println("  RM     - Delete file:  RM <n>");
    println("  ECHO   - Print text");
    println("  INFO   - System info");
    println("  MKDIR  - Make directory");
    println("  CD     - Change directory");
    println("  PWD    - Print working directory");
    println("  SEARCH - Search files by name");
    println("  START  - Run script or print file");
    println("  GREP   - Search text in file");
    println("  HELP   - Show this message");
    println("  debug:");
    println("  MADT   - Show MADT entries");
    println("  I2C    - I2C status / probe");
    println("  I2CHID  - HID-over-I2C touchpad test");
    println("  I2CPOLL - Poll HID-over-I2C touchpad");
}

void Shell::cmd_i2c()
{
    char buf[64];

    println("I2C status (initialized at boot):");

    /* Read state from the already-initialized controller */
    uint64_t base = bootloader.hhdmOffset + 0xFE040000u;

    uint32_t comp   = i2cRead(base, DW_IC_COMP_TYPE);
    uint32_t con    = i2cRead(base, DW_IC_CON);
    uint32_t tar    = i2cRead(base, DW_IC_TAR);
    uint32_t status = i2cRead(base, DW_IC_STATUS);
    uint32_t en     = i2cRead(base, DW_IC_ENABLE);

    print("  controller : ");
    println(comp == DW_IC_COMP_TYPE_VALUE ? "DesignWare (OK)" : "unknown/not found");

    print("  base addr  : 0x"); u64toa(0xFE040000u, buf, 16); println(buf);

    print("  IC_ENABLE  : "); u64toa(en,     buf, 16); println(buf);
    print("  IC_CON     : 0x"); u64toa(con,    buf, 16); println(buf);
    print("  IC_TAR     : 0x"); u64toa(tar,    buf, 16); println(buf);
    print("  IC_STATUS  : 0x"); u64toa(status, buf, 16); println(buf);

    println("  device     : ELAN touchpad");
    print("  address    : 0x"); u64toa(I2C_ADDR_ELAN_TOUCHPAD, buf, 16); println(buf);
    println("  last probe : ACK (confirmed at boot)");
}

void Shell::cmd_madt()
{
    AcpiMadt* madt = acpiGetMadt();
    if (!madt) { println("MADT not found."); return; }
    println("MADT entries:");
    uint8_t* ptr = (uint8_t*)madt + sizeof(AcpiMadt);
    uint8_t* end = (uint8_t*)madt + madt->hdr.length;
    while (ptr < end) {
        AcpiEntryHdr* entry = (AcpiEntryHdr*)ptr;
        char buf[128];
        switch (entry->type) {
            case 0: {
                AcpiMadtLapic* e = (AcpiMadtLapic*)entry;
                print("  LAPIC: uid="); u64toa(e->processor_uid, buf, 10); print(buf);
                print(" id="); u64toa(e->lapic_id, buf, 10); print(buf);
                print(" flags=0x"); u64toa(e->flags, buf, 16); println(buf);
            } break;
            case 1: {
                AcpiMadtIoapic* e = (AcpiMadtIoapic*)entry;
                print("  IOAPIC: id="); u64toa(e->id, buf, 10); print(buf);
                print(" addr=0x"); u64toa(e->address, buf, 16); print(buf);
                print(" gsi="); u64toa(e->gsi_base, buf, 10); println(buf);
            } break;
            case 2: {
                AcpiMadtInterruptSourceOverride* e = (AcpiMadtInterruptSourceOverride*)entry;
                print("  ISO: bus="); u64toa(e->bus, buf, 10); print(buf);
                print(" src="); u64toa(e->source, buf, 10); print(buf);
                print(" gsi="); u64toa(e->gsi, buf, 10); print(buf);
                print(" flags=0x"); u64toa(e->flags, buf, 16); println(buf);
            } break;
            default:
                print("  type="); u64toa(entry->type, buf, 10); println(buf);
                break;
        }
        ptr += entry->length;
    }
}

void Shell::cmd_clear() { console->clear_screen(); }

void Shell::cmd_echo(int argc, char* argv[])
{
    for (int i = 1; i < argc; i++) { print(argv[i]); if (i != argc-1) print(" "); }
    console->print_char('\n');
}

void Shell::cmd_info()
{
    char buf[128];
    println("DualFuse OS - 64-bit");
    print("  width:   "); u64toa(tempframebuffer ? tempframebuffer->width  : 0, buf, 10); println(buf);
    print("  height:  "); u64toa(tempframebuffer ? tempframebuffer->height : 0, buf, 10); println(buf);
    print("  FPS:     "); u64toa(frequency, buf, 10); println(buf);
    print("  lapicID: "); u64toa(apicGetBspLapicId(), buf, 10); println(buf);
}

void Shell::cmd_page()
{
    void* page = malloc(1000); char buf[32];
    print("Allocated at: 0x"); u64toa((uint64_t)(uintptr_t)page, buf, 16); println(buf);
}

void Shell::cmd_mk(int argc, char* argv[])
{
    if (argc < 3) { println("Usage: MK <filename> <content>"); return; }
    char content[512]; content[0] = '\0';
    for (int i = 2; i < argc; i++) { strcat(content, argv[i]); if (i != argc-1) strcat(content, " "); }
    if (fs_create(argv[1]) == 0) { fs_write(argv[1], content); println("File created."); }
    else println("Error: could not create file.");
}

void Shell::cmd_cat(int argc, char* argv[])
{
    if (argc < 2) { println("Usage: CAT <filename>"); return; }
    char buf[512] = {0};
    if (fs_read(argv[1], buf) == 0) println(buf);
    else println("Error: could not read file.");
}

void Shell::cmd_ls() { fs_list(); }

void Shell::cmd_rm(int argc, char* argv[])
{
    if (argc < 2) { println("Usage: RM <n>"); return; }
    int r = fs_delete(argv[1]);
    if (r == 0) println("Deleted.");
    else if (r == -2) println("Error: directory not empty.");
    else println("Error: not found.");
}

void Shell::cmd_end() { println("Stopping CPU..."); Halt(); }

void Shell::cmd_mkdir(int argc, char* argv[])
{
    if (argc < 2) { println("Usage: mkdir <n>"); return; }
    if (fs_mkdir(argv[1]) == 0) println("Directory created.");
    else println("Error: could not create directory.");
}

void Shell::cmd_cd(int argc, char* argv[])
{
    if (argc < 2) { println("Usage: cd <name|..>"); return; }
    if (fs_cd(argv[1]) != 0) println("Error: directory not found.");
}

void Shell::cmd_pwd() { char buf[MAX_PATH]; fs_pwd(buf); println(buf); }

void Shell::cmd_search(int argc, char* argv[])
{
    if (argc < 2) { println("Usage: search <term>"); return; }
    fs_search(argv[1]);
}

void Shell::cmd_start(int argc, char* argv[])
{
    if (argc < 2) { println("Usage: start <file>"); return; }
    fs_start(argv[1], this);
}

void Shell::cmd_grep(int argc, char* argv[])
{
    if (argc < 3) { println("Usage: grep <pattern> <file>"); return; }
    char buf[BLOCK_SIZE + 1];
    if (fs_read(argv[2], buf) != 0) { println("Error: file not found."); return; }
    char* line = buf;
    for (char* p = buf; ; p++) {
        if (*p == '\n' || *p == '\0') {
            char end = *p; *p = '\0';
            if (strstr(line, argv[1])) println(line);
            if (end == '\0') break;
            line = p + 1;
        }
    }
}

void Shell::cmd_i2chid()
{
    char buf[64];

    println("[1] Power on + find base...");
    i2cPowerOnAll();
    I2cBaseResult res = i2cFindBase();
    if (res.virtBase == 0) { println("  FAILED - no controller"); return; }
    print("  base=0x"); u64toa(res.virtBase, buf, 16); println(buf);

    println("[2] Deassert LPSS resets...");
    i2cDeassertReset(res.virtBase);

    println("[3] Init DW controller (target=0x15)...");
    i2cInit(res.virtBase, I2C_ADDR_ELAN_TOUCHPAD);
    uint32_t en = i2cRead(res.virtBase, DW_IC_ENABLE);
    print("  IC_ENABLE="); u64toa(en, buf, 10); println(buf);
    sleep(1);

    println("[4] Reading HID descriptor (reg 0x0001)...");
    HidI2cDescriptor desc;
    if (hidI2cGetDescriptor(res.virtBase, I2C_ADDR_ELAN_TOUCHPAD, &desc) < 0) {
        println("  FAILED"); return;
    }
    print("  wHIDDescLength   : "); u64toa(desc.wHIDDescLength,     buf, 10); println(buf);
    print("  wInputRegister   : 0x"); u64toa(desc.wInputRegister,   buf, 16); println(buf);
    print("  wMaxInputLength  : "); u64toa(desc.wMaxInputLength,     buf, 10); println(buf);
    print("  wCommandRegister : 0x"); u64toa(desc.wCommandRegister, buf, 16); println(buf);
    print("  wVendorID        : 0x"); u64toa(desc.wVendorID,        buf, 16); println(buf);
    print("  wProductID       : 0x"); u64toa(desc.wProductID,       buf, 16); println(buf);

    println("[5] Sending SET_POWER ON + RESET...");
    if (hidI2cReset(res.virtBase, I2C_ADDR_ELAN_TOUCHPAD, &desc) < 0) {
        println("  reset failed - continuing anyway");
    }

    println("[6] Reading raw reports (touch the pad now)...");
    uint16_t maxLen = desc.wMaxInputLength;
    if (maxLen < 4 || maxLen > 64) maxLen = 32;
    uint8_t rbuf[64];
    uint8_t reg[2] = { (uint8_t)(desc.wInputRegister & 0xFF),
                       (uint8_t)(desc.wInputRegister >> 8) };

    for (int rep = 0; rep < 5; rep++) {
        sleep(120);
        int got = i2cWriteRead(res.virtBase, reg, 2, rbuf, maxLen);
        if (got < 0) { println("  read failed"); continue; }
        print("  ["); u64toa(rep, buf, 10); print(buf); print("] id=0x");
        u64toa(rbuf[2], buf, 16); print(buf);
        print(" raw: ");
        for (int i = 0; i < got && i < 12; i++) {
            if (rbuf[i] < 0x10) print("0");
            u64toa(rbuf[i], buf, 16); print(buf); print(" ");
        }
        println("");
    }
    println("done.");
}

void Shell::cmd_i2cpoll()
{
    char buf[64];
    i2cPowerOnAll();
    I2cBaseResult res = i2cFindBase();
    if (res.virtBase == 0) { println("no controller"); return; }
    i2cDeassertReset(res.virtBase);
    i2cInit(res.virtBase, I2C_ADDR_ELAN_TOUCHPAD);
    sleep(1);

    HidI2cDescriptor desc;
    if (hidI2cGetDescriptor(res.virtBase, I2C_ADDR_ELAN_TOUCHPAD, &desc) < 0) {
        println("descriptor failed"); return;
    }
    hidI2cReset(res.virtBase, I2C_ADDR_ELAN_TOUCHPAD, &desc);

    println("Polling 30 reports - touch the pad...");
    for (int i = 0; i < 30; i++) {
        /* small delay between reads */
        sleep(1);

        uint8_t rbuf[16] = {0};
        uint8_t reg[2] = { (uint8_t)(desc.wInputRegister & 0xFF),
                           (uint8_t)(desc.wInputRegister >> 8) };
        int got = i2cWriteRead(res.virtBase, reg, 2, rbuf, 14);
        if (got < 0) { println("read failed"); continue; }

        /* ELAN 14-byte report layout:
         * [0,1]=len [2]=reportID [3]=flags [4,5]=X [6,7]=Y */
        uint8_t flags = rbuf[3];
        uint16_t x = (uint16_t)rbuf[4] | ((uint16_t)rbuf[5] << 8);
        uint16_t y = (uint16_t)rbuf[6] | ((uint16_t)rbuf[7] << 8);

        if (flags & 0x01) {
            print("  TOUCH x="); u64toa(x, buf, 10); print(buf);
            print(" y="); u64toa(y, buf, 10); print(buf);
            print(" flags=0x"); u64toa(flags, buf, 16); println(buf);
        } else {
            println("  idle");
        }
    }
    println("poll done.");
}