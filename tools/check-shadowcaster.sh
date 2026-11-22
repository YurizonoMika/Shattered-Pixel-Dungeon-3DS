#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
upstream=${1:-../spd-upstream}
scratch=$(mktemp -d "${TMPDIR:-/tmp}/opencode/spd-fov.XXXXXX")
trap 'find "$scratch" -depth -delete' EXIT
pkg=com/shatteredpixel/shatteredpixeldungeon
mkdir -p "$scratch/java/$pkg/mechanics" "$scratch/java/com/watabou/utils"
git -C "$upstream" show 57a4e06a4caf162446d1c28caa7983f0493fecf0:core/src/main/java/$pkg/mechanics/ShadowCaster.java > "$scratch/java/$pkg/mechanics/ShadowCaster.java"
cat > "$scratch/java/$pkg/ShatteredPixelDungeon.java" <<'JAVA'
package com.shatteredpixel.shatteredpixeldungeon;
public class ShatteredPixelDungeon {public static void reportException(Exception e){throw new RuntimeException(e);}}
JAVA
cat > "$scratch/java/com/watabou/utils/BArray.java" <<'JAVA'
package com.watabou.utils;
public class BArray {public static void setFalse(boolean[] a){java.util.Arrays.fill(a,false);}}
JAVA
cat > "$scratch/java/Oracle.java" <<'JAVA'
import com.shatteredpixel.shatteredpixeldungeon.mechanics.ShadowCaster;
public class Oracle {
 static int seed=42;
 static int draw(int n){seed^=seed<<13;seed^=seed>>>17;seed^=seed<<5;return Integer.remainderUnsigned(seed,n);}
 public static void main(String[] args){
  for(int trial=0;trial<10000;trial++){
   boolean[] blocking=new boolean[41*41],fov=new boolean[41*41];
   int density=draw(85);
   for(int c=0;c<blocking.length;c++){
    boolean blocked=draw(100)<density;
    blocking[c]=c%41==0||c%41==40||c/41==0||c/41==40||blocked;
   }
   int x=1+draw(39),y=1+draw(39),distance=1+draw(20);
   ShadowCaster.castShadow(x,y,41,fov,blocking,distance);
   for(boolean v:fov)System.out.print(v?'1':'0');
   System.out.println();
  }
 }
}
JAVA
find "$scratch/java" -name '*.java' > "$scratch/sources.txt"
javac -d "$scratch/classes" @"$scratch/sources.txt"
java -cp "$scratch/classes" Oracle > "$scratch/java.txt"
${CXX:-c++} -std=c++17 -O2 -Isource -x c++ -o "$scratch/native" - <<'CPP'
#include "shadowcaster.h"
#include <cstdint>
#include <cstdio>
uint32_t seed=42;
int draw(int n){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed%n;}
int main(){
 for(int trial=0;trial<10000;++trial){
  bool blocking[41*41],fov[41*41]{};
  int density=draw(85);
  for(int c=0;c<41*41;++c){bool blocked=draw(100)<density;blocking[c]=c%41==0||c%41==40||c/41==0||c/41==40||blocked;}
  int x=1+draw(39),y=1+draw(39),distance=1+draw(20);
  shadowcaster::cast(x,y,distance,
   [&](int cx,int cy){return cx<0||cy<0||cx>=41||cy>=41||blocking[cy*41+cx];},
   [&](int cx,int cy){if(cx>=0&&cy>=0&&cx<41&&cy<41)fov[cy*41+cx]=true;});
  for(bool v:fov)std::putchar(v?'1':'0');
  std::puts("");
 }
}
CPP
"$scratch/native" > "$scratch/native.txt"
diff -u "$scratch/java.txt" "$scratch/native.txt"
printf '10,000 ShadowCaster masks match pinned v4.0.2 (radii 1..20, borders and obstacles)\n'
