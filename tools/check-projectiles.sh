#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
upstream=${1:-../spd-upstream}
scratch=$(mktemp -d "${TMPDIR:-/tmp}/opencode/spd-projectiles.XXXXXX")
trap 'find "$scratch" -depth -delete' EXIT
python3 - "$upstream" "$scratch" <<'PY'
from pathlib import Path
import re,subprocess,sys
upstream,scratch=sys.argv[1],Path(sys.argv[2])
root='core/src/main/java/com/shatteredpixel/shatteredpixeldungeon/items/weapon/'
def source(path):
 return subprocess.check_output(['git','-C',upstream,'show','57a4e06a4caf162446d1c28caa7983f0493fecf0:'+root+path],text=True)
def method(text,name,args):
 m=re.search(r'(?:public|protected)(?: static)?(?: final)? (?:int|float) '+name+r'\(\s*'+re.escape(args)+r'\s*\)\s*\{',text)
 if not m:return ''
 pos=text.index('{',m.start());end=pos+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[m.start():end]
base=source('missiles/MissileWeapon.java')
methods='\n'.join(method(base,n,'int lvl') for n in ['min','max','STRReq'])
methods+='\n'+method(source('Weapon.java'),'STRReq','int tier, int lvl')
methods+='\n'+method(base,'durabilityPerUse','int level')
stubs='''class Hero {boolean hasTalent(int t){return false;}int pointsInTalent(int t){return 0;}}
class Dungeon {static Hero hero=new Hero();}
class Talent {static int DURABLE_PROJECTILES=0;}
class MagicalHolster {static float HOLSTER_DURABILITY_FACTOR=1.5f;}
class RingOfSharpshooting {static int bonus=0; static float durabilityMultiplier(Hero h){return (float)Math.pow(1.2,bonus);}}
class Crystal {}
'''
(scratch/'Base.java').write_text(stubs+'''class Base {
 int tier;float baseUses=8;boolean masteryPotionBonus=false,holster=false,useRoundingInDurabilityCalc=true;
 Object enchantment; static final float MAX_DURABILITY=100;
 static class Augment {float factor=1;float delayFactor(float f){return f*factor;}}
 Augment augment=new Augment();
'''+methods+'\n}\n')
mapping=[('ThrowingSpike','THROWING_SPIKE'),('ThrowingKnife','KNIFE'),('ThrowingStone','THROWING_STONE'),
 ('FishingSpear','FISHING_SPEAR'),('Shuriken','SHURIKEN'),('ThrowingClub','THROWING_CLUB'),
 ('ThrowingSpear','THROWING_SPEAR'),('Bolas','BOLAS'),('Kunai','KUNAI'),('Javelin','JAVELIN'),
 ('Tomahawk','TOMAHAWK'),('HeavyBoomerang','BOOMERANG'),('Trident','TRIDENT'),('ThrowingHammer','THROWING_HAMMER'),('ForceCube','FORCE_CUBE')]
for name,_ in mapping:
 text=source('missiles/'+name+'.java')
 fields=''.join(m.group(0) for m in re.finditer(r'\b(?:tier|baseUses)\s*=\s*[0-9.]+f?;',text))
 methods='\n'.join(method(text,n,'int lvl') for n in ['min','max','STRReq'])
 (scratch/(name+'.java')).write_text('class '+name+' extends Base {{'+fields+'}\n'+methods+'}\n')
classes=','.join('new '+name+'()' for name,_ in mapping)
(scratch/'Oracle.java').write_text('''public class Oracle {public static void main(String[] args){
 Base[] all={'''+classes+'''};
 for(Base b:all)for(int level=0;level<=30;level++)for(int ring=-2;ring<=8;ring++)for(int aug=0;aug<3;aug++)for(int crystal=0;crystal<2;crystal++){
  RingOfSharpshooting.bonus=ring;b.augment.factor=aug==1?2f/3f:aug==2?5f/3f:1f;b.enchantment=crystal==1?new Crystal():null;
  System.out.printf(java.util.Locale.ROOT,"%d %d %d %.5f%n",b.min(level+ring),b.max(level+ring),b.STRReq(level),b.durabilityPerUse(level));
 }}}
''')
ids=','.join('IT_'+id for _,id in mapping)
(scratch/'native.cpp').write_text('''#include "projectile_rules.h"
#include <cstdio>
int main(){const uint8_t ids[]={'''+ids+'''};
for(auto id:ids)for(int level=0;level<=30;++level)for(int ring=-2;ring<=8;++ring)for(int aug=0;aug<3;++aug)for(int crystal=0;crystal<2;++crystal){
auto d=projectile_rules::damage(id,level+ring);std::printf("%d %d %d %.5f\\n",d.min,d.max,projectile_rules::strength(id,level),
projectile_rules::durability_cost(id,level,ring,aug,crystal?magic_rules::Crystal:0));}}''')
PY
javac -d "$scratch/classes" "$scratch"/*.java
java -cp "$scratch/classes" Oracle > "$scratch/java.txt"
${CXX:-c++} -std=c++17 -O2 -Isource "$scratch/native.cpp" -o "$scratch/native"
"$scratch/native" > "$scratch/native.txt"
python3 - "$scratch" <<'PY'
from pathlib import Path
import sys
root=Path(sys.argv[1]);a=(root/'java.txt').read_text().splitlines();b=(root/'native.txt').read_text().splitlines()
assert len(a)==len(b)==30690
for i,(java,native) in enumerate(zip(a,b)):
 j=java.split();n=native.split()
 # Public min()/max() clamp negative base method results to zero.
 assert [max(0,int(j[0])),max(0,int(j[1])),int(j[2])]==list(map(int,n[:3])),(i,java,native)
 assert abs(float(j[3])-float(n[3]))<.0001,(i,java,native)
print('30,690 projectile cases match v4.0.2 Java: damage, STR and durability across all 15 missiles, levels, rings, augmentation and Crystal.')
PY
