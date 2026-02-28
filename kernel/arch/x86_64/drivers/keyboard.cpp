#include <keyboard.h>

#include <task.h>
#include <types.h>
#include <ramdisk.h>


#include <utility.h>
#include <isr.h>
#include <vfs.h>
#include <console.h>
#include <liballoc.h>
#include <linux.h>
#include <apic.h>


// for info command
#include <framebufferutil.h>
#include <timer.h>
#include <bootloader.h>


#include <stdint.h>
#include <stdio.h>

bool shift_down;
bool capsLock;
char *kbBuff = 0;
uint32_t kbCurr = 0;
uint32_t kbMax = 0;
uint32_t kbTaskId = 0;

const uint32_t UNKNOWN = 0xFFFFFFFF;
const uint32_t ESC = 0xFFFFFFFF - 1;
const uint32_t CTRL = 0xFFFFFFFF - 2;
const uint32_t LSHFT = 0xFFFFFFFF - 3;
const uint32_t RSHFT = 0xFFFFFFFF - 4;
const uint32_t ALT = 0xFFFFFFFF - 5;
const uint32_t F1 = 0xFFFFFFFF - 6;
const uint32_t F2 = 0xFFFFFFFF - 7;
const uint32_t F3 = 0xFFFFFFFF - 8;
const uint32_t F4 = 0xFFFFFFFF - 9;
const uint32_t F5 = 0xFFFFFFFF - 10;
const uint32_t F6 = 0xFFFFFFFF - 11;
const uint32_t F7 = 0xFFFFFFFF - 12;
const uint32_t F8 = 0xFFFFFFFF - 13;
const uint32_t F9 = 0xFFFFFFFF - 14;
const uint32_t F10 = 0xFFFFFFFF - 15;
const uint32_t F11 = 0xFFFFFFFF - 16;
const uint32_t F12 = 0xFFFFFFFF - 17;
const uint32_t SCRLCK = 0xFFFFFFFF - 18;
const uint32_t HOME = 0xFFFFFFFF - 19;
const uint32_t UP = 0xFFFFFFFF - 20;
const uint32_t LEFT = 0xFFFFFFFF - 21;
const uint32_t RIGHT = 0xFFFFFFFF - 22;
const uint32_t DOWN = 0xFFFFFFFF - 23;
const uint32_t PGUP = 0xFFFFFFFF - 24;
const uint32_t PGDOWN = 0xFFFFFFFF - 25;
const uint32_t END = 0xFFFFFFFF - 26;
const uint32_t INS = 0xFFFFFFFF - 27;
const uint32_t DEL = 0xFFFFFFFF - 28;
const uint32_t CAPS = 0xFFFFFFFF - 29;
const uint32_t NONE = 0xFFFFFFFF - 30;
const uint32_t ALTGR = 0xFFFFFFFF - 31;
const uint32_t NUMLCK = 0xFFFFFFFF - 32;
#define ENTER 0x1C
#define BACKSPACE 0X0E


static char key_buffer[1024] = {0};


const uint32_t lowercase[128] = {
UNKNOWN,ESC,'1','2','3','4','5','6','7','8',
'9','0','-','=','\b','\t','q','w','e','r',
't','y','u','i','o','p','[',']','\n',CTRL,
'a','s','d','f','g','h','j','k','l',';',
'\'','`',LSHFT,'\\','z','x','c','v','b','n','m',',',
'.','/',RSHFT,'*',ALT,' ',CAPS,F1,F2,F3,F4,F5,F6,F7,F8,F9,F10,NUMLCK,SCRLCK,HOME,UP,PGUP,'-',LEFT,UNKNOWN,RIGHT,
'+',END,DOWN,PGDOWN,INS,DEL,UNKNOWN,UNKNOWN,UNKNOWN,F11,F12,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN
};

const uint32_t uppercase[128] = {
    UNKNOWN,ESC,'!','@','#','$','%','^','&','*','(',')','_','+','\b','\t','Q','W','E','R',
'T','Y','U','I','O','P','{','}','\n',CTRL,'A','S','D','F','G','H','J','K','L',':','"','~',LSHFT,'|','Z','X','C',
'V','B','N','M','<','>','?',RSHFT,'*',ALT,' ',CAPS,F1,F2,F3,F4,F5,F6,F7,F8,F9,F10,NUMLCK,SCRLCK,HOME,UP,PGUP,'-',
LEFT,UNKNOWN,RIGHT,'+',END,DOWN,PGDOWN,INS,DEL,UNKNOWN,UNKNOWN,UNKNOWN,F11,F12,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,
UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN,UNKNOWN
};

