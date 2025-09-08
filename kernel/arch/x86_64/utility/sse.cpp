#include <console.h>
#include <utility.h>

void initiateSSE() 
{
    // enable SSE
    asm volatile("mov %%cr0, %%rax;"
                        "and $0xFFFB, %%ax;"
                        "or  $2, %%eax;"
                        "mov %%rax, %%cr0;"
                        "mov %%cr4, %%rax;"
                        "or  $0b11000000000, %%rax;"
                        "mov %%rax, %%cr4;"
                 :
                 :
                 : "rax");
  
    // set NE in cr0 and reset x87 fpu
    asm volatile("fninit;"
                        "mov %%cr0, %%rax;"
                        "or $0b100000, %%rax;"
                        "mov %%rax, %%cr0;"
                 :
                 :
                 : "rax");
  }