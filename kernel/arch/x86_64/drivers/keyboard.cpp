#include <keyboard.h>

#include <task.h>
#include <types.h>
#include <utility.h>
#include <isr.h>
#include <vfs.h>
#include <console.h> 
#include <liballoc.h>
#include <apic.h>

#include <stdint.h>
#include <stdio.h>

// ─── State ─────────────────────────────────────────────────────────────────
bool     shift_down = false;
bool     capsLock   = false;
char*    kbBuff     = nullptr;
uint32_t kbCurr     = 0;
uint32_t kbMax      = 0;
uint32_t kbTaskId   = 0;

// ─── Special key codes ─────────────────────────────────────────────────────
const uint32_t UNKNOWN = 0xFFFFFFFF;
const uint32_t ESC     = 0xFFFFFFFF - 1;
const uint32_t CTRL    = 0xFFFFFFFF - 2;
const uint32_t LSHFT   = 0xFFFFFFFF - 3;
const uint32_t RSHFT   = 0xFFFFFFFF - 4;
const uint32_t ALT     = 0xFFFFFFFF - 5;
const uint32_t F1      = 0xFFFFFFFF - 6;
const uint32_t F2      = 0xFFFFFFFF - 7;
const uint32_t F3      = 0xFFFFFFFF - 8;
const uint32_t F4      = 0xFFFFFFFF - 9;
const uint32_t F5      = 0xFFFFFFFF - 10;
const uint32_t F6      = 0xFFFFFFFF - 11;
const uint32_t F7      = 0xFFFFFFFF - 12;
const uint32_t F8      = 0xFFFFFFFF - 13;
const uint32_t F9      = 0xFFFFFFFF - 14;
const uint32_t F10     = 0xFFFFFFFF - 15;
const uint32_t F11     = 0xFFFFFFFF - 16;
const uint32_t F12     = 0xFFFFFFFF - 17;
const uint32_t SCRLCK  = 0xFFFFFFFF - 18;
const uint32_t HOME    = 0xFFFFFFFF - 19;
const uint32_t UP      = 0xFFFFFFFF - 20;
const uint32_t LEFT    = 0xFFFFFFFF - 21;
const uint32_t RIGHT   = 0xFFFFFFFF - 22;
const uint32_t DOWN    = 0xFFFFFFFF - 23;
const uint32_t PGUP    = 0xFFFFFFFF - 24;
const uint32_t PGDOWN  = 0xFFFFFFFF - 25;
const uint32_t END     = 0xFFFFFFFF - 26;
const uint32_t INS     = 0xFFFFFFFF - 27;
const uint32_t DEL     = 0xFFFFFFFF - 28;
const uint32_t CAPS    = 0xFFFFFFFF - 29;
const uint32_t NONE    = 0xFFFFFFFF - 30;
const uint32_t ALTGR   = 0xFFFFFFFF - 31;
const uint32_t NUMLCK  = 0xFFFFFFFF - 32;

#define ENTER     0x1C
#define BACKSPACE 0x0E

const uint32_t lowercase[128] = {
    UNKNOWN,ESC,'1','2','3','4','5','6','7','8',
    '9','0','-','=','\b','\t','q','w','e','r',
    't','y','u','i','o','p','[',']','\n',CTRL,
    'a','s','d','f','g','h','j','k','l',';',
    '\'','`',LSHFT,'\\','z','x','c','v','b','n','m',',',
    '.','/',RSHFT,'*',ALT,' ',CAPS,F1,F2,F3,F4,F5,F6,F7,F8,F9,F10,
    NUMLCK,SCRLCK,HOME,UP,PGUP,'-',LEFT,UNKNOWN,RIGHT,
    '+',END,DOWN,PGDOWN,INS,DEL,UNKNOWN,UNKNOWN,UNKNOWN,F11,F12,
    UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
    UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
    UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
    UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
    UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN
};

const uint32_t uppercase[128] = {
    UNKNOWN,ESC,'!','@','#','$','%','^','&','*','(',')','_','+',
    '\b','\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',CTRL,
    'A','S','D','F','G','H','J','K','L',':','"','~',LSHFT,'|',
    'Z','X','C','V','B','N','M','<','>','?',RSHFT,'*',ALT,' ',CAPS,
    F1,F2,F3,F4,F5,F6,F7,F8,F9,F10,NUMLCK,SCRLCK,HOME,UP,PGUP,'-',
    LEFT,UNKNOWN,RIGHT,'+',END,DOWN,PGDOWN,INS,DEL,UNKNOWN,UNKNOWN,
    UNKNOWN,F11,F12,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
    UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
    UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
    UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
    UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN
};

