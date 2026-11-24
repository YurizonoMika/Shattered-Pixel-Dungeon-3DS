#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
upstream=${1:-../spd-upstream}
scratch=$(mktemp -d "${TMPDIR:-/tmp}/opencode/spd-bubble.XXXXXX")
trap 'find "$scratch" -depth -delete' EXIT
python3 - "$upstream" "$scratch" <<'PY'
from pathlib import Path
import re,subprocess,sys
upstream,scratch=sys.argv[1],Path(sys.argv[2])
text=subprocess.check_output(['git','-C',upstream,'show','57a4e06a4caf162446d1c28caa7983f0493fecf0:core/src/main/java/com/shatteredpixel/shatteredpixeldungeon/plants/Swiftthistle.java'],text=True)
def method(name,args):
 m=re.search(r'public void '+name+r'\('+args+r'\)\s*\{',text)
 start=text.index('{',m.start());end=start+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[m.start():end]
(scratch/'Oracle.java').write_text('''public class Oracle {
 float left;boolean detached;void detach(){detached=true;}
'''+method('reset','int turns')+method('processTime','float time')+'''
 public static void main(String[] args){
 for(int turns=0;turns<=25;turns++)for(float step:new float[]{.0005f,.001f,.1f,.33333334f,.5f,1,1.2f,3,30}){
  Oracle o=new Oracle();o.reset(turns);
  for(int i=0;i<400;i++){
   if(!o.detached)o.processTime(step);
   System.out.printf(java.util.Locale.ROOT,"%.6f %d%n",o.left,o.detached?1:0);
  }
 }
 }
}''')
PY
javac -d "$scratch/classes" "$scratch/Oracle.java"
java -cp "$scratch/classes" Oracle > "$scratch/java.txt"
${CXX:-c++} -std=c++17 -O2 -Isource -x c++ -o "$scratch/native" - <<'CPP'
#include "time_bubble_rules.h"
#include <cstdio>
#include <initializer_list>
int main(){for(int turns=0;turns<=25;++turns)for(float step:{.0005f,.001f,.1f,.33333334f,.5f,1.f,1.2f,3.f,30.f}){
 float left=time_bubble_rules::reset(turns);bool detached=false;
 for(int i=0;i<400;++i){if(!detached)detached=time_bubble_rules::process(left,step);std::printf("%.6f %d\n",left,detached);}
}}
CPP
"$scratch/native" > "$scratch/native.txt"
diff -u "$scratch/java.txt" "$scratch/native.txt"
printf '93,600 TimeBubble states match pinned Java reset/processTime across fractional costs and expiry tolerance.\n'
