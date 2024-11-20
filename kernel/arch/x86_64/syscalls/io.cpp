#include <console.h>
#include <framebufferutil.h>
#include <keyboard.h>
#include <linux.h>
#include <syscalls.h>
#include <task.h>

#include <liballoc.h>

// Manages /dev/std* files and the like

int read_handler(OpenFile *fd, uint8_t *in, size_t limit) {
  // console fb
  // while (kbIsOccupied()) {
  // } done in kbTaskRead()

  // assign new buffer
  uint8_t *kernelBuff = malloc(limit);

  // start reading
  keyboard_task_read(currentTask->id, (char *)kernelBuff, limit, true);
  asm volatile("sti"); // leave this task/execution (awaiting return)
  while (currentTask->state == TASK_STATE_WAITING_INPUT) {
    hand_control();
  }
  if (currentTask->term.c_lflag & ICANON)
    printf("\n"); // you technically pressed enter, didn't you?

  // finalise
  uint32_t fr = currentTask->tmpRecV;
  memcpy(in, kernelBuff, fr);
  if (currentTask->term.c_lflag & ICANON && fr < limit)
    in[fr++] = '\n';
  // only add newline if we can!

  return fr;
}

int write_handler(OpenFile *fd, uint8_t *out, size_t limit) {
  // console fb
  for (int i = 0; i < limit; i++) {
#if DEBUG_SYSCALLS_EXTRA
    serial_send(COM1, out[i]);
#endif
    printfch(out[i]);
  }
  return limit;
}

int ioctl_handler(OpenFile *fd, uint64_t request, void *arg) {
  switch (request) {
  case 0x5413: {
    winsize *win = (winsize *)arg;
    win->ws_row = screen_height / TTY_CHARACTER_HEIGHT;
    win->ws_col = screen_width / TTY_CHARACTER_WIDTH;

    win->ws_xpixel = screen_width;
    win->ws_ypixel = screen_height;
    return 0;
    break;
  }
  case TCGETS: {
    memcpy(arg, &currentTask->term, sizeof(termios));
    // printf("got %d %d\n", currentTask->term.c_lflag & ICANON,
    //        currentTask->term.c_lflag & ECHO);
    return 0;
    break;
  }
  case TCSETS:
  case TCSETSW:   // this drains(?), idek man
  case TCSETSF: { // idek anymore man
    memcpy(&currentTask->term, arg, sizeof(termios));
    // printf("setting %d %d\n", currentTask->term.c_lflag & ICANON,
    //        currentTask->term.c_lflag & ECHO);
    return 0;
    break;
  }
  case 0x540f: // TIOCGPGRP
  {
    int *pid = (int *)arg;
    *pid = currentTask->id;
    return 0;
    break;
  }
  default:
    return -1;
    break;
  }
}

size_t memory_map_handler(size_t addr, size_t length, int prot, int flags,
                   OpenFile *fd, size_t pgoffset) {
  printf("[io::mmap] FATAL! Tried to mmap on stdio!\n");
  return -1;
}

int stat_handler(OpenFile *fd, stat *target) {
  target->st_dev = 420;
  target->st_ino = rand(); // todo!
  target->st_mode = S_IFCHR | S_IRUSR | S_IWUSR;
  target->st_nlink = 1;
  target->st_uid = 0;
  target->st_gid = 0;
  target->st_rdev = 34830;
  target->st_blksize = 0x1000;
  target->st_size = 0;
  target->st_blocks = CEILING_DIVISION(target->st_size, 512);
  target->st_atime = 69;
  target->st_mtime = 69;
  target->st_ctime = 69;

  return 0;
}

VfsHandlers stdio = {
                     .read = read_handler,
                     .write = write_handler,
                     .ioctl = ioctl_handler,
                     .stat = stat_handler,
                     .mmap = memory_map_handler,
                     .getdents64 = 0,
                     .duplicate = 0,
                     .open = 0,
                     .close = 0
                     };