const char sc_ascii[] = { '?', '?', '1', '2', '3', '4', '5', '6',     
    '7', '8', '9', '0', '-', '=', '?', '?', 'Q', 'W', 'E', 'R', 'T', 'Y', 
        'U', 'I', 'O', 'P', '[', ']', '?', '?', 'A', 'S', 'D', 'F', 'G', 
        'H', 'J', 'K', 'L', ';', '\'', '`', '?', '\\', 'Z', 'X', 'C', 'V', 
        'B', 'N', 'M', ',', '.', '/', '?', '?', '?', ' '};


void keyboard_initialize()
{
    #if defined(DEBUG_KEYBOARD)
    printf("[keyboard] init\n");
    #endif
    shift_down = false;
    capsLock = false;
    int keyboard_irq = 1;

    if (apic_initialized)
    {
        keyboard_irq = ioApicRedirect(keyboard_irq, false);
    }

    #if defined(DEBUG_KEYBOARD)
    printf("[keyboard] keyboard_initialize: irq_install_handler with keyboard_irq:%d\n", keyboard_irq);
    #endif
    if (isr_initialized)
    {
        irq_install_handler(keyboard_irq, &keyboard_handler);
    }
    else 
    {
        printf("[keyboard] warning: isr not initialized. keyboard initialization omitted");
        return;
    }
    #if defined(DEBUG_KEYBOARD)
    printf("[keyboard] keyboard_initialize: irq_install_handler done\n");
    #endif

    if (apic_initialized)
    {
        keyboard_Write(0x64, 0xae);
        in_port_byte(0x60);
    }
    printf("keyboard initialized.\n");
}

void keyboard_Write(uint16_t port, uint8_t value) {
  while (in_port_byte(0x64) & 2){}

  out_port_byte(port, value);
}

void keyboard_handler(struct interrupt_registers *registers)
{
    #if defined(DEBUG_KEYBOARD)
    printf("[keyboard] keyboard_handler called \n");
    #endif
    char byte = in_port_byte(0x60);
    char scan_code = byte & 0x7F;   // Lower 7 bits: which key
    char press = byte & 0x80;       // Bit 7: 0x80 = released, 0x00 = pressed
    #if defined(DEBUG_KEYBOARD)
    printf("[keyboard] keyboard_handler scan_code:%d ,press:%d \n");
    #endif
    
    

    switch(scan_code){
        case 1:
        case 29:
        case 56:
        case 59:
        case 60:
        case 61:
        case 62:
        case 63:
        case 64:
        case 65:
        case 66:
        case 67:
        case 68:
        case 87:
        case 88:
            break;
        case 42:
            //shift key
            if (press == 0){
                shift_down = true;
                
            }else{
                shift_down = false;
            }
            break;
        case 58:
            if (!capsLock && press == 0){
                capsLock = true;
            }else if (capsLock && press == 0){
                capsLock = false;
            }
            break;
        default:
            if (press == 0){
                if (shift_down || capsLock){
                    printf("%c", uppercase[scan_code]);
                }else{
                    printf("%c", lowercase[scan_code]);
                }
            }
            
    }

    if (press == 0)
    {
	if (scan_code == BACKSPACE)
    {
		backspace(key_buffer);
	}else if (scan_code == ENTER)
    {
		user_input(key_buffer);
		key_buffer[0] = '\0';
	}else{  
		char letter = sc_ascii[(int)scan_code];
		/*Remeber that printf only accepts char[] */
		if ((letter >= 'a' && letter <= 'z') ||
        (letter >= 'A' && letter <= 'Z') ||
        (letter >= '0' && letter <= '9') ||
        (letter == ' '))
    {
        char str[2] = {letter, '\0'};
        append(key_buffer, letter, sizeof(key_buffer));
    }
	}
    }

}

bool keyboard_is_occupied() { return !!kbBuff; }

bool keyboard_task_read(uint32_t taskId, char *buff, uint32_t limit, bool changeTaskState) {
  while (keyboard_is_occupied())
    ;
  Task *task = task_get(taskId);
  if (!task)
    return false;

  kbBuff = buff;
  kbCurr = 0;
  kbMax = limit;
  kbTaskId = taskId;

  if (changeTaskState)
    task->state = TASK_STATE_WAITING_INPUT;
  return true;
}

int tokenize(char *input, char *argv[], int max_args)
{
    int argc = 0;
    char *p = input;

    while (*p && argc < max_args)
    {
        while (*p == ' ')
            p++; // skip spaces
        if (*p == '\0')
            break;

        argv[argc++] = p; // start of token

        while (*p != ' ' && *p != '\0')
            p++; // go to end of token
        if (*p == ' ')
            *p++ = '\0'; // null-terminate token
    }

    return argc;
}


