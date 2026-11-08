#include "rt_display_stream_release.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static u32 state=0x524f4c4c;
static u32 randomWord(void){return state=state*1664525+1013904223;}
static u8 upload(u16 pattern,u16 tile,void *context){return 1;}
int main(void)
{
    RT_DisplayStream reference,grouped;
    for(unsigned trial=0;trial<5000;trial++){
        memset(&reference,0,sizeof reference);
        assert(RT_tileCacheInitialize(&reference.tiles,64,32,64));
        u16 plane=randomWord()&1,column=randomWord()&63,row=randomWord()&31;
        u16 special=16,run=0;
        for(u16 p=0;p<2;p++)for(u16 y=0;y<32;y++)for(u16 x=0;x<64;x++){
            u16 at=p*2048+y*64+x,pattern=(randomWord()>>16)&15;
            if(p==plane && x==column){
                if(!run){run=1+(randomWord()>>16)%12;special=16+(randomWord()>>16)%32;}
                run--;pattern=special;
            }
            if((randomWord()>>16)%17==0){reference.cells[at]=65535;continue;}
            u16 attr=pattern|((randomWord()>>16)&0xf800);
            assert(RT_tileCacheAcquire(&reference.tiles,attr,reference.cells+at,upload,0));
        }
        grouped=reference;
        for(u16 j=0;j<32;j++){
            u16 index=plane*2048+((row+j)&31)*64+column;
            if(reference.cells[index]==65535)continue;
            assert(RT_tileCacheRelease(&reference.tiles,reference.cells[index]));
            reference.cells[index]=65535;
        }
        assert(RT_displayReleaseColumn(&grouped,plane,column,row));
        assert(!memcmp(&reference,&grouped,sizeof reference));
    }
    puts("5,000 columns: identical cell ownership, reference counts and free-list order across ring wraps.");
}
