#include <disk.h>
#include <ext2.h>
#include <fat32.h>
#include <liballoc.h>
#include <poll.h>
#include <string.h>
#include <syscalls.h>
#include <system.h>
#include <task.h>
#include <unixSocket.h>
#include <utility.h>
#include <vfs.h>

// VFS-sided implementation of generic *poll() functions
// Copyright (C) 2025 Panagiotis

// todo: think this through as pairs span across more than one fd (think of
// pipes with read/write ends, etc)
void fsInformReady(OpenFile *fd, int epollEvents) {
  // spinlock_acquire(&fd->LOCK_POLL);
  // do nothing
  // spinlock_release(&fd->LOCK_POLL);
}
