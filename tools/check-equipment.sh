#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
upstream=${1:-../spd-upstream}
scratch=$(mktemp -d "${TMPDIR:-/tmp}/opencode/spd-equipment.XXXXXX")
trap 'find "$scratch" -depth -delete' EXIT
python3 - "$upstream" "$scratch" <<'PY'
from pathlib import Path
import subprocess,re,sys
upstream,scratch=sys.argv[1],Path(sys.argv[2])
root='core/src/main/java/com/shatteredpixel/shatteredpixeldungeon/items/weapon/'
mapping=[('WornShortsword','WORN_SHORTSWORD'),('Dagger','DAGGER'),('Gloves','GLOVES'),('Rapier','RAPIER'),('Cudgel','CUDGEL'),
 ('Shortsword','SHORTSWORD'),('HandAxe','HAND_AXE'),('Spear','SPEAR'),('Quarterstaff','QUARTERSTAFF'),('Dirk','DIRK'),('Sickle','SICKLE'),
 ('Sword','SWORD'),('Mace','MACE'),('Scimitar','SCIMITAR'),('RoundShield','ROUND_SHIELD'),('Sai','SAI'),('Whip','WHIP'),
 ('Longsword','LONGSWORD'),('BattleAxe','BATTLEAXE'),('Flail','FLAIL'),('RunicBlade','RUNEBLADE'),('AssassinsBlade','ASSASSINS_BLADE'),('Crossbow','CROSSBOW'),('Katana','KATANA'),
 ('Greatsword','GREATSWORD'),('WarHammer','WARHAMMER'),('Glaive','HALBERD'),('Greataxe','GREATAXE'),('Greatshield','GREATSHIELD'),('Gauntlet','GAUNTLET'),('WarScythe','WAR_SCYTHE')]
def source(path):return subprocess.check_output(['git','-C',upstream,'show','57a4e06a4caf162446d1c28caa7983f0493fecf0:'+root+path],text=True)
def method(text,name,arg):
 m=re.search(r'(?:public|protected)(?: static)? int '+name+r'\('+arg+r'\)\s*\{',text)
 if not m:return ''
 pos=text.index('{',m.start());depth=1;end=pos+1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[m.start():end]
base=source('melee/MeleeWeapon.java');weapon=source('Weapon.java')
base_methods='\n'.join([method(base,'min','int lvl'),method(base,'max','int lvl'),method(base,'STRReq','int lvl'),method(weapon,'STRReq','int tier, int lvl')])
(scratch/'Base.java').write_text('class Base { int tier;float ACC=1f,DLY=1f;int RCH=1; boolean masteryPotionBonus=false;\n'+base_methods+'\n}')
for name,_ in mapping:
 text=source('melee/'+name+'.java')
 fields=''.join(m.group(0) for m in re.finditer(r'\b(?:tier|ACC|DLY|RCH)\s*=\s*[0-9.]+f?;',text))
 methods='\n'.join(method(text,n,'int lvl') for n in ['min','max','STRReq'])
 (scratch/(name+'.java')).write_text('class '+name+' extends Base { { '+fields+' }\n'+methods+'\n}')
classes=','.join('new '+name+'()' for name,_ in mapping)
(scratch/'Oracle.java').write_text('''public class Oracle {
 public static void main(String[] args){
 Base[] all={'''+classes+'''};
 for(Base w:all)for(int level=0;level<=30;level++)
  System.out.printf(java.util.Locale.ROOT,"%d %d %d %d %.5f %.5f %d%n",w.tier,w.min(level),w.max(level),w.STRReq(level),w.ACC,w.DLY,w.RCH);
 }}''')
ids=','.join('IT_'+item for _,item in mapping)
(scratch/'native.cpp').write_text('''#include "hero_rules.h"
#include <cstdio>
int main(){const unsigned char all[]={'''+ids+'''};
for(auto id:all)for(int level=0;level<=30;++level){
 auto range=hero_rules::weapon_range(id,level,30,false);
 std::printf("%d %d %d %d %.5f %.5f %d\\n",hero_rules::equipment_tier(id),range.min,range.max,
 hero_rules::strength_requirement(id,level),hero_rules::weapon_accuracy(id),hero_rules::attack_delay(id,level,30),hero_rules::weapon_reach(id));
}}''')
# Compile the original armor methods and augmentation enum, with only the
# challenge switch stubbed off. This includes the evasion-cloth DR fallback.
armor=subprocess.check_output(['git','-C',upstream,'show','57a4e06a4caf162446d1c28caa7983f0493fecf0:core/src/main/java/com/shatteredpixel/shatteredpixeldungeon/items/armor/Armor.java'],text=True)
enum_start=armor.index('public enum Augment')
enum_end=armor.index('public Augment augment',enum_start)
(scratch/'ArmorOracle.java').write_text('''class Challenges {static int NO_ARMOR=1;}
class Dungeon {static boolean isChallenged(int c){return false;}}
public class ArmorOracle {
 int tier; Augment augment;
'''+armor[enum_start:enum_end]+method(armor,'DRMax','int lvl')+method(armor,'DRMin','int lvl')+'''
 public static void main(String[] args){
 ArmorOracle a=new ArmorOracle();
 for(a.tier=1;a.tier<=5;a.tier++)for(int level=0;level<=30;level++)
 for(Augment aug:new Augment[]{Augment.NONE,Augment.EVASION,Augment.DEFENSE}){
 a.augment=aug;System.out.println(a.DRMin(level)+" "+a.DRMax(level)+" "+aug.evasionFactor(level));
 }}}
''')
(scratch/'armor.cpp').write_text('''#include "hero_rules.h"
#include <cstdio>
int main(){for(int tier=1;tier<=5;++tier)for(int level=0;level<=30;++level)for(int aug=0;aug<3;++aug){
 auto range=hero_rules::armor_range(hero_rules::armor_for_tier(tier),level,aug);
 std::printf("%d %d %d\\n",range.min,range.max,(2+level)*(aug==1?2:aug==2?-2:0));
}}''')
PY
javac -d "$scratch/classes" "$scratch"/*.java
java -cp "$scratch/classes" Oracle > "$scratch/java.txt"
${CXX:-c++} -std=c++17 -O2 -Isource "$scratch/native.cpp" -o "$scratch/native"
"$scratch/native" > "$scratch/native.txt"
diff -u "$scratch/java.txt" "$scratch/native.txt"
printf '961 equipment cases match pinned v4.0.2 (31 weapons, +0..30, tier/damage/STR/accuracy/delay/reach)\n'
java -cp "$scratch/classes" ArmorOracle > "$scratch/java-armor.txt"
${CXX:-c++} -std=c++17 -O2 -Isource "$scratch/armor.cpp" -o "$scratch/armor"
"$scratch/armor" > "$scratch/native-armor.txt"
diff -u "$scratch/java-armor.txt" "$scratch/native-armor.txt"
printf '465 armor augmentation cases match pinned v4.0.2 (five tiers, +0..30, DR min/max and evasion)\n'
