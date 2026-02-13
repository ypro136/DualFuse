#include <dev.h>

#include <liballoc.h>
#include <utility.h>
#include <circular.h>

#include <syscalls.h>

// Global variable definitions
FakefsFile *inputFakedir = nullptr;
Fakefs      rootDev = {0};

// dev_event globals
DevInputEvent devInputEvents[MAX_EVENTS] = {0};
int           lastInputEvent = 0;

// dev_pty globals
VfsHandlers handlePtmx = {0};
VfsHandlers handlePts = {0};


/* dev_event.c implementations */
DevInputEvent *devInputEventSetup(char *devname) {
  if (lastInputEvent >= MAX_EVENTS) {
    printf("[dev_event] Exceeded maximum input events!\n");
    return nullptr;
  }

  DevInputEvent *event = &devInputEvents[lastInputEvent];
  event->devname = devname;
  event->timesOpened = 0;
  
  printf("[dev_event] Setting up input device: %s at index %d\n", devname, lastInputEvent);
  
  // Note: spinlock_acquire is commented out because the spinlock isn't properly initialized yet
  // The zero-initialized spinlock can cause crashes. For now, skip the acquire during setup.
  // spinlock_acquire(&event->LOCK_USERSPACE);
  
  // TODO: Implement proper spinlock initialization
  // TODO: Implement circular buffer for event queuing
  // CircularIntAllocate(&event->deviceEvents, EVENT_BUFFER_SIZE);
  
  printf("[dev_event] Input device setup complete\n");
  lastInputEvent++;
  return event;
}

void inputGenerateEvent(DevInputEvent *item, uint16_t type, uint16_t code,
                        int32_t value) {
  if (!item) return;
  
  // Create input event structure
  struct input_event evt;
  // TODO: Get current time from system timer
  evt.sec = 0;
  evt.usec = 0;
  evt.type = type;
  evt.code = code;
  evt.value = value;
  
  // Lock and add to circular buffer
  spinlock_acquire(&item->LOCK_USERSPACE);
  // TODO: Add event to circular buffer when CircularIntAllocate is implemented
  // CircularIntWrite(&item->deviceEvents, (uint8_t*)&evt, sizeof(struct input_event));
  spinlock_release(&item->LOCK_USERSPACE);
}

/* dev_setup and mounting */
void dev_setup() {
  // fakefsAddFile(&rootDev, rootDev.rootFile, "stdin", 0,
  //               S_IFCHR | S_IRUSR | S_IWUSR, &stdio);
  // fakefsAddFile(&rootDev, rootDev.rootFile, "stdout", 0,
  //               S_IFCHR | S_IRUSR | S_IWUSR, &stdio);
  // fakefsAddFile(&rootDev, rootDev.rootFile, "stderr", 0,
  //               S_IFCHR | S_IRUSR | S_IWUSR, &stdio);
  // fakefsAddFile(&rootDev, rootDev.rootFile, "tty", 0,
  //               S_IFCHR | S_IRUSR | S_IWUSR, &stdio);
  // fakefsAddFile(&rootDev, rootDev.rootFile, "fb0", 0,
  //               S_IFCHR | S_IRUSR | S_IWUSR, &fb0);
  // fakefsAddFile(&rootDev, rootDev.rootFile, "null", 0,
  //               S_IFCHR | S_IRUSR | S_IWUSR, &handleNull);
}

bool dev_mount(MountPoint *mount) {
  // install handlers
  mount->handlers = &fakefsHandlers;
  mount->stat = fakefsStat;
  mount->lstat = fakefsLstat;

  mount->fsInfo = malloc(sizeof(FakefsOverlay));
  memset(mount->fsInfo, 0, sizeof(FakefsOverlay));
  FakefsOverlay *dev = (FakefsOverlay *)mount->fsInfo;

  dev->fakefs = &rootDev;
  if (!(rootDev.rootFile)) {
    fakefsSetupRoot(&rootDev.rootFile);
    dev_setup();
  }

  return true;
}
