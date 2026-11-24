#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
upstream=${1:-../spd-upstream}
scratch=$(mktemp -d "${TMPDIR:-/tmp}/opencode/spd-foresight.XXXXXX")
trap 'find "$scratch" -depth -delete' EXIT
python3 - "$upstream" "$scratch" <<'PY'
from pathlib import Path
import subprocess,sys
upstream,scratch=sys.argv[1],Path(sys.argv[2])
commit='57a4e06a4caf162446d1c28caa7983f0493fecf0'
pkg='com/shatteredpixel/shatteredpixeldungeon'
def source(path):
 return subprocess.check_output(['git','-C',upstream,'show',commit+':'+path],text=True)
def write(path,text):
 p=scratch/'java'/path;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
for name in ['Ballistica','ConeAOE']:
 write(pkg+'/mechanics/'+name+'.java',source('core/src/main/java/'+pkg+'/mechanics/'+name+'.java'))
for name in ['Point','PointF']:
 write('com/watabou/utils/'+name+'.java',source('SPD-classes/src/main/java/com/watabou/utils/'+name+'.java'))
write('com/watabou/utils/GameMath.java','''package com.watabou.utils;
public class GameMath {public static float gate(float lo,float v,float hi){return Math.max(lo,Math.min(v,hi));}}
''')
write(pkg+'/Dungeon.java','''package com.shatteredpixel.shatteredpixeldungeon;
import com.watabou.utils.Point;
public class Dungeon {
 public static Hero hero=new Hero();public static Level level=new Level();
 public static class Hero {public int pos;}
 public static class Level {
  public int w,h;public boolean[] passable,avoid,solid;
  public int width(){return w;}public int height(){return h;}
  public boolean insideMap(int c){return c>=w && c<w*(h-1) && c%w>0 && c%w<w-1;}
  public Point cellToPoint(int c){return new Point(c%w,c/w);}
  public int pointToCell(Point p){return p.x+p.y*w;}
  public boolean adjacent(int a,int b){return Math.max(Math.abs(a%w-b%w),Math.abs(a/w-b/w))==1;}
  public float trueDistance(int a,int b){return (float)Math.sqrt(Math.pow(Math.abs(a%w-b%w),2)+Math.pow(Math.abs(a/w-b/w),2));}
 }
}
''')
write(pkg+'/actors/Actor.java','''package com.shatteredpixel.shatteredpixeldungeon.actors;
public class Actor {public static Object findChar(int c){return null;}}
''')
write(pkg+'/ShatteredPixelDungeon.java','''package com.shatteredpixel.shatteredpixeldungeon;
public class ShatteredPixelDungeon {public static void reportException(Exception e){throw new RuntimeException(e);}}
''')
talisman=source('core/src/main/java/'+pkg+'/items/artifacts/TalismanOfForesight.java')
maxdist=talisman[talisman.index('private float maxDist()'):talisman.index('public CellSelector.Listener scry')]
scan=talisman[talisman.index('//enforces at least 2 tiles'):talisman.index('int earnedExp = 0;')]
spend=talisman[talisman.index('charge -= 3 + dist*1.08f;'):talisman.index('Invisibility.dispel(curUser);')]
write('Oracle.java','''import com.shatteredpixel.shatteredpixeldungeon.Dungeon;
import com.shatteredpixel.shatteredpixeldungeon.mechanics.*;
public class Oracle {
 static int seed=42;int charge,lvl;float partialCharge;
 Dungeon.Hero curUser=Dungeon.hero;
 int level(){return lvl;}
 static int draw(int n){seed^=seed<<13;seed^=seed>>>17;seed^=seed<<5;return Integer.remainderUnsigned(seed,n);}
'''+maxdist+'''
 void run(Integer target){
'''+scan+spend+'''
 System.out.printf(java.util.Locale.ROOT,"%d %.6f %d %.6f",target,dist,charge,partialCharge);
 for(int c:new java.util.TreeSet<Integer>(cone.cells))System.out.print(" "+c);
 System.out.println();
 }
 public static void main(String[] args){
  Oracle o=new Oracle();
  for(int i=0;i<12000;i++){
   int w=5+draw(60),h=5+draw(60);Dungeon.level.w=w;Dungeon.level.h=h;
   Dungeon.level.passable=new boolean[w*h];Dungeon.level.avoid=new boolean[w*h];Dungeon.level.solid=new boolean[w*h];
   o.curUser.pos=(1+draw(h-2))*w+1+draw(w-2);
   int target=draw(h)*w+draw(w);if(target==o.curUser.pos)target++;
   o.lvl=draw(11);o.charge=5+draw(96);o.partialCharge=draw(100)/100f;o.run(target);
  }
  // Minimum charge at each of the eight adjacent directions, including corners.
  Dungeon.level.w=19;Dungeon.level.h=17;
  for(int y:new int[]{1,8,15})for(int x:new int[]{1,9,17})for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
   if(dx==0 && dy==0)continue;
   o.curUser.pos=y*19+x;o.lvl=0;o.charge=5;o.partialCharge=0;o.run((y+dy)*19+x+dx);
  }
 }
}
''')
(scratch/'native.cpp').write_text('''#include "foresight_rules.h"
#include <cstdio>
uint32_t seed=42;
int draw(int n){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed%n;}
void run(int from,int target,int w,int h,int level,int charge,float partial){
 auto scan=foresight_rules::scan(from%w+from/w*64,target%w+target/w*64,64,w,h,level,charge);
 scan.target=scan.target%64+scan.target/64*w;
 for(int &c:scan.cells)c=c%64+c/64*w;
 foresight_rules::spend(charge,partial,scan.distance);
 std::printf("%d %.6f %d %.6f",scan.target,scan.distance,charge,partial);
 for(int c:scan.cells)std::printf(" %d",c);std::puts("");
}
int main(){for(int i=0;i<12000;++i){
 int w=5+draw(60),h=5+draw(60);
 int from=(1+draw(h-2))*w;from+=1+draw(w-2);
 int target=draw(h)*w;target+=draw(w);if(target==from)++target;
 int level=draw(11),charge=5+draw(96);float partial=draw(100)/100.f;
 run(from,target,w,h,level,charge,partial);
 }
 for(int y:{1,8,15})for(int x:{1,9,17})for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx){
 if(dx==0 && dy==0)continue;run(y*19+x,(y+dy)*19+x+dx,19,17,0,5,0);
 }
}
''')
PY
find "$scratch/java" -name '*.java' > "$scratch/sources.txt"
javac -d "$scratch/classes" @"$scratch/sources.txt"
java -cp "$scratch/classes" Oracle > "$scratch/java.txt"
${CXX:-c++} -std=c++17 -O2 -ffp-contract=off -Isource "$scratch/native.cpp" -o "$scratch/native"
"$scratch/native" > "$scratch/native.txt"
python3 - "$scratch" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);a=(root/'java.txt').read_text().splitlines();b=(root/'native.txt').read_text().splitlines()
assert len(a)==len(b)==12072
for i,(java,native) in enumerate(zip(a,b)):
 j=java.split();n=native.split()
 assert j[0]==n[0] and j[2]==n[2] and j[4:]==n[4:],(i,java,native)
 assert abs(float(j[1])-float(n[1]))<.00001 and abs(float(j[3])-float(n[3]))<.00001,(i,java,native)
print('12,072 Foresight cases match pinned Java: adjacent extension, distance clamp, exact cone cells and fractional charge (including edge targets and minimum charge).')
PY
