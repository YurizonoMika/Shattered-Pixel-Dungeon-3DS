#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
upstream=${1:-../spd-upstream}
scratch=$(mktemp -d "${TMPDIR:-/tmp}/opencode/spd-plants.XXXXXX")
trap 'find "$scratch" -depth -delete' EXIT
python3 - "$upstream" "$scratch" <<'PY'
from pathlib import Path
import re,subprocess,sys
upstream,scratch=sys.argv[1],Path(sys.argv[2])
base='core/src/main/java/com/shatteredpixel/shatteredpixeldungeon/'
def source(path):
 return subprocess.check_output(['git','-C',upstream,'show','57a4e06a4caf162446d1c28caa7983f0493fecf0:'+base+path],text=True)
def method(text,name):
 m=re.search(r'(?:public|private)(?: static)? (?:int|void|boolean) '+name+r'\(\s*[^)]*\)\s*\{',text)
 start=text.index('{',m.start());end=start+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[m.start():end]
earth=source('plants/Earthroot.java');sun=source('plants/Sungrass.java')
cure=method(source('items/potions/PotionOfHealing.java'),'cure')
names=re.findall(r'Buff.detach\(\s*ch,\s*(\w+).class\s*\)',cure)
assert len(names)==9
constants={name:re.search(r'DURATION\s*=\s*([0-9.]+)f',source('actors/buffs/'+name+'.java')).group(1) for name in ['Bless','Blindness','Cripple']}
(scratch/'Oracle.java').write_text('''class Dungeon {static int depth;static int scalingDepth(){return depth;}}
class Char {int pos,HT=20,HP=1;Sprite sprite=new Sprite();}
class Hero extends Char {boolean resting;}
class Sprite {void showStatusWithIcon(int a,String b,int c){}}
class CharSprite {static int POSITIVE=0;}class FloatingText {static int HEALING=0;}
class Buff {boolean detached;void detach(){detached=true;}void spend(float f){}static void detach(Char c,Class<?> type){Oracle.cured.add(type.getSimpleName());}}
'''+''.join('class '+n+' {}\n' for n in names)+'''
class Armor extends Buff {int pos,level;Char target=new Char();
'''+method(earth,'blocking')+method(earth,'absorb')+'''}
class Health extends Buff {int pos,level;float partialHeal;Char target=new Char();static float STEP=1;
'''+method(sun,'act')+'''}
public class Oracle {
 static java.util.Set<String> cured=new java.util.TreeSet<>();
'''+cure+'''
 public static void main(String[] args){
 for(int depth=1;depth<=26;depth++)for(int pool=1;pool<=25;pool++)for(int damage=0;damage<=40;damage++){
  Dungeon.depth=depth;Armor a=new Armor();a.level=pool;
  int left=a.absorb(damage);System.out.println(left+" "+(a.detached?0:a.level));
 }
 for(int maximum:new int[]{20,65,115,165,1000})for(int pool:new int[]{1,2,5,20,1000})for(float partial:new float[]{0,.5f,.9f,1f}){
  Health h=new Health();h.target.HT=maximum;h.level=pool;h.partialHeal=partial;
  for(int turn=0;turn<180;turn++){
   int before=h.target.HP;
   if(!h.detached)h.act();
   System.out.printf(java.util.Locale.ROOT,"%d %d %.6f%n",h.target.HP-before,Math.max(0,h.level),h.partialHeal);
  }
 }
 cure(new Char());System.out.println(String.join(" ",cured));
 System.out.println("'''+ ' '.join(f'{float(constants[n]):.1f}' for n in ['Bless','Blindness','Cripple'])+'''");
 }
}
''')
(scratch/'native.cpp').write_text('''#include "seed_rules.h"
#include <cstdio>
int main(){
for(int depth=1;depth<=26;++depth)for(int pool=1;pool<=25;++pool)for(int damage=0;damage<=40;++damage){
 float remaining=pool;int left=seed_rules::absorb(remaining,damage,depth);std::printf("%d %d\\n",left,int(remaining));
}
for(int maximum:{20,65,115,165,1000})for(int pool:{1,2,5,20,1000})for(float initial:{0.f,.5f,.9f,1.f}){
 float remaining=pool,partial=initial;int hp=1;
 for(int turn=0;turn<180;++turn){int before=hp;hp=std::min(maximum,hp+seed_rules::heal_tick(remaining,partial,maximum));
 std::printf("%d %d %.6f\\n",hp-before,int(remaining),partial);}
}
magic_rules::Effects s;s.poison=s.bleeding=s.blind=1;
mechanics_rules::Actor e;e.cripple=e.weakness=e.vulnerable=e.drowsy=e.slow=e.vertigo=1;
seed_rules::cure(s,e);
if(!s.bleeding)std::printf("Bleeding ");if(!s.blind)std::printf("Blindness ");if(!e.cripple)std::printf("Cripple ");
if(!e.drowsy)std::printf("Drowsy ");if(!s.poison)std::printf("Poison ");if(!e.slow)std::printf("Slow ");
if(!e.vertigo)std::printf("Vertigo ");if(!e.vulnerable)std::printf("Vulnerable ");if(!e.weakness)std::printf("Weakness");std::puts("");
std::printf("%.1f %.1f %.1f\\n",seed_rules::blessing_duration,seed_rules::blindness_duration,seed_rules::cripple_duration);
}
''')
PY
javac -d "$scratch/classes" "$scratch/Oracle.java"
java -cp "$scratch/classes" Oracle > "$scratch/java.txt"
${CXX:-c++} -std=c++17 -O2 -Isource "$scratch/native.cpp" -o "$scratch/native"
"$scratch/native" > "$scratch/native.txt"
diff -u "$scratch/java.txt" "$scratch/native.txt"
printf '26,650 Earthroot absorptions, 18,000 Sungrass ticks, nine cure entries and three buff durations match pinned v4.0.2 Java.\n'
