#include "rt_display.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static u32 randomState=0x524f4c4c;
static u16 randomWord(void){randomState=randomState*1664525+1013904223;return randomState>>16;}
int main(void)
{
    u16 blocks[256*16],buffer[64];
    for(u16 i=0;i<4096;i++)blocks[i]=randomWord();
    const u16 widths[]={16,64,128,608},heights[]={16,64,224};
    for(u8 w=0;w<4;w++)for(u8 h=0;h<3;h++){
        u16 width=widths[w],height=heights[h];u32 cells=(u32)width*height;
        u16 *map=malloc(cells/16*sizeof(u16)),*expanded=malloc(cells*sizeof(u16));
        for(u32 i=0;i<cells/16;i++){
            map[i]=randomWord()&255;
            for(u8 j=0;j<16;j++)expanded[i*16+j]=blocks[map[i]*16+j];
        }
        RT_DisplayStage stage={.map={map,map},.widthCells=width,.heightCells=height,
            .padding={0x0123,0x89ab},.blocks=blocks};
        for(u16 trial=0;trial<10000;trial++){
            u8 vertical=trial&1;u16 plane=(trial>>1)&1;
            s16 at=(s16)(randomWord()%((vertical?width:height)+128))-64;
            s16 start=(s16)(randomWord()%((vertical?height:width)+128))-64;
            RT_displayPackedStrip(&stage,plane,at,start,vertical,buffer);
            for(u16 i=0;i<(vertical?32:64);i++){
                s16 x=vertical?at:start+i,y=vertical?start+i:at;
                u16 expected=(x>=0 && x<width && y>=0 && y<height)?expanded[(u32)y*width+x]:stage.padding[plane];
                assert(buffer[(u16)(start+i)&(vertical?31:63)]==expected);
            }
        }
        free(map);free(expanded);
    }
    puts("120,000 packed rows/columns match expanded maps, including padding and ring wraps.");
}
