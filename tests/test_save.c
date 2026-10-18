#include <assert.h>
#include <stdio.h>
#include "rt_save.h"
static u8 memory[256];static int writes,limit=-1;static bool enabled,forbidReads;
void SYS_disableInts(void){} void SYS_enableInts(void){}
bool Z80_getAndRequestBus(bool wait){(void)wait;return false;}void Z80_releaseBus(void){}
void SRAM_enable(void){assert(!enabled);enabled=true;}void SRAM_disable(void){enabled=false;}
u8 SRAM_readByte(u32 offset){assert(enabled&&offset<256&&!forbidReads);return memory[offset];}
void SRAM_writeByte(u32 offset,u8 value){assert(enabled&&offset<256);if(limit<0||writes<limit)memory[offset]=value;writes++;}
static u8 controls;
static u8 records[35]={0,0x30,0,5,10,15,41,0,0x20,0,4,10,15,20,0,0x10,0,3,10,15,20,0,8,0,2,10,15,20,0,4,0,1,10,15,20};
static void load(u32 expected,u8 credit,u8 muted){u8 r[35],c,m;u32 b;limit=-1;assert(RT_saveLoad(r,&b,&c,&m,&controls));assert(b==expected&&c==credit&&m==muted&&!memcmp(r,records,35));}
static u32 crc(const u8 *p){u32 c=0xffffffffUL;for(u8 i=0;i<60;i++){c^=p[i];for(u8 j=0;j<8;j++)c=(c>>1)^((c&1)?0xedb88320UL:0);}return ~c;}
int main(void){
    u8 r[35],c=6,m=0;u32 b=3000;
    memset(memory,255,sizeof memory);assert(!RT_saveLoad(r,&b,&c,&m,&controls));assert(c==6&&b==3000);
    assert(RT_saveStore(records,3000,6,0,0));load(3000,6,0);
    writes=0;assert(RT_saveStore(records,3000,6,0,0));assert(!writes);
    u8 baseline[128];memcpy(baseline,memory,128);
    /* Every possible interrupted write must retain the preceding valid slot. */
    for(int cut=0;cut<65;cut++){
        memcpy(memory,baseline,128);load(3000,6,0);writes=0;limit=cut;
        assert(RT_saveStore(records,654321,9,1,0));load(3000,6,0);
    }
    limit=-1;assert(RT_saveStore(records,654321,9,1,0));load(654321,9,1);
    RT_SaveReset snapshot;RT_saveResetExport(&snapshot);
    forbidReads=true;RT_saveResetImport(&snapshot);load(654321,9,1);
    /* An interrupted/corrupt mirror publication falls back to the other slot. */
    snapshot.slots[1][22]^=1;RT_saveResetImport(&snapshot);load(3000,6,0);
    memset(snapshot.slots,0,sizeof snapshot.slots);RT_saveResetImport(&snapshot);
    assert(!RT_saveLoad(r,&b,&c,&m,&controls));
    forbidReads=false;
    memory[64+22]^=1;load(3000,6,0); /* newest CRC invalid */
    assert(!RT_saveStore(records,3000,0,0,0));assert(!RT_saveStore(records,1000000,6,0,0));
    memset(memory,0,sizeof memory);assert(!RT_saveLoad(r,&b,&c,&m,&controls));
    limit=0;writes=0;assert(!RT_saveLoad(r,&b,&c,&m,&controls));assert(!rtSaveAvailable);
    assert(!RT_saveStore(records,3000,6,0,0));
    limit=-1;assert(!RT_saveLoad(r,&b,&c,&m,&controls));
    assert(RT_saveStore(records,3000,6,0,9));controls=0;load(3000,6,0);assert(controls==9);
    assert(!RT_saveStore(records,3000,6,0,3));assert(!RT_saveStore(records,3000,6,0,12));
    /* Import a real v1 payload: retain scores/settings and default controls. */
    memset(memory+64,0,64);memory[4]=1;memory[5]=41;memory[51]=0;
    u32 oldcrc=crc(memory);for(u8 i=0;i<4;i++)memory[60+i]=oldcrc>>(24-i*8);
    controls=10;load(3000,6,0);assert(controls==0);
    assert(RT_saveStore(records,3000,6,0,6));controls=0;load(3000,6,0);assert(controls==6);
    puts("SRAM: roundtrip, unchanged writes, 65 interrupted writes, corruption, defaults, warm reset without SRAM reads and validation passed.");
}
