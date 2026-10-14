
// smp.h
#ifndef SMP_H
#define SMP_H

#include <types.h>

extern volatile uint8_t smp_ready_for_ap_ticks;

void smp_install_trampoline();
void smp_boot_all_aps();
#endif