#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
upstream=${1:-../spd-upstream}
scratch=$(mktemp -d "${TMPDIR:-/tmp}/opencode/spd-frost.XXXXXX")
trap 'find "$scratch" -depth -delete' EXIT
python3 - "$upstream" "$scratch" <<'PY'
from pathlib import Path
import subprocess,sys
upstream,scratch=sys.argv[1],Path(sys.argv[2])
base='core/src/main/java/com/shatteredpixel/shatteredpixeldungeon/'
def source(path):
 return subprocess.check_output(['git','-C',upstream,'show','57a4e06a4caf162446d1c28caa7983f0493fecf0:'+base+path],text=True)
def method(text,signature):
 start=text.index(signature);end=text.index('{',start)+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
chill=source('actors/buffs/Chill.java')
speed=method(chill,'public float speedFactor()')
(scratch/'Oracle.java').write_text('''public class Oracle {float left;float cooldown(){return left;}
'''+speed+'''
 public static void main(String[] args){Oracle o=new Oracle();
 for(int i=0;i<=10000;i++){o.left=i/100f;System.out.printf(java.util.Locale.ROOT,"%.6f%n",o.speedFactor());}
 }
}''')
# Keep the duration and water-detach claims tied to the pinned implementation.
frost=source('actors/buffs/Frost.java');freezing=source('actors/blobs/Freezing.java')
assert 'DURATION\t= 10f' in frost
assert 'Chill.DURATION/2f' in method(frost,'public void detach()')
assert 'Frost.DURATION * 3' in method(freezing,'public static boolean affect(')
PY
javac -d "$scratch/classes" "$scratch/Oracle.java"
java -cp "$scratch/classes" Oracle > "$scratch/java.txt"
${CXX:-c++} -std=c++17 -O2 -ffp-contract=off -Isource -x c++ -o "$scratch/native" - <<'CPP'
#include "frost_rules.h"
#include <cstdio>
#include <cassert>
int main(){for(int i=0;i<=10000;++i)std::printf("%.6f\n",frost_rules::speed(i/100.f));
assert(frost_rules::duration(false)==10 && frost_rules::duration(true)==30);
float frost=30,chill=2;frost_rules::detach(frost,chill,true);assert(frost==0 && chill==5);
}
CPP
"$scratch/native" > "$scratch/native.txt"
diff -u "$scratch/java.txt" "$scratch/native.txt"
printf '10,001 Chill speed cases match pinned Java; Frost water/duration constants verified.\n'
