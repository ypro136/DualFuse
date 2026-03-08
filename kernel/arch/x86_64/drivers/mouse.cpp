#include <framebufferutil.h>
#include <isr.h>
#include <mouse.h>
#include <system.h>
#include <apic.h>
#include <timer.h>
#include <utility.h>
#include <linux_event_codes.h>

// PS/2 mouse driver, should work with USB mice too
 

/* Global variable definition */
DevInputEvent *mouseEvent = nullptr;

void mouseWait(uint8_t a_type) {
  uint32_t timeout = MOUSE_TIMEOUT;
  if (!a_type) {
    while (--timeout) {
      if (in_port_byte(MOUSE_STATUS) & MOUSE_BBIT)
        break;
    }
  } else {
    while (--timeout) {
      if (!((in_port_byte(MOUSE_STATUS) & MOUSE_ABIT)))
        break;
    }
  }
}

static inline int apply_mouse_accel(int delta)
{
    if (delta == 0) return 0;

    int sign = (delta > 0) ? 1 : -1;
    int abs_delta = (delta < 0) ? -delta : delta;

    // Scale down first
    int scaled = abs_delta / MOUSE_BASE_DIVISOR;

    // Apply acceleration if movement is fast
    if (abs_delta > MOUSE_ACCEL_THRESHOLD)
        scaled += (abs_delta - MOUSE_ACCEL_THRESHOLD) * MOUSE_ACCEL_FACTOR / MOUSE_BASE_DIVISOR;

    // Always move at least 1 pixel if there was any input
    if (scaled == 0) scaled = 1;

    return sign * scaled;
}

void mouseWrite(uint8_t write) {
  mouseWait(1);
  out_port_byte(MOUSE_STATUS, MOUSE_WRITE);
  mouseWait(1);
  out_port_byte(MOUSE_PORT, write);
}

uint8_t mouseRead() {
  mouseWait(0);
  char t = in_port_byte(MOUSE_PORT);
  return t;
}

int mouseCycle = 0;

int mouse1 = 0;
int mouse2 = 0;

int mouse_position_x = 0;
int mouse_position_y = 0;

bool clickedLeft = false;
bool clickedRight = true;

//DevInputEvent *mouseEvent;

void mouseIrq() {
  // Non-blocking version: just read the port directly without waiting
  // This avoids the blocking mouseWait() calls that freeze the system
  uint8_t byte = in_port_byte(MOUSE_PORT);
  
  #if defined(DEBUG_MOUSE)
    static int irqCount = 0;
    if (++irqCount % 100 == 0) 
    {
      printf("[mouse-irq] IRQ fired %d times\n", irqCount);
    }
  #endif
  
  // rest are just for demonstration

  if (mouseCycle == 0) {
    mouse1 = byte;
    #if defined(DEBUG_MOUSE)
      printf("[mouse-irq] Byte 0: %02x\n", mouse1);
    #endif
    if (!(mouse1 & (1 << 3))) {
        #if defined(DEBUG_MOUSE)
        printf("[mouse-irq] Desync detected! Byte 0 missing sync bit, discarding\n");
        #endif
        mouseCycle = 0;  // stay at 0, wait for real packet start
        return;
    }
  }
  else if (mouseCycle == 1) {
    mouse2 = byte;
    #if defined(DEBUG_MOUSE)
      printf("[mouse-irq] Byte 1: %02x\n", mouse2);
    #endif
  }
  else {
    int mouse3 = byte;
    #if defined(DEBUG_MOUSE)
      printf("[mouse-irq] Byte 2: %02x (complete packet)\n", mouse3);
    #endif
    
    do {
      // Note: The bit 3 check in real hardware is important, but we skip it here
      // for compatibility with test environments like QEMU
      // assert(mouse1 & (1 << 3));
      
      int x = mouse2;
      int y = mouse3;
      if (x && mouse1 & (1 << 4))
        x -= 0x100;
      if (y && mouse1 & (1 << 5))
        y -= 0x100;

      mouse_position_x += apply_mouse_accel(x);
      mouse_position_y += apply_mouse_accel(-y);

      if (mouse_position_x < 0)
        mouse_position_x = 0;
      if (mouse_position_y < 0)
        mouse_position_y = 0;
      if (mouse_position_x > tempframebuffer_data.width)
        mouse_position_x = tempframebuffer_data.width;
      if (mouse_position_y > tempframebuffer_data.height)
        mouse_position_y = tempframebuffer_data.height;

      bool click = mouse1 & (1 << 0);
      bool rclick = mouse1 & (1 << 1);
      
      clickedRight = rclick;
      clickedLeft = click;

      // do nothing... seriously!
      (void)mouse3;
    } while (0);
  }

  mouseCycle++;
  if (mouseCycle > 2)
    mouseCycle = 0;
}