/* Safe backspace */
void backspace(char s[]){
    int len = strlen(s);
    if (len > 0){
        s[len-1] = '\0';
    }
}

/* Safe append */
void append(char s[], char n, int max_len){
    int len = strlen(s);

    if (len < max_len - 1){
        s[len] = n;
        s[len+1] = '\0';
    }
}

/* Convert integer to hexadecimal ASCII */
void hex_to_ascii(int n, char str[]){
    str[0] = '\0';   // IMPORTANT: initialize string

    append(str, '0', 32);
    append(str, 'x', 32);

    char zeros = 0;
    int32_t tmp;
    int i;

    for(i = 28; i > 0; i -= 4){
        tmp = (n >> i) & 0xF;

        if(tmp == 0 && zeros == 0)
            continue;

        zeros = 1;

        if(tmp >= 0xA)
            append(str, tmp - 0xA + 'a', 32);
        else
            append(str, tmp + '0', 32);
    }

    tmp = n & 0xF;

    if(tmp >= 0xA)
        append(str, tmp - 0xA + 'a', 32);
    else
        append(str, tmp + '0', 32);
}
// For directory listing
#define O_DIRECTORY 00200000
#define O_RDONLY    00000000


void user_input(char *input)
{
    char *argv[100];
    int argc = tokenize(input, argv, 100);
    if (argc == 0)
        return;

    if (strcmp(argv[0], "END") == 0)
    {
        printf("Stopping CPU\n");
        Halt();
    }
    else if (strcmp(argv[0], "CLEAR") == 0)
    {
        clear_screen();
    }
    else if (strcmp(argv[0], "PAGE") == 0)
    {
        uint32_t page = malloc(1000);
        char tmp[16];
        hex_to_ascii(page, tmp);
        printf("Allocated page at: %lx\n",tmp);

    }
    else if (strcmp(argv[0], "MK") == 0)
    {
        if (argc < 3)
        {
            printf("Usage: MK filename content\n");
        }
        else
        {
            char content[512];
            content[0] = '\0';
            for (int i = 2; i < argc; i++)
            {
                strcat(content, argv[i]);
                if (i != argc - 1)
                    strcat(content, " ");
            }

            
            if (fs_create(argv[1]) == 0)
            {
                
                fs_write(argv[1], content);
                printf("File created!\n");
            }
            else
            {
                printf("Error creating file.\n");
            }
        }
    }
    else if (strcmp(argv[0], "CAT") == 0)
    {
        if (argc < 2)
        {
            printf("Usage: CAT filename\n");
        }
        else
        {
            char buf[512] = {0};
            if (fs_read(argv[1], buf) == 0)
            {
                printf(buf);
                printf("\n");
            }
            else
            {
                printf("Error reading file.\n");
            }
            
        }
    }
        else if (strcmp(argv[0], "LS") == 0)
    {
        fs_list();
    }
    else if (strcmp(argv[0], "RM") == 0)
    {
        if (argc < 2)
        {
            printf("Usage: RM filename\n");
        }
        else
        {
            if (fs_delete(argv[1]) == 0)
            {
                printf("File deleted!\n");
            }
            else
            {
                printf("Error deleting file.\n");
            }
        }
    }
    else if (strcmp(argv[0], "HELP") == 0)
    {
        printf("Available commands:\n");
        printf("  END    - Halt the CPU\n");
        printf("  CLEAR  - Clear the screen\n");
        printf("  PAGE   - Allocate memory\n");
        printf("  MK     - Create file\n");
        printf("  CAT    - Show file content\n");
        printf("  LS     - List files\n");
        printf("  RM     - Delete file\n");
        printf("  ECHO   - Print text\n");
        printf("  INFO   - System info\n");
    }
    else if (strcmp(argv[0], "ECHO") == 0)
    {
        for (int i = 1; i < argc; i++)
        {
            printf(argv[i]);
            if (i != argc - 1)
                printf(" ");
        }
        printf("\n");
    }
    else if (strcmp(argv[0], "INFO") == 0)
    {
        printf("Nachtlauf v0.1\n");
        printf("64-bit long mode\n");
        printf("DISPLAY INFO\nframebuffer is at :%lx, tempframebuffer is at:%lx, \nwidth:%d, hight:%d, pitch:%d address:%lx\nframerate:%d\n", 
            bootloader.framebuffer, tempframebuffer, 
            tempframebuffer->width, tempframebuffer->height, 
            tempframebuffer->pitch, tempframebuffer->address,
            frequency);

    }
    else
    {
        printf("Unknown command: ");
        printf(argv[0]);
        printf("\n");
    }
}


