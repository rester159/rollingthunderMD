#include "rt_cart_probe.h"
#include "rt_rom_layout.h"
volatile u8 rtMapperSupported;
bool RT_cartridgeBankingAvailable(void)
{
#ifdef RT_LINEAR_ROM
    rtMapperSupported=TRUE;
    return TRUE;
#else
    /* Only inspect ROM. Never try a write-enable protocol on an unknown cart.
     * Code and reference data remain in fixed windows 0/1 throughout. */
    SYS_disableInts();
    bool bus=Z80_getAndRequestBus(TRUE);
    u16 saved=SYS_getBank(7);
    bool valid=TRUE;
    for(u16 page=0;page<2;page++){
        SYS_setBank(7,page);
        const vu16 *reference=(const vu16 *)(page*0x80000UL+0x100);
        const vu16 *mapped=(const vu16 *)0x380100;
        for(u8 i=0;i<16;i++)if(reference[i]!=mapped[i])valid=FALSE;
    }
    SYS_setBank(7,saved);
    if(!bus)Z80_releaseBus();
    SYS_enableInts();
    rtMapperSupported=valid;
    return valid;
#endif
}
