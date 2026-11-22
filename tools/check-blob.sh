#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
upstream=${1:-../spd-upstream}
scratch=$(mktemp -d "${TMPDIR:-/tmp}/opencode/spd-blob.XXXXXX")
trap 'find "$scratch" -depth -delete' EXIT
git -C "$upstream" show 57a4e06a4caf162446d1c28caa7983f0493fecf0:core/src/main/java/com/shatteredpixel/shatteredpixeldungeon/actors/blobs/Blob.java > "$scratch/Blob.java"
python3 - "$scratch" <<'PY'
from pathlib import Path
import sys
p=Path(sys.argv[1]); text=(p/'Blob.java').read_text()
start=text.index('\tprotected void evolve() {')
end=text.index('\n\tpublic void seed(',start)
method=text[start:end]
(p/'Oracle.java').write_text('''
public class Oracle {
 static int seed=42;
 static int draw(int n){seed^=seed<<13;seed^=seed>>>17;seed^=seed<<5;return Integer.remainderUnsigned(seed,n);}
 static class Rect{int left,top,right,bottom;}
 static class Level{
  boolean[] solid=new boolean[323];
  int width(){return 19;}
  boolean insideMap(int c){return c>=0 && c<323 && c%19>0 && c%19<18 && c/19>0 && c/19<16;}
 }
 static class Dungeon{static Level level=new Level();}
 static class Gas{
  int[] cur=new int[323],off=new int[323];int volume;Rect area=new Rect();
  void seed(int c,int n){
   cur[c]+=n;volume+=n;int x=c%19,y=c/19;
   if(area.right<=area.left||area.bottom<=area.top){area.left=x;area.right=x+1;area.top=y;area.bottom=y+1;}
   else {area.left=Math.min(area.left,x);area.right=Math.max(area.right,x+1);area.top=Math.min(area.top,y);area.bottom=Math.max(area.bottom,y+1);}
  }
''' + method + '''
  void act(){
   if(volume>0){volume=0;evolve();int[] tmp=cur;cur=off;off=tmp;}
   else if(area.right>area.left&&area.bottom>area.top){area=new Rect();System.arraycopy(cur,0,off,0,cur.length);}
  }
 }
 public static void main(String[] args){
  for(int trial=0;trial<500;trial++){
   Gas gas=new Gas();
   for(int c=0;c<323;c++){boolean b=draw(100)<30;Dungeon.level.solid[c]=c%19==0||c%19==18||c/19==0||c/19==16||b;}
   for(int n=0;n<5;n++){int c=(1+draw(15))*19+1+draw(17);Dungeon.level.solid[c]=false;gas.seed(c,300+draw(1000));}
   for(int turn=0;turn<35;turn++){
    gas.act();System.out.print(gas.volume);
    for(int v:gas.cur)System.out.print(" "+v);System.out.println();
   }
  }
 }
}
''')
PY
javac -d "$scratch" "$scratch/Oracle.java"
java -cp "$scratch" Oracle > "$scratch/java.txt"
${CXX:-c++} -std=c++17 -O2 -Isource -x c++ -o "$scratch/native" - <<'CPP'
#include "blob_rules.h"
#include <cstdint>
#include <cstdio>
uint32_t seed=42;
int draw(int n){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed%n;}
int main(){
 for(int trial=0;trial<500;++trial){
  blob_rules::Gas<19,17> gas;
  bool solid[323];
  for(int c=0;c<323;++c){bool b=draw(100)<30;solid[c]=c%19==0||c%19==18||c/19==0||c/19==16||b;}
  for(int n=0;n<5;++n){int c=(1+draw(15))*19;c+=1+draw(17);solid[c]=false;gas.seed(c,300+draw(1000));}
  for(int turn=0;turn<35;++turn){
   gas.act([](int x,int y){return x>0&&x<18&&y>0&&y<16;},[&](int c){return solid[c];},[](int){});
   std::printf("%d",gas.volume);for(int v:gas.cur)std::printf(" %d",v);std::puts("");
  }
 }
}
CPP
"$scratch/native" > "$scratch/native.txt"
diff -u "$scratch/java.txt" "$scratch/native.txt"
printf '17,500 Blob diffusion steps and volumes match pinned v4.0.2\n'
