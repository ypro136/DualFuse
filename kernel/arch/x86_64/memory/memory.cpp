#include <memory.h>


#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>


#include <utility.h>
#include <data_structures/bitmap.h>
#include <pmm.h>
#include <vmm.h>
#include <liballoc.h>


#include <bootloader.h>



void memory_initialize()
{

    physical_memory_manager_initialize(bootloader.mmTotal, bootloader.mmEntryCnt, bootloader.mmEntries , bootloader.hhdmOffset);

    virtual_memory_manager_initialize(bootloader.mmTotal, bootloader.mmEntryCnt, bootloader.mmEntries , bootloader.hhdmOffset);

}




