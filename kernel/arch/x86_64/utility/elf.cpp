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
#include <fs.h>

// ELF (for now only 64) parser

#define ELF_DEBUG 0

extern OpenFile *fsRegisterNode(Task *task, size_t id);

// ---------------------------------------------------------------------------
// Minimal console handlers (write goes to serial, read returns 0 for now)
// ---------------------------------------------------------------------------
static size_t console_write(OpenFile *file, uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++)
        printf("%c", buf[i]);      // uses existing serial output
    return len;
}

static size_t console_read(OpenFile *file, uint8_t *buf, size_t len) {
    (void)file; (void)buf; (void)len;
    return 0;                       // no input yet
}

static size_t console_get_filesize(OpenFile *file) {
    (void)file;
    return 0;
}

static VfsHandlers dev_console_handlers = {
    .read        = console_read,
    .write       = console_write,
    .getFilesize = console_get_filesize,
    // all other function pointers left NULL
};

// ---------------------------------------------------------------------------
// ELF validation
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Load a PT_LOAD segment
// ---------------------------------------------------------------------------
void elfProcessLoad(Elf64_Phdr *elf_phdr, uint8_t *out, size_t base) {
  size_t   startRounded = (elf_phdr->p_vaddr & ~0xFFF);
  uint64_t pagesRequired = CEILING_DIVISION(
      (elf_phdr->p_vaddr - startRounded) + elf_phdr->p_memsz, 0x1000);
  for (int j = 0; j < pagesRequired; j++) {
    size_t vaddr = (elf_phdr->p_vaddr & ~0xFFF) + j * 0x1000;
    if (paging_virtual_to_physical(base + vaddr))
      continue;
    size_t paddr = physical_allocate(1);
    virtual_map(base + vaddr, paddr, PF_USER | PF_RW);
    tlb_shootdown_all();
  }

  memcpy((void *)(base + elf_phdr->p_vaddr), out + elf_phdr->p_offset,
         elf_phdr->p_filesz);

  if (elf_phdr->p_memsz > elf_phdr->p_filesz)
    memset((void *)(base + elf_phdr->p_vaddr + elf_phdr->p_filesz), 0,
           elf_phdr->p_memsz - elf_phdr->p_filesz);
}

