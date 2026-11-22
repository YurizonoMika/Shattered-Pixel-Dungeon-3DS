#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
upstream=${1:-../spd-upstream}
scratch=$(mktemp -d "${TMPDIR:-/tmp}/opencode/spd-ballistica.XXXXXX")
trap 'find "$scratch" -depth -delete' EXIT
pkg=com/shatteredpixel/shatteredpixeldungeon
mkdir -p "$scratch/java/$pkg/mechanics" "$scratch/java/$pkg/actors"
git -C "$upstream" show 57a4e06a4caf162446d1c28caa7983f0493fecf0:core/src/main/java/$pkg/mechanics/Ballistica.java > "$scratch/java/$pkg/mechanics/Ballistica.java"
cat > "$scratch/java/$pkg/Dungeon.java" <<'JAVA'
package com.shatteredpixel.shatteredpixeldungeon;
public class Dungeon {
 public static Level level=new Level();
 public static class Level {
  public boolean[] passable=new boolean[323],avoid=new boolean[323],solid=new boolean[323];
  public int width(){return 19;}
  public boolean insideMap(int c){return c>=0 && c<323 && c%19>0 && c%19<18 && c/19>0 && c/19<16;}
 }
}
JAVA
cat > "$scratch/java/$pkg/actors/Actor.java" <<'JAVA'
package com.shatteredpixel.shatteredpixeldungeon.actors;
public class Actor {
 public static boolean[] chars=new boolean[323];
 public static Object findChar(int cell){return chars[cell]?Boolean.TRUE:null;}
}
JAVA
cat > "$scratch/java/$pkg/ShatteredPixelDungeon.java" <<'JAVA'
package com.shatteredpixel.shatteredpixeldungeon;
public class ShatteredPixelDungeon {public static void reportException(Exception e){throw new RuntimeException(e);}}
JAVA
cat > "$scratch/java/Oracle.java" <<'JAVA'
import com.shatteredpixel.shatteredpixeldungeon.Dungeon;
import com.shatteredpixel.shatteredpixeldungeon.actors.Actor;
import com.shatteredpixel.shatteredpixeldungeon.mechanics.Ballistica;
public class Oracle {
 static int seed=42;
 static int draw(int n){seed^=seed<<13;seed^=seed>>>17;seed^=seed<<5;return Integer.remainderUnsigned(seed,n);}
 public static void main(String[] args){
  for(int trial=0;trial<10000;trial++){
   for(int c=0;c<323;c++){
    int type=draw(6);
    Dungeon.level.passable[c]=type>=2;
    Dungeon.level.avoid[c]=type==1;
    Dungeon.level.solid[c]=type==0||type==2;
    Actor.chars[c]=draw(20)==0;
   }
   int a=(1+draw(15))*19+1+draw(17),b=(1+draw(15))*19+1+draw(17);
   for(int flags:new int[]{7,6,4,12}){
    Ballistica ray=new Ballistica(a,b,flags);
    System.out.print(ray.collisionPos+" "+ray.dist);
    for(int c:ray.path)System.out.print(" "+c);
    System.out.println();
   }
  }
 }
}
JAVA
find "$scratch/java" -name '*.java' > "$scratch/sources.txt"
javac -d "$scratch/classes" @"$scratch/sources.txt"
java -cp "$scratch/classes" Oracle > "$scratch/java.txt"
${CXX:-c++} -std=c++17 -O2 -Isource -x c++ -o "$scratch/native" - <<'CPP'
#include "ballistica.h"
#include <cstdint>
#include <cstdio>
uint32_t seed=42;
int draw(int n){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed%n;}
int main(){
 for(int trial=0;trial<10000;++trial){
  bool pass[323],avoid[323],solid[323],chars[323];
  for(int c=0;c<323;++c){
   int type=draw(6);pass[c]=type>=2;avoid[c]=type==1;solid[c]=type==0||type==2;chars[c]=draw(20)==0;
  }
  int a=(1+draw(15))*19; a+=1+draw(17);
  int b=(1+draw(15))*19; b+=1+draw(17);
  for(int flags:{7,6,4,12}){
   auto ray=ballistica::cast(a,b,19,flags,
    [](int c){return c>=0 && c<323 && c%19>0 && c%19<18 && c/19>0 && c/19<16;},
    [&](int c){return pass[c];},[&](int c){return avoid[c];},[&](int c){return solid[c];},[&](int c){return chars[c];});
   std::printf("%d %d",ray.collision,ray.distance);
   for(int c:ray.path)std::printf(" %d",c);
   std::puts("");
  }
 }
}
CPP
"$scratch/native" > "$scratch/native.txt"
diff -u "$scratch/java.txt" "$scratch/native.txt"
printf '40,000 Ballistica paths, collisions and distances match pinned v4.0.2\n'
