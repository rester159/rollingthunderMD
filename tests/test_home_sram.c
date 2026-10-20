#define main oldSaveTests
#include "test_save.c"
#undef main
int main(void)
{
    oldSaveTests();
    u8 payload[48]={0},actual[48],baseline[1024];
    RT_saveExtrasLoad();
    for(u8 slot=0;slot<4;slot++){
        assert(!RT_saveExtraRead(slot,actual));
        memset(payload,slot+1,48);assert(RT_saveExtraWrite(slot,payload));
    }
    RT_saveExtrasLoad();
    for(u8 slot=0;slot<4;slot++){
        assert(RT_saveExtraRead(slot,actual));
        for(u8 i=0;i<48;i++)assert(actual[i]==slot+1);
    }
    memcpy(baseline,memory,sizeof memory);
    for(int cut=0;cut<65;cut++){
        memcpy(memory,baseline,sizeof memory);RT_saveExtrasLoad();
        memset(payload,9,48);writes=0;limit=cut;assert(RT_saveExtraWrite(1,payload));limit=-1;
        RT_saveExtrasLoad();assert(RT_saveExtraRead(1,actual));
        for(u8 i=0;i<48;i++)assert(actual[i]==2);
        assert(!memcmp(memory,baseline,128));
    }
    assert(RT_saveExtraWrite(1,payload));
    RT_SaveReset snapshot;RT_saveResetExport(&snapshot);
    forbidReads=true;RT_saveResetImport(&snapshot);RT_saveExtrasLoad();
    assert(RT_saveExtraRead(1,actual)&&!memcmp(actual,payload,48));forbidReads=false;
    writes=0;assert(RT_saveExtraWrite(1,payload));assert(!writes);
    snapshot.extra[1][1][22]^=1;RT_saveResetImport(&snapshot);RT_saveExtrasLoad();
    assert(RT_saveExtraRead(1,actual)&&actual[0]==2);
    assert(!RT_saveExtraWrite(4,payload));assert(!RT_saveExtraRead(4,actual));
    puts("Three Campaign SRAM slots and profile: isolated journals, all interrupted writes, cold/warm restore, corruption fallback and unchanged saves passed.");
}