// ---------------------------------------------------------------------------
// Main ELF loader
// ---------------------------------------------------------------------------
Task *elfExecute(char *filepath, uint32_t argc, char **argv, uint32_t envc,
                 char **envv, bool startup) {
    FIL elf_file_handle;
    if (f_open(&elf_file_handle, filepath, FA_READ) != FR_OK) {
        printf("[elf] Could not open %s\n", filepath);
        return 0;
    }
    size_t filesize = f_size(&elf_file_handle);
    uint8_t *elf_file_buffer = (uint8_t *)malloc(filesize);
    UINT elf_bytes_read;
    f_read(&elf_file_handle, elf_file_buffer, filesize, &elf_bytes_read);
    f_close(&elf_file_handle);

    Elf64_Ehdr *elf_ehdr = (Elf64_Ehdr *)elf_file_buffer;

    if (!elf_check_file(elf_ehdr)) {
        printf("[elf] %s is not a valid ELF64 executable\n", filepath);
        free(elf_file_buffer);
        return 0;
    }

    uint64_t *old_page_directory = get_page_directory();
    uint64_t *new_page_directory = page_directory_allocate();
    change_page_directory(new_page_directory);

    int32_t new_task_id = task_generate_id();
    if (new_task_id == -1) {
        printf("[elf] task id exhausted\n");
        Halt();
    }

    size_t interpreter_entry_point = 0;
    size_t interpreter_load_base   = 0x100000000000;

    for (int program_header_index = 0; program_header_index < elf_ehdr->e_phnum; program_header_index++) {
        Elf64_Phdr *program_header = (Elf64_Phdr *)((size_t)elf_file_buffer
                                     + elf_ehdr->e_phoff
                                     + program_header_index * elf_ehdr->e_phentsize);

        if (program_header->p_type == PT_INTERP) {
            char *interpreter_path = (char *)(elf_file_buffer + program_header->p_offset);
            FIL interpreter_file_handle;
            if (f_open(&interpreter_file_handle, interpreter_path, FA_READ) != FR_OK) {
                printf("[elf] interpreter %s not found\n", interpreter_path);
                Halt();
            }
            size_t interpreter_file_size = f_size(&interpreter_file_handle);
            uint8_t *interpreter_buffer = (uint8_t *)malloc(interpreter_file_size);
            UINT interpreter_bytes_read;
            f_read(&interpreter_file_handle, interpreter_buffer, interpreter_file_size, &interpreter_bytes_read);
            f_close(&interpreter_file_handle);

            Elf64_Ehdr *interpreter_ehdr = (Elf64_Ehdr *)interpreter_buffer;
            if (interpreter_ehdr->e_type != 3) { // ET_DYN
                printf("[elf] interpreter %s is not ET_DYN\n", interpreter_path);
                Halt();
            }
            interpreter_entry_point = interpreter_ehdr->e_entry;
            for (int interp_phdr_index = 0; interp_phdr_index < interpreter_ehdr->e_phnum; interp_phdr_index++) {
                Elf64_Phdr *interp_phdr = (Elf64_Phdr *)((size_t)interpreter_buffer
                                          + interpreter_ehdr->e_phoff
                                          + interp_phdr_index * interpreter_ehdr->e_phentsize);
                if (interp_phdr->p_type != PT_LOAD)
                    continue;
                elfProcessLoad(interp_phdr, interpreter_buffer, interpreter_load_base);
            }
            free(interpreter_buffer);
            continue;
        }

        if (program_header->p_type != PT_LOAD)
            continue;
        elfProcessLoad(program_header, elf_file_buffer, 0);
    }

    change_page_directory(old_page_directory);

    uint64_t entry_point = interpreter_entry_point
                         ? (interpreter_load_base + interpreter_entry_point)
                         : elf_ehdr->e_entry;

    Task *new_task = task_create(new_task_id, entry_point, false,
                                 new_page_directory, argc, argv);

    new_task->cmdline    = (char *)malloc(2);
    new_task->cmdline[0] = '/';
    new_task->cmdline[1] = '\0';

    size_t executable_load_base = 0;
    stackGenerateUser(new_task, argc, argv, envc, envv,
                      elf_file_buffer, filesize, elf_ehdr,
                      interpreter_entry_point ? interpreter_load_base : 0,
                      executable_load_base);
    free(elf_file_buffer);

    OpenFile *stdout_file = fsRegisterNode(new_task, 1);
    stdout_file->flags    = O_WRONLY;
    stdout_file->handlers = &dev_console_handlers;
    stdout_file->mountPoint = 0;

    OpenFile *stdin_file  = fsRegisterNode(new_task, 0);
    stdin_file->flags     = O_RDONLY;
    stdin_file->handlers  = &dev_console_handlers;
    stdin_file->mountPoint = 0;

    OpenFile *stderr_file = fsRegisterNode(new_task, 2);
    stderr_file->flags    = O_WRONLY;
    stderr_file->handlers = &dev_console_handlers;
    stderr_file->mountPoint = 0;

    spinlock_cnt_write_acquire(&new_task->infoFiles->WLOCK_FILES);
    bitmapGenericSet(new_task->infoFiles->fdBitmap, 0, true);
    bitmapGenericSet(new_task->infoFiles->fdBitmap, 1, true);
    bitmapGenericSet(new_task->infoFiles->fdBitmap, 2, true);
    spinlock_cnt_write_release(&new_task->infoFiles->WLOCK_FILES);

    task_adjust_heap(new_task,
                     DivRoundUp(new_task->infoPd->heap_end, 0x1000) * 0x1000,
                     &new_task->infoPd->heap_start,
                     &new_task->infoPd->heap_end);

    new_task->parent = current_task_this_core();
    task_create_finish(new_task);

    return new_task;
}