void bitmapset(uint8_t *map, size_t toSet) {
  size_t div = toSet / 8;
  size_t mod = toSet % 8;
  map[div] |= (1 << mod);
}

size_t mouseEventBit(OpenFile *fd, uint64_t request, void *arg) {
  size_t number = _IOC_NR(request);
  size_t size = _IOC_SIZE(request);

  size_t ret = ERR(ENOENT);
  switch (number) {
  case 0x20: {
    size_t out = (1 << EV_SYN) | (1 << EV_KEY) | (1 << EV_REL);
    ret = MIN(sizeof(size_t), size);
    memcpy(arg, &out, ret);
    break;
  }
  case (0x20 + EV_SW):
  case (0x20 + EV_MSC):
  case (0x20 + EV_SND):
  case (0x20 + EV_LED):
  case (0x20 + EV_ABS): {
    ret = MIN(sizeof(size_t), size);
    break;
  }
  case (0x20 + EV_FF): {
    ret = MIN(16, size);
    break;
  }
  case (0x20 + EV_REL): {
    size_t out = (1 << REL_X) | (1 << REL_Y);
    ret = MIN(sizeof(size_t), size);
    memcpy(arg, &out, ret);
    break;
  }
  case (0x20 + EV_KEY): {
    uint8_t map[96] = {0};
    bitmapset(map, BTN_RIGHT);
    bitmapset(map, BTN_LEFT);
    ret = MIN(96, size);
    memcpy(arg, map, ret);
    break;
  }
  case (0x40 + ABS_X): {
    assert(size >= sizeof(struct input_absinfo));
    struct input_absinfo *target = (struct input_absinfo *)arg;
    memset(target, 0, sizeof(struct input_absinfo));
    target->value = 0; // todo
    target->minimum = 0;
    target->maximum = tempframebuffer_data.width;
    ret = 0;
    break;
  }
  case (0x40 + ABS_Y): {
    assert(size >= sizeof(struct input_absinfo));
    struct input_absinfo *target = (struct input_absinfo *)arg;
    memset(target, 0, sizeof(struct input_absinfo));
    target->value = 0; // todo
    target->minimum = 0;
    target->maximum = tempframebuffer_data.height;
    ret = 0;
    break;
  }
  case 0x18: // EVIOCGKEY()
    ret = MIN(96, size);
    break;
  case 0x19: // EVIOCGLED()
    ret = MIN(8, size);
    break;
  case 0x1b: // EVIOCGSW()
    ret = MIN(8, size);
    break;
  }

  return ret;
}

void initiateMouse() {
  // printf("[mouse] Starting mouse initialization\n");
  
  // mouseEvent = devInputEventSetup("PS/2 Mouse");
  // printf("[mouse] Device setup complete\n");
  
  // below is optional (vmware didn't do it)
  // mouseEvent->properties = INPUT_PROP_POINTER;
  // printf("[mouse] Setting input ID fields\n");
  // mouseEvent->inputid.bustype = 0x05;   // BUS_PS2
  // mouseEvent->inputid.vendor = 0x045e;  // Microsoft
  // mouseEvent->inputid.product = 0x00b4; // Generic MS Mouse
  // mouseEvent->inputid.version = 0x0100; // Basic MS Version
  // mouseEvent->eventBit = mouseEventBit;
  // printf("[mouse] Input ID fields set\n");

  // enable the auxiliary mouse
  printf("[mouse] Enabling auxiliary mouse device\n");
  mouseWait(1);
  out_port_byte(0x64, 0xA8);

  // irq handler
  printf("[mouse] Setting up mouse IRQ handler\n");
  uint8_t targIrq = 12;
  if(apic_initialized)
  {
    targIrq = ioApicRedirect(targIrq, false);
  }

  if (isr_initialized)
    {
        irq_install_handler(12, mouseIrq);
    }
    else 
    {
        printf("[mouse] warning: isr not initialized. Mouse initialization omitted");
        return;
    }

  // enable interrupts
  printf("[mouse] Enabling mouse interrupts\n");
  mouseWait(1);
  out_port_byte(0x64, 0x20);
  
  // printf("[mouse] About to call sleep...\n");
  // sleep(100);
  
  mouseWait(0);
  // sleep(100);
  
  uint8_t status;
  status = (in_port_byte(0x60) | 2);
  mouseWait(1);
  out_port_byte(0x64, 0x60);
  mouseWait(1);
  out_port_byte(0x60, status);

  // default settings
  printf("[mouse] Setting default mouse settings\n");
  mouseWrite(0xF6);
  mouseRead();

  // enable device
  printf("[mouse] Enabling mouse device\n");
  mouseWrite(0xF4);
  mouseRead();
  
  printf("[mouse] Mouse initialization complete (IRQ: %d)\n", targIrq);
}