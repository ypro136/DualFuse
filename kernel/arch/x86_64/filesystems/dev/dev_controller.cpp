#include <dev.h>

#include <liballoc.h>
#include <utility.h>

#include <syscalls.h>

Fakefs rootDev = {0};

MountPoint *firstMountPoint;



void dev_setup() {
  // fake_file_system_add_file(&rootDev, rootDev.rootFile, "stdin", 0,
  //               S_IFCHR | S_IRUSR | S_IWUSR, &stdio);
  // fake_file_system_add_file(&rootDev, rootDev.rootFile, "stdout", 0,
  //               S_IFCHR | S_IRUSR | S_IWUSR, &stdio);
  // fake_file_system_add_file(&rootDev, rootDev.rootFile, "stderr", 0,
  //               S_IFCHR | S_IRUSR | S_IWUSR, &stdio);
  // fake_file_system_add_file(&rootDev, rootDev.rootFile, "tty", 0,
  //               S_IFCHR | S_IRUSR | S_IWUSR, &stdio);
  // fake_file_system_add_file(&rootDev, rootDev.rootFile, "fb0", 0,
  //               S_IFCHR | S_IRUSR | S_IWUSR, &fb0);
  // fake_file_system_add_file(&rootDev, rootDev.rootFile, "null", 0,
  //               S_IFCHR | S_IRUSR | S_IWUSR, &handleNull);
}

bool dev_mount(MountPoint *mount) {
  // install handlers
  mount->handlers = &fake_file_systemHandlers;
  mount->stat = fake_file_system_stat;
  mount->lstat = fake_file_system_lstat;

  mount->fsInfo = malloc(sizeof(FakefsOverlay));
  memset(mount->fsInfo, 0, sizeof(FakefsOverlay));
  FakefsOverlay *dev = (FakefsOverlay *)mount->fsInfo;

  dev->fake_file_system = &rootDev;
  if (!rootDev.rootFile) {
    fake_file_system_setup_root(&rootDev.rootFile);
    dev_setup();
  }

  return true;
}
