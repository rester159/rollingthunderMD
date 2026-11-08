#ifndef GENESIS_H
#define GENESIS_H
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
typedef int8_t s8; typedef int16_t s16; typedef int32_t s32;
#define TRUE true
#define FALSE false
void SYS_disableInts(void);void SYS_enableInts(void);
bool Z80_getAndRequestBus(bool wait);void Z80_releaseBus(void);
void SRAM_enable(void);void SRAM_disable(void);
u8 SRAM_readByte(u32 offset);void SRAM_writeByte(u32 offset,u8 value);
#endif
