#include "rt_rom_resource_copy.h"
#include "rt_rom_layout.h"

static u8 mappedByte(u32 address)
{
    return *(const volatile u8 *)address;
}

bool RT_copyRomResource(void *output,u32 address,u16 length,u32 romBytes)
{
    u8 *destination=output;
    u16 previousBank;
    if(!output || !length || length>256 || romBytes>0x2000000UL ||
       address>=romBytes || length>romBytes-address)return FALSE;
    SYS_disableInts();
    previousBank=SYS_getBank(7);
    while(length){
        u32 mapped=address;
        u16 count=length;
        u32 available=0x80000UL-(address&0x7FFFFUL);
        if(available<count)count=available;
#ifndef RT_LINEAR_ROM
        if(address>=0x280000UL){
            u16 bank=address>>19;
            if(SYS_getBank(7)!=bank)SYS_setBank(7,bank);
            mapped=0x380000UL+(address&0x7FFFFUL);
        }
#endif
        address+=count;length-=count;
        while(count--)*destination++=mappedByte(mapped++);
    }
    if(SYS_getBank(7)!=previousBank)SYS_setBank(7,previousBank);
    SYS_enableInts();
    return TRUE;
}