const char sc_ascii[] = {
    '?','?','1','2','3','4','5','6','7','8','9','0','-','=','?','?',
    'Q','W','E','R','T','Y','U','I','O','P','[',']','?','?',
    'A','S','D','F','G','H','J','K','L',';','\'','`','?','\\',
    'Z','X','C','V','B','N','M',',','.','/',  '?','?','?',' '
};

static char key_buffer[1024] = {0};


static void kb_backspace(char s[])
{
    int len = strlen(s);
    if (len > 0) s[len - 1] = '\0';
}

static void kb_append(char s[], char c, int max_len)
{
    int len = strlen(s);
    if (len < max_len - 1)
    {
        s[len]     = c;
        s[len + 1] = '\0';
    }
}

// ─── Initialization ────────────────────────────────────────────────────────

void keyboard_initialize()
{
#if defined(DEBUG_KEYBOARD)
    printf("[keyboard] init\n");
#endif

    shift_down = false;
    capsLock   = false;
    int keyboard_irq = 1;

    if (apic_initialized)
        keyboard_irq = ioApicRedirect(keyboard_irq, false);

    if (isr_initialized)
    {
        irq_install_handler(keyboard_irq, &keyboard_handler);
    }
    else
    {
        printf("[keyboard] warning: ISR not initialized, keyboard skipped\n");
        return;
    }

    if (apic_initialized)
    {
        keyboard_Write(0x64, 0xae);
        in_port_byte(0x60);
    }

    printf("keyboard initialized.\n");
}

void keyboard_Write(uint16_t port, uint8_t value)
{
    while (in_port_byte(0x64) & 2) {}
    out_port_byte(port, value);
}


void keyboard_handler(struct interrupt_registers* /*registers*/)
{
    uint8_t byte      = in_port_byte(0x60);
    uint8_t scan_code = byte & 0x7F;    // which key
    uint8_t released  = byte & 0x80;    // 0x80 = key-up

#if defined(DEBUG_KEYBOARD)
    printf("[keyboard] scan:%d released:%d\n", scan_code, released);
#endif

    switch (scan_code)
    {
        // Ignored keys (function, ESC, CTRL, NUMLCK, SCRLCK, ALT …)
        case 1: case 29: case 56:
        case 59: case 60: case 61: case 62: case 63: case 64:
        case 65: case 66: case 67: case 68: case 87: case 88:
            return;

        case 42:    // Left Shift
            shift_down = (released == 0);
            return;

        case 58:    // Caps Lock (toggle on key-down only)
            if (released == 0)
                capsLock = !capsLock;
            return;

        default:
            break;
    }

    if (released != 0) return;

    if (scan_code == BACKSPACE)
    {
        kb_backspace(key_buffer);
        // Echo backspace to the active console
        if (active_console)
            active_console->print_char('\b');
        return;
    }

    if (scan_code == ENTER)
    {
        if (active_console)
            active_console->print_char('\n');

        // Route the completed line to the shell bound to the active console
        if (active_console && active_console->get_shell())
            active_console->get_shell()->execute(key_buffer);

        key_buffer[0] = '\0';
        return;
    }

    bool upper = shift_down ^ capsLock;   // XOR: shift inverts caps
    uint32_t mapped = upper ? uppercase[scan_code] : lowercase[scan_code];

    if (mapped == UNKNOWN || mapped >= 0x80) return;  // non-printable

    char final_char = (char)mapped;

    kb_append(key_buffer, final_char, sizeof(key_buffer));
    if (active_console)
        active_console->print_char(final_char);
}


bool keyboard_is_occupied() { return !!kbBuff; }

bool keyboard_task_read(uint32_t taskId, char* buff, uint32_t limit, bool changeTaskState)
{
    while (keyboard_is_occupied()) {}

    Task* task = task_get(taskId);
    if (!task) return false;

    kbBuff    = buff;
    kbCurr    = 0;
    kbMax     = limit;
    kbTaskId  = taskId;

    if (changeTaskState)
        task->state = TASK_STATE_WAITING_INPUT;

    return true;
}