#include <bitmap.h>
#include <console.h>
#include <elf.h>
#include <framebufferutil.h>

#include <liballoc.h>

#include <paging.h>
#include <pmm.h>
#include <task_stack.h>
#include <syscalls.h>
#include <system.h>
#include <task.h>
#include <timer.h>
#include <utility.h>
#include <hcf.hpp>
#include <vfs.h>

// ELF (for now only 64) parser
// Copyright (C) 2024 Panagiotis

#define ELF_DEBUG 0

bool elf_check_file(Elf64_Ehdr *hdr) {
  if (!hdr)
    return false;
  if (hdr->e_ident[EI_MAG0] != ELFMAG0) {
    printf("[elf] Header EI_MAG0 incorrect.\n");
    return false;
  }
  if (hdr->e_ident[EI_MAG1] != ELFMAG1) {
    printf("[elf] Header EI_MAG1 incorrect.\n");
    return false;
  }
  if (hdr->e_ident[EI_MAG2] != ELFMAG2) {
    printf("[elf] Header EI_MAG2 incorrect.\n");
    return false;
  }
  if (hdr->e_ident[EI_MAG3] != ELFMAG3) {
    printf("[elf] Header EI_MAG3 incorrect.\n");
    return false;
  }
  if (hdr->e_ident[EI_CLASS] != ELFCLASS64 ||
      hdr->e_machine != ELF_x86_64_MACHINE) {
    printf("[elf] Architecture is not supported.\n");
    return false;
  }
  return true;
}

void elfProcessLoad(Elf64_Phdr *elf_phdr, uint8_t *out, size_t base) {
  // Map the (current) program page
  size_t   startRounded = (elf_phdr->p_vaddr & ~0xFFF);
  uint64_t pagesRequired = CEILING_DIVISION(
      (elf_phdr->p_vaddr - startRounded) + elf_phdr->p_memsz, 0x1000);
  for (int j = 0; j < pagesRequired; j++) {
    size_t vaddr = (elf_phdr->p_vaddr & ~0xFFF) + j * 0x1000;
    if (paging_virtual_to_physical(base + vaddr))
      continue;
    size_t paddr = physical_allocate(1);
    virtual_map(base + vaddr, paddr, PF_USER | PF_RW);
  }

  // Copy the required info
  memcpy((void *)(base + elf_phdr->p_vaddr), out + elf_phdr->p_offset,
         elf_phdr->p_filesz);

  // wtf is this? (needed)
  if (elf_phdr->p_memsz > elf_phdr->p_filesz)
    memset((void *)(base + elf_phdr->p_vaddr + elf_phdr->p_filesz), 0,
           elf_phdr->p_memsz - elf_phdr->p_filesz);
}

