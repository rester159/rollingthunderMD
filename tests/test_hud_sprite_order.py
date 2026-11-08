#!/usr/bin/env python3
"""Exercise the production HUD SAT linker with 20,000 adversarial actor orders.
Checks every actor, priority/tie order, empty batches, and a single DMA transfer.
"""
import subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
s=(root/'src/sprites.c').read_text();body=s[s.index('u8 RT_spritesQueue('):s.index('\nu8 RT_spriteSubmitPacked(')]
pre=r'''
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
typedef uint8_t u8;typedef uint16_t u16;
typedef struct {u16 y;u8 size,link;u16 attribute,x;} RT_SpriteCommand;
typedef struct {RT_SpriteCommand storage[80],*commands;u16 count,queuedBytes;u8 failed,reserved,orderKeys[80],runCount,runStarts[80],runKeys[80];} RT_SpriteBatch;
static u8 prepareTransfers=1,nextPublication;static unsigned transfers,words;
enum {DMA_QUEUE,DMA_QUEUE_COPY,DMA_VRAM};
static void linkSpriteOrder(RT_SpriteBatch *b){(void)b;assert(0);}
static int prepareTransfer(int m,int l,void *s,int d,int n,int step)
{(void)m;(void)l;(void)s;assert(d==0xf400&&step==2);transfers++;words=n;return 1;}
'''
post=r'''
int main(void){
 unsigned seed=1973;
 for(unsigned trial=0;trial<20000;trial++){
  RT_SpriteCommand sat[80]={0};RT_SpriteBatch b={0};u8 keys[58];
  b.commands=sat+4;b.reserved=22;b.count=trial%59;
  for(unsigned i=0;i<b.count;i++){
   seed=seed*1664525u+1013904223u;keys[i]=trial%5==0?7:(seed>>24)%32;
   b.commands[i]=(RT_SpriteCommand){200,0,i+3,i+1,160};
   if(!i||keys[i]!=keys[i-1]){b.runStarts[b.runCount]=i;b.runKeys[b.runCount++]=keys[i];}
  }
  for(int i=0;i<4;i++)sat[i]=(RT_SpriteCommand){0,0,i==3?62:i+1,0,128};
  transfers=0;assert(RT_spritesQueue(&b));
  for(int i=0;i<4;i++)assert(sat[i].link==(i==3?62:i+1));assert(transfers==1);
  assert(words==(b.count?b.count+4:5)*4);
  unsigned current=4,seen=0;
  if(!b.count){assert(!sat[4].attribute&&!sat[4].link);continue;}
  for(int key=31;key>=0;key--)for(int i=b.count-1;i>=0;i--)if(keys[i]==key){
   assert(current>=4&&current<62);assert(sat[current].attribute==i+1);
   current=sat[current].link;seen++;
  }
  assert(seen==b.count&&!current);
 }
 puts("20,000 SAT batches preserve every actor and priority/tie order with one DMA transfer, including empty batches.");
}
'''
with tempfile.TemporaryDirectory() as t:
 c=Path(t)/'order.c';binary=Path(t)/'order';c.write_text(pre+body+post)
 subprocess.run(['cc','-std=c11','-O2',str(c),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
