/*****************************************************************************
*
* © 2014 Cyber-SB. Written by Selyutin Alex.
*
*****************************************************************************/

#include "crc32.h"
#include "pins.h"
#include "memory/memory.h"
#include "memory/firmware.h"
#include "mem_map.h"
#include "version.h"
#include "stellaris.h"

#include "debug/debug.h"

#define INVALID_MEMORY_VALUE    0xFFFFFFFF

#ifdef DEBUG
void __error__(char *pcFilename, unsigned long ulLine)
{
    dbg_printf("__boot_error__(\"%s\", ln=%u);\n", pcFilename, ulLine);
    tn_halt();
}
#endif

typedef void (*app_entry_t)();

int main()
{
    //tn_cpu_int_disable(); called in tn_reset_int_handler();
    //hw_watchdog_init();
    
    dbg_init();
    dbg_printf("\nBooting...\nBoot loader version %u.%u\n", (unsigned)BOOT_VER->major, (unsigned)BOOT_VER->minor);

    crc32_init();
    opins_init();
    iflash_init();  // internal FLASH memory initialization

    dbg_printf("Finding firmware update...");
    if (firmware_find())
    {
        dbg_puts("[update found]");

        dbg_printf("Firmware update...");
        //pin_on(OPIN_EXT_LED);
        firmware_update();
        //pin_off(OPIN_EXT_LED);
        dbg_puts("[done]");
    }
    else
        dbg_puts("[not found]");

    dbg_printf("Startup firmware..."); // before opins_free();

    unsigned long const stk_p       = HWREG(MEM_FW_PTR);
    app_entry_t const   app_entry   = (app_entry_t)HWREG(MEM_FW_PTR + sizeof(long));
    
    /*
    if ((unsigned long)app_entry < MEM_FW_PTR || (unsigned long)app_entry > MEM_FW_PTR + MEM_FW_SZ - sizeof(void*) ||
        stk_p < MEM_RAM_PTR + TN_MIN_STACK_SIZE || stk_p > MEM_RAM_PTR + MEM_RAM_SZ - sizeof(void*))
    {
        dbg_puts("[fail, firmware not found]");
        tn_halt_boot();
    }
    else
        dbg_puts(""); // put "\n"
    */
    dbg_free();
    opins_free();

    // jump to common firmware
    HWREG(NVIC_VTABLE) = MEM_FW_PTR;
    __set_SP(stk_p); // IAR function, set stack pointer
    app_entry();
    return 0;
}
