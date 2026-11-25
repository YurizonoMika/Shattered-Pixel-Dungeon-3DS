#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
upstream=${1:-../spd-upstream}
scratch=$(mktemp -d "${TMPDIR:-/tmp}/opencode/spd-hourglass.XXXXXX")
trap 'find "$scratch" -depth -delete' EXIT
python3 - "$upstream" "$scratch" <<'PY'
from pathlib import Path
import subprocess,sys
upstream,scratch=sys.argv[1],Path(sys.argv[2])
text=subprocess.check_output(['git','-C',upstream,'show','57a4e06a4caf162446d1c28caa7983f0493fecf0:core/src/main/java/com/shatteredpixel/shatteredpixeldungeon/items/artifacts/TimekeepersHourglass.java'],text=True)
stasis=text[text.index('int usedCharge = Math.min(charge, 2);'):text.index('target.invisible++;')]
text=text[text.index('public class timeFreeze'):]
start=text.index('public void processTime(');end=text.index('{',start)+1;depth=1
while depth:
 depth+=(text[end]=='{')-(text[end]=='}');end+=1
method=text[start:end]
(scratch/'Oracle.java').write_text('''class Hunger {int level;boolean isStarving(){return level>=450;}void satisfy(int n){level=Math.max(0,level-n);}}
class Buff {static Hunger hunger;static Hunger affect(Object target,Class<?> type){return hunger;}}
public class Oracle {
 Object target=new Object();int duration;void spend(int n){duration=n;}
 void stasis(){'''+stasis+'''}
 int charge;float turnsToCost=2;boolean detached;
 void detach(){detached=true;}void updateQuickslot(){}
'''+method+'''
 public static void main(String[] args){
 for(int initial=1;initial<=10;initial++)for(float step:new float[]{0,.0005f,.001f,.1f,.33333334f,.5f,1,1.2f,2,3,30}){
  Oracle o=new Oracle();o.charge=initial-1;o.processTime(0);
  for(int i=0;i<400;i++){
   if(!o.detached)o.processTime(step);
   System.out.printf(java.util.Locale.ROOT,"%d %.6f %d%n",o.charge,o.turnsToCost,o.detached?1:0);
  }
 }
 for(int initial=1;initial<=10;initial++)for(int hunger=0;hunger<=450;hunger++){
  Oracle o=new Oracle();o.charge=initial;Buff.hunger=new Hunger();Buff.hunger.level=hunger;o.stasis();
  System.out.printf("%d %d %d%n",o.charge,o.duration,Buff.hunger.level);
 }
 }
}''')
PY
javac -d "$scratch/classes" "$scratch/Oracle.java"
java -cp "$scratch/classes" Oracle > "$scratch/java.txt"
${CXX:-c++} -std=c++17 -O2 -Isource -x c++ -o "$scratch/native" - <<'CPP'
#include "hourglass_rules.h"
#include <cstdio>
#include <initializer_list>
int main(){for(int initial=1;initial<=10;++initial)for(float step:{0.f,.0005f,.001f,.1f,.33333334f,.5f,1.f,1.2f,2.f,3.f,30.f}){
 int charge=initial-1;float left=2;bool detached=hourglass_rules::process(charge,left,0);
 for(int i=0;i<400;++i){if(!detached)detached=hourglass_rules::process(charge,left,step);std::printf("%d %.6f %d\n",charge,left,detached);}
}
for(int initial=1;initial<=10;++initial)for(int hunger=0;hunger<=450;++hunger)
 std::printf("%d %d %d\n",initial-(initial<2?initial:2),hourglass_rules::stasis_duration(initial),hourglass_rules::stasis_hunger(hunger,initial));
}
CPP
"$scratch/native" > "$scratch/native.txt"
diff -u "$scratch/java.txt" "$scratch/native.txt"
printf '44,000 Hourglass charge/turnsToCost/expiry states match pinned Java across 1..10 charges and fractional/oversized actions.\n'
printf '4,510 Stasis charge/duration/hunger cases match extracted pinned Java attachTo logic.\n'
