#  DualFuse Operating System

**DualFuse** is a minimalist 64-bit operating system built entirely from scratch — designed to demonstrate the core principles of OS development, including kernel design, memory management, process handling, and ELF binary execution.  
It serves as both an educational tool and a foundation for further experimental or research-based system software.

---

##  Overview

DualFuse is built to provide a **clean, modular, and transparent** implementation of an operating system.  
Unlike hobby OS projects that rely heavily on existing codebases, DualFuse emphasizes understanding the full stack — from bootloader to user space.

### Key Features (Current & Planned)
- **Custom 64-bit Kernel** — Written in C and Assembly.
- **Limine Bootloader** — Modern multiboot-compatible loader.
- **Musl libc Integration** — Lightweight C standard library.
- **ELF Executable Support** — Parses and loads ELF binaries.
- **GCC-based Cross Toolchain** — Fully self-contained build process.
- **Minimal Shell (planned)** — For basic command execution.
- **Virtual Memory Management (planned)** — Paging and heap.
- **Custom GUI (future)** — Lightweight graphical layer on top of framebuffer.
