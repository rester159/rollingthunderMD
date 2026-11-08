/* Exhaustively compare the optimized conversion to decimal division. */
#include <stdint.h>
typedef int8_t s8;
#include "../src/campaign_hud.c"
#include <assert.h>
#include <stdio.h>
int main(void)
{
    RT_HudCell actual[6],expected[6];
    for(u8 count=1;count<=6;count++){
        u32 limit=1;for(u8 i=0;i<count;i++)limit*=10;
        for(u32 value=0;value<limit;value++){
            u32 n=value;
            for(u8 i=count;i;i--){expected[i-1]=(RT_HudCell){n%10,252};n/=10;}
            for(u8 i=0;i+1<count && !expected[i].glyph;i++)expected[i].glyph=255;
            digits(actual,value,count,252);
            assert(!memcmp(actual,expected,count*sizeof(*actual)));
        }
    }
    puts("All one-to-six-digit fields preserve exact glyphs, groups and leading blanks.");
}