Task *elfExecute(char *filepath, uint32_t argc, char **argv, uint32_t envc, char **envv, bool startup) {
  // Open & read executable file
  OpenFile *dir = file_system_kernel_open(filepath, O_RDONLY, 0);
  if (!dir) {
    printf("[elf] Could not open %s\n", filepath);
    return 0;
  }
  size_t filesize = file_system_get_filesize(dir);
#if ELF_DEBUG
  printf("[elf] Executing %s: filesize{%d}\n", filepath, filesize);
#endif
  uint8_t *out = (uint8_t *)malloc(filesize);
  file_system_read_full_file(dir, out);
  file_system_kernel_close(dir);

  // Cast ELF32 header
  Elf64_Ehdr *elf_ehdr = (Elf64_Ehdr *)(out);

  if (!elf_check_file(elf_ehdr)) {
    printf("[elf] File %s is not a valid cavOS ELF32 executable!\n", filepath);
    return 0;
  }

  // Create a new page directory which is later used by the process
  uint64_t *oldpagedir = get_page_directory();
  uint64_t *pagedir = page_directory_allocate();
  change_page_directory(pagedir);

#if ELF_DEBUG
  printf("\n[elf_ehdr] entry=%x type=%d arch=%d\n", elf_ehdr->e_entry,
         elf_ehdr->e_type, elf_ehdr->e_machine);
  printf("[early phdr] offset=%x count=%d size=%d\n", elf_ehdr->e_phoff,
         elf_ehdr->e_phnum, elf_ehdr->e_phentsize);
#endif

  int32_t id = task_generate_id();
  if (id == -1) {
    printf(
        "[elf] Cannot fetch task id... You probably reached the task limit!");
    // optional
    // printf("[elf] Cannot fetch task id... You probably reached the task
    // limit!");
    Halt();
  }

  size_t interpreterEntry = 0;
  size_t interpreterBase = 0x100000000000; // todo: not hardcode
  // Loop through the multiple ELF32 program header tables
  for (int i = 0; i < elf_ehdr->e_phnum; i++) {
    Elf64_Phdr *elf_phdr = (Elf64_Phdr *)((size_t)out + elf_ehdr->e_phoff +
                                          i * elf_ehdr->e_phentsize);
    if (elf_phdr->p_type == 3) {
      char     *interpreterFilename = (char *)(out + elf_phdr->p_offset);
      OpenFile *interpreter = file_system_kernel_open(interpreterFilename, O_RDONLY, 0);
      if (!interpreter) {
        printf("[elf] Interpreter path{%s} could not be found!\n",
               interpreterFilename);
        Halt();
      }
      size_t size = file_system_get_filesize(interpreter);

      uint8_t *interpreterContents = (uint8_t *)malloc(size);
      file_system_read_full_file(interpreter, interpreterContents);
      file_system_kernel_close(interpreter);

      Elf64_Ehdr *interpreterEhdr = (Elf64_Ehdr *)(interpreterContents);
      if (interpreterEhdr->e_type != 3) { // ET_DYN
        printf("[elf::dyn] Interpreter{%s} isn't really of type ET_DYN!\n",
               interpreterFilename);
        Halt();
      }
      interpreterEntry = interpreterEhdr->e_entry;
      for (int i = 0; i < interpreterEhdr->e_phnum; i++) {
        Elf64_Phdr *interpreterPhdr =
            (Elf64_Phdr *)((size_t)interpreterContents +
                           interpreterEhdr->e_phoff +
                           i * interpreterEhdr->e_phentsize);
        if (interpreterPhdr->p_type != PT_LOAD)
          continue;
        elfProcessLoad(interpreterPhdr, interpreterContents, interpreterBase);
      }
      free(interpreterContents);

      continue;
    }
    if (elf_phdr->p_type != PT_LOAD)
      continue;

    elfProcessLoad(elf_phdr, out, 0);

#if ELF_DEBUG
    printf("[elf] Program header: type{%d} offset{%x} vaddr{%x} size{%x} "
           "alignment{%x}\n",
           elf_phdr->p_type, elf_phdr->p_offset, elf_phdr->p_vaddr,
           elf_phdr->p_memsz, elf_phdr->p_align);
#endif
  }

  // For the foreseeable future ;)
#if ELF_DEBUG
  // for (int i = 0; i < elf_ehdr->e_shnum; i++) {
  //   Elf64_Shdr *elf_shdr = (Elf64_Shdr *)((uint32_t)out + elf_ehdr->e_shoff +
  //                                         i * elf_ehdr->e_shentsize);
  //   printf("[elf] Section header: type{%d} offset{%lx}\n", elf_shdr->sh_type,
  //          elf_shdr->sh_offset);
  // }
#endif

#if ELF_DEBUG
  printf("[elf] New pagedir: offset{%x}\n", pagedir);
#endif

  // Done loading into the pagedir
  change_page_directory(oldpagedir);

  Task *target =
      task_create(id,
                 interpreterEntry ? (interpreterBase + interpreterEntry)
                                  : elf_ehdr->e_entry,
                 false, pagedir, argc, argv);

  // libc takes care of tls lmao
  /*if (tls) {
    change_page_directory(pagedir);

    size_t tls_start = tls->p_vaddr;
    size_t tls_end = tls->p_vaddr + tls->p_memsz;

    printf("[elf::tls] Found: virt{%lx} len{%lx}\n", tls->p_vaddr,
           tls->p_memsz);
    uint8_t *tls = (uint8_t *)target->heap_end;
    task_adjust_heap(target, target->heap_end + 4096);

    target->fsbase = (size_t)tls + 512;
    *(uint64_t *)(tls + 512) = (size_t)tls + 512;

    uint8_t *tlsp =
        (uint8_t *)((size_t)tls + 512 - (tls_end - tls_start)); // copy tls
    for (uint8_t *i = (uint8_t *)tls_start; (size_t)i < tls_end; i++)
      *tlsp++ = *i++;

    change_page_directory(oldpagedir);
  }*/

  // Current working directory init
  target->cwd = (char *)malloc(2);
  target->cwd[0] = '/';
  target->cwd[1] = '\0';

  // User stack generation: the stack itself, AUXs, etc...
  stack_generate_user(target, argc, argv, envc, envv, out, filesize, elf_ehdr);
  free(out);

  // void **a = (void **)(&target->firstSpecialFile);
  // fsUserOpenSpecial(a, "/dev/stdin", target, 0, &stdio);
  // fsUserOpenSpecial(a, "/dev/stdout", target, 1, &stdio);
  // fsUserOpenSpecial(a, "/dev/stderr", target, 2, &stdio);

  // fsUserOpenSpecial(a, "/dev/fb0", target, -1, &fb0);
  // fsUserOpenSpecial(a, "/dev/tty", target, -1, &stdio);

  int stdin = file_system_user_open(target, "/dev/stdin", O_RDWR, 0);
  int stdout = file_system_user_open(target, "/dev/stdout", O_RDWR, 0);
  int stderr = file_system_user_open(target, "/dev/stderr", O_RDWR, 0);

  if (stdin < 0 || stdout < 0 || stderr < 0) {
    printf("[elf] Couldn't establish basic IO!\n");
    Halt();
  }

  OpenFile *fdStdin = file_system_user_get_node(target, stdin);
  OpenFile *fdStdout = file_system_user_get_node(target, stdout);
  OpenFile *fdStderr = file_system_user_get_node(target, stderr);

  fdStdin->id = 0;
  fdStdout->id = 1;
  fdStderr->id = 2;
  // todo fixup all of the ^

  // Align it, just in case...
  task_adjust_heap(target, CEILING_DIVISION(target->heap_end, 0x1000) * 0x1000,
                 &target->heap_start, &target->heap_end);

  // Just a sane default
  target->parent = currentTask;

  if (startup)
    task_create_finish(target);

  return target;
}
