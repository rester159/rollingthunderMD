"""Alternative native-friendly byte LZ: cheap 8-bit distances, longer runs.

0..127 = literal length 1..128; 128..255 = match length 3..130.
Match distance is the next byte (1..255), or zero followed by a 16-bit distance.
Each stream is independent, with a maximum 1024-byte backward distance.
"""
from collections import defaultdict,deque
from pose_codec import normalize,mapping

def encode(raw):
 """Minimize bytes, then native decoder work among equally small parses."""
 if not raw:raise ValueError('Empty pose')
 positions=defaultdict(list);matches=[]
 for i in range(len(raw)):
  best=[(0,0)]*5
  if i+3<=len(raw):
   key=raw[i:i+3];limit=min(130,len(raw)-i)
   for at in reversed(positions[key]):
    distance=i-at
    if distance>1024:break
    n=3
    while n<limit and raw[at+n]==raw[i+n]:n+=1
    group=0 if distance<=2 else (1 if distance<=255 else 3)+(distance&1)
    if n>best[group][0]:best[group]=n,distance
    if n==limit and (distance<=2 or (distance<=255 and not distance&1)):break
   positions[key].append(i)
  matches.append(best)
 costs=[(0,0)]*(len(raw)+1);steps=[None]*len(raw);literals=deque()
 for i in range(len(raw)-1,-1,-1):
  j=i+1;key=(j+costs[j][0],32*j+costs[j][1])
  while literals and literals[-1][1]>key:literals.pop()
  literals.append((j,key))
  while literals[0][0]>i+128:literals.popleft()
  end,key=literals[0];cost=(1-i+key[0],54-32*i+key[1]);count=end-i;distance=0
  for longest,back in matches[i]:
   if not back:continue
   price=2 if back<=255 else 4
   for n in range(3,longest+1):
    if n<8:cycles=120+22*n
    elif back<=2:cycles=200+22*(n//4)+22*(n%4)
    elif back&1:cycles=214+12*n+10*((n+7)//8)
    else:
     aligned=n-(i&1)
     cycles=206+(18 if i&1 else 0)+30*(aligned//4)+22*(aligned%4)
    if back>255:cycles+=36
    candidate=(price+costs[i+n][0],cycles+costs[i+n][1])
    if candidate<cost:cost,count,distance=candidate,n,back
  costs[i]=cost;steps[i]=count,distance
 result=bytearray();i=0
 while i<len(raw):
  count,distance=steps[i]
  if distance:
   result.append(128+count-3)
   if distance<=255:result.append(distance)
   else:result.extend((0,distance>>8,distance&255))
  else:result.append(count-1);result+=raw[i:i+count]
  i+=count
 assert len(result)==costs[0][0]
 return bytes(result)

def decode(packed,size,palette=bytes.fromhex('0123456789abcdef')):
 table=mapping(palette);result=bytearray();cursor=0
 while len(result)<size:
  token=packed[cursor];cursor+=1
  if token&128:
   count=(token&127)+3;distance=packed[cursor];cursor+=1
   if not distance:
    distance=int.from_bytes(packed[cursor:cursor+2],'big');cursor+=2
   if not 0<distance<=min(1024,len(result)):raise ValueError('Invalid history distance')
   for _ in range(count):result.append(result[-distance])
  else:
   count=token+1;result+=packed[cursor:cursor+count].translate(table);cursor+=count
 if len(result)!=size or cursor!=len(packed):raise ValueError('Stream length mismatch')
 return bytes(result)
