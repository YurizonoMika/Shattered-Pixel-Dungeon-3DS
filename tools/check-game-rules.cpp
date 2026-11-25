#include "game.h"
#include "actor_rules.h"
#include "artifact_rules.h"
#include "foresight_rules.h"
#include "projectile_rules.h"
#include "seed_rules.h"
#include "hero_rules.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>

void dlog(const char*, ...) {}

static void arena(Game& game, uint8_t type, int x, int y) {
  game.new_run(0, 0x12345678);
  game.mobs.clear(); game.features.clear(); game.items.clear();
  game.mob_cap = 0; game.inv_n = 0;
  game.spawn_rooms.clear();game.spawn_weights.clear();game.spawn_deck.clear();game.entrance_cell=-1;
  game.map_width = 23; game.map_height = 15;
  memset(game.tiles, T_WALL, sizeof(game.tiles));
  for (int cy = 1; cy < 14; ++cy)
    for (int cx = 1; cx < 22; ++cx) game.tiles[cy][cx] = T_FLOOR;
  game.px = 10; game.py = 7;
  game.hp = game.maxhp = 1000;
  game.hp_boost = 980;
  game.armor = IT_NONE;
  Mob mob;
  mob.type = type; mob.x = x; mob.y = y;
  mob.hp = mob.maxhp = 100;
  mob.ai = Mob::Hunting; mob.target = game.py*MW+game.px;
  game.mobs.push_back(mob);
  game.recompute_fov();
}

static bool same_clock(float a, float b) { return std::fabs(a-b) < .00001f; }

int main() {
  Game game;
  arena(game, M_CRAB, 6, 7);
  game.spend_action(.4f);
  assert(game.mobs[0].x == 7 && same_clock(game.mobs[0].next_action,.5f));
  assert(game.turns == 1 && game.hunger == 1);
  game.spend_action(.1f);
  // At t=.5 hero input wins the tie over the crab's second movement.
  assert(game.mobs[0].x == 7 && same_clock(game.mobs[0].next_action,.5f));
  game.spend_action(.01f);
  assert(game.mobs[0].x == 8);
  assert(game.turns == 1 && game.hunger == 1);

  arena(game, M_RAT, 6, 7);
  game.spend_action(1.f);
  assert(game.mobs[0].x == 7);
  arena(game, M_BAT, 6, 7);
  game.spend_action(1.f);
  assert(game.mobs[0].x == 8);
  arena(game, M_CRAB, 9, 7);
  game.spend_action(1.f);
  // Fast movement does not imply fast melee attacks.
  assert(same_clock(game.mobs[0].next_action,0.f));
  arena(game, M_RIPPER, 9, 7);
  game.spend_action(.75f);
  assert(same_clock(game.mobs[0].next_action,1.f));

  arena(game, M_BAT, 6, 7);
  game.mobs[0].sleep_t = 2;
  game.spend_action(.75f);
  assert(game.mobs[0].sleep_t == 1 && game.mobs[0].x == 6);
  game.spend_action(.75f);
  assert(game.mobs[0].sleep_t == 0 && game.mobs[0].x == 6);
  game.spend_action(.51f);
  assert(game.mobs[0].x == 7);

  arena(game, M_RAT, 9, 7);
  game.hp = 1; game.healing_left = 30;
  game.spend_action(.1f);
  assert(game.hp > 0 && !game.dead && game.healing_left == 22);
  assert(game.mobs[0].combat.age >= 0);

  arena(game, M_CRAB, 6, 7);
  game.spend_action(.3f);
  game.mobs[0].terror_t = 3;
  assert(game.save(106));
  Game restored;
  assert(restored.load(106));
  assert(restored.mobs[0].ai == game.mobs[0].ai && restored.mobs[0].target == game.mobs[0].target);
  assert(same_clock(restored.health_time,game.health_time));
  assert(same_clock(restored.mobs[0].next_action,game.mobs[0].next_action));
  for (float action : {.2f,.8f,1.5f,.4f,1.f}) {
    game.spend_action(action); restored.spend_action(action);
    assert(game.rng == restored.rng && game.hp == restored.hp && game.hunger == restored.hunger);
    assert(game.mobs[0].x == restored.mobs[0].x && game.mobs[0].y == restored.mobs[0].y);
    assert(game.mobs[0].ai == restored.mobs[0].ai && game.mobs[0].target == restored.mobs[0].target);
    assert(same_clock(game.mobs[0].next_action,restored.mobs[0].next_action));
  }

  std::ifstream input("sdmc:/3ds/spd/slot106.bin",std::ios::binary);
  std::string bytes{std::istreambuf_iterator<char>(input),{}};
  assert(bytes.size() > sizeof(GameDisk));
  bytes.pop_back();
  std::ofstream output("sdmc:/3ds/spd/slot107.bin",std::ios::binary);
  output.write(bytes.data(),bytes.size()); output.close();
  assert(!restored.load(107));

  arena(game,M_RAT,3,3); game.mobs.clear();
  game.add_item(IT_LONGSWORD,1,2,IF_CURSED);
  game.equip_item(0);
  assert(game.weapon == IT_LONGSWORD && game.weapon_level == 2);
  assert(game.weapon_flags == (IF_CURSED|IF_CURSE_KNOWN));
  const int equipped_turn = game.turns;
  game.equip_item(0);
  assert(game.weapon == IT_LONGSWORD && game.turns == equipped_turn);
  game.add_item(IT_SCROLL_CURSE);
  game.use_item(game.inv_n-1);
  assert(game.transmute_open && game.can_select_item(-1));
  game.select_item(-1);
  assert(!(game.weapon_flags & IF_CURSED) && game.weapon_level == 2);
  game.equip_item(0);
  assert(game.weapon == IT_DAGGER && (game.inv[0].flags & IF_CURSE_KNOWN));
  assert(!(game.inv[0].flags & IF_IDENTIFIED));
  game.add_item(IT_WAND_FIRE,1,0,0);
  game.add_item(IT_SCROLL_IDENTIFY);
  game.use_item(game.inv_n-1);
  assert(game.can_select_item(0));
  game.select_item(0);
  assert(game.inv[0].flags == IF_KNOWN && game.inv[1].flags == 0);
  game.add_item(IT_SCROLL_UP);
  game.use_item(game.inv_n-1);
  assert(game.transmute_open && game.can_select_item(1));
  const int weapon_upgrade = game.weapon_level;
  game.select_item(1);
  assert(game.inv[1].level == 1 && game.weapon_level == weapon_upgrade);
  const int inventory_before = game.inv_n;
  game.add_item(IT_SCROLL_UP);
  game.use_item(game.inv_n-1);
  game.transmute_open = false; // user presses B
  assert(game.inv_n == inventory_before+1 && game.inv[game.inv_n-1].id == IT_SCROLL_UP);
  game.inv_n = 0;
  game.add_item(IT_RING_MIGHT,1,0,IF_CURSED);
  game.equip_ring(0);
  assert(game.effective_strength() == game.strength-2);
  assert(game.maxhp==934);
  game.add_item(IT_LONGSWORD,1,3,IF_CURSED);
  assert(game.drop_item(0));
  assert(game.items[0].flags == IF_CURSED);
  game.pick_item(0);
  assert(game.inv[0].flags == IF_CURSED);
  assert(game.save(106));
  assert(restored.load(106));
  assert(restored.inv[0].flags == IF_CURSED && restored.ring1_flags == game.ring1_flags);

  // A genuine old-format-sized fixture: discard all V12 trailers, retain
  // the unchanged V11 bytes and version, then check explicit migration.
  std::ifstream v12("sdmc:/3ds/spd/slot106.bin",std::ios::binary);
  std::string legacy{std::istreambuf_iterator<char>(v12),{}};
  const size_t actor_offset = legacy.find("ACT1",sizeof(GameDisk));
  assert(actor_offset != std::string::npos);
  legacy.resize(actor_offset); legacy[4]=11; legacy[5]=0;
  std::ofstream v11("sdmc:/3ds/spd/slot108.bin",std::ios::binary);
  v11.write(legacy.data(),legacy.size()); v11.close();
  assert(restored.load(108));
  assert(restored.strength == game.strength && restored.hunger == game.hunger);
  assert(restored.inv[0].flags == IF_KNOWN && restored.ring1_flags == IF_KNOWN);
  assert(restored.health_time >= restored.action_remainder);

  arena(game,M_DM100,6,7);
  assert(game.bolt_reaches(6,7,10,7));
  game.tiles[7][8]=T_WALL;
  assert(!game.bolt_reaches(6,7,10,7));
  game.tiles[7][8]=T_FLOOR;
  Mob blocker; blocker.x=8; blocker.y=7; game.mobs.push_back(blocker);
  assert(!game.bolt_reaches(6,7,10,7));
  game.mobs.pop_back();
  game.armor=IT_PLATE; game.armor_level=100; game.strength=100;
  const int caster_x=game.mobs[0].x;
  bool magic_damage=false;
  for (int i=0; i<12; ++i) {
    const int hp=game.hp;
    game.mob_turn(game.mobs[0]);
    magic_damage |= game.hp<hp;
    assert(game.mobs[0].x==caster_x);
  }
  assert(magic_damage); // magic bypasses even enormous physical DR
  arena(game,M_SCORPIO,9,7);
  const int old_hp=game.hp;
  game.mob_turn(game.mobs[0]);
  assert(game.hp==old_hp && game.mobs[0].x<9);
  arena(game,M_SHAMAN,6,7);
  game.mobs[0].variant=2;
  for (int i=0; i<40 && !game.debuffs[status_rules::Hex]; ++i) game.mob_turn(game.mobs[0]);
  assert(game.debuffs[status_rules::Hex]==30);
  game.debuffs[status_rules::Poison]=8;
  const int poison_hp=game.hp;
  game.tick_buffs();
  assert(game.hp==poison_hp-3 && game.debuffs[status_rules::Poison]==7);
  game.debuffs[status_rules::Degrade]=30;
  assert(game.buffed_level(10)==5);
  assert(game.save(106) && restored.load(106));
  assert(restored.debuffs[status_rules::Poison]==7 && restored.mobs[0].variant==2);
  game.add_item(IT_POTION_HEAL); game.use_item(0);
  assert(game.debuffs[status_rules::Poison]==0);
  assert(game.debuffs[status_rules::Hex]>0 && game.debuffs[status_rules::Degrade]>0);

  arena(game,M_EYE,6,7);
  const int eye_hp=game.hp;
  game.spend_action(.1f);
  assert(game.mobs[0].beam_charged && game.hp==eye_hp);
  game.mobs[0].def=0;
  game.damage_mob(0,20);
  assert(game.mobs[0].hp==95);
  assert(game.save(106) && restored.load(106));
  assert(restored.mobs[0].beam_charged && restored.mobs[0].beam_target==7*MW+10);
  game.py=8; restored.py=8;
  game.invis_t=5; restored.invis_t=5;
  game.spend_action(2.f); restored.spend_action(2.f);
  assert(!game.mobs[0].beam_charged && game.hp==eye_hp);
  assert(game.mobs[0].beam_cooldown>=4 && game.mobs[0].beam_cooldown<=6);
  assert(game.rng==restored.rng && game.hp==restored.hp);

  arena(game,M_EYE,6,7);
  game.spend_action(.1f);game.py=8;
  game.mob_turn(game.mobs[0]);
  assert(!game.mobs[0].beam_charged && !game.combat_fx.empty());
  // A visible hero is tracked; an invisible hero above was able to dodge.
  assert(game.combat_fx.back().tx!=21 || game.combat_fx.back().ty!=7);

  arena(game,M_RAT,3,3);game.mobs.clear();
  game.features.push_back({10,7,F_TRAP_FOUND,0,TR_TOXIC});
  game.trigger_trap(0);
  assert(game.toxic_gas.cur[7*MW+10]==320);
  const int gas_hp=game.hp;
  game.tick_trap_clouds();
  assert(game.hp==gas_hp-1 && game.toxic_gas.cur[7*MW+10]>0);
  game.tick_trap_clouds();
  assert(game.save(106) && restored.load(106));
  for (int i=0;i<25;++i) {
    game.tick_trap_clouds();restored.tick_trap_clouds();
    assert(game.toxic_gas.cur==restored.toxic_gas.cur);
    assert(game.toxic_gas.volume==restored.toxic_gas.volume && game.hp==restored.hp);
  }
  game.new_run(0,42);
  assert(game.toxic_gas.volume==0);

  game.mobs.clear();game.mob_cap=0;
  const int first_entrance=game.entrance_cell;
  game.keys=3;game.golden_keys=2;
  game.features.push_back({game.px+1,game.py,F_TRAP_DISARMED,0,TR_ALARM});
  game.items.push_back({game.px+1,game.py+1,IT_LONGSWORD,2,IF_CURSED});
  const auto first_tiles=std::vector<uint8_t>(&game.tiles[0][0],&game.tiles[0][0]+MW*MH);
  game.descend();game.mobs.clear();game.mob_cap=0;
  assert(game.floor==2 && game.keys==0 && game.visited_floors.size()==1);
  game.keys=1;game.crystal_keys=1;
  assert(game.save(106) && restored.load(106));
  restored.ascend();game.ascend();
  assert(game.floor==1 && game.keys==3 && game.golden_keys==2 && game.entrance_cell==first_entrance);
  assert(std::equal(first_tiles.begin(),first_tiles.end(),&game.tiles[0][0]));
  assert(game.items.back().flags==IF_CURSED && game.items.back().level==2);
  assert(restored.floor==game.floor && restored.keys==game.keys && restored.items.size()==game.items.size());
  game.descend();
  assert(game.floor==2 && game.keys==1 && game.crystal_keys==1);
  // Save more entities than the legacy tables can hold; the floor archive
  // must preserve them without silently losing loot or monsters.
  game.items.clear();
  for(int i=0;i<80;++i) game.items.push_back({game.px,game.py,IT_FOOD_RATION});
  assert(game.save(106) && restored.load(106));
  assert(restored.items.size()==80);

  game.floor=25;game.boss_dead[4]=1;game.gen_level();game.mobs.clear();
  game.descend();
  assert(game.floor==26 && !game.won && game.map_width==16 && game.map_height==64);
  assert(game.px==8 && game.py==54 && game.items.size()==1);
  assert(game.items[0].id==IT_AMULET && game.items[0].x==8 && game.items[0].y==12);
  assert(game.save(106) && restored.load(106) && restored.floor==26);
  game.px=8;game.py=12;game.pick_item(0);
  assert(game.dialog && game.dlg_type==21 && !game.won);
  game.dialog_choose(0);game.ascend();
  assert(game.floor==25 && !game.won);
  game.floor=1;game.boss_dead[0]=1;game.ascend();
  assert(game.won && game.dlg_type==3);

  arena(game,M_NECROMANCER,6,7);
  game.spend_action(.1f);
  assert(game.mobs[0].summoning_pos>=0 && game.mobs.size()==1);
  game.spend_action(1.f);
  assert(game.mobs.size()==2 && game.mobs[0].minion==1 && game.mobs[1].owner==0);
  assert(game.mobs[1].hp==20 && game.mobs[1].maxhp==25);
  assert(game.save(106) && restored.load(106));
  assert(restored.mobs[0].minion==1 && restored.mobs[1].owner==0);
  game.spend_action(1.f);restored.spend_action(1.f);
  assert(game.rng==restored.rng && game.hp==restored.hp && game.mobs[1].hp==restored.mobs[1].hp);
  game.damage_mob(0,100);
  assert(!game.mobs[0].alive && !game.mobs[1].alive);
  assert(game.xp==7); // summoned skeleton never grants XP
  arena(game,M_SKELETON,6,7);game.mobs[0].adrenaline=3;
  assert(game.mob_turn(game.mobs[0])==.5f);
  game.mobs[0].x=9;game.mobs[0].y=7;
  assert(same_clock(game.mob_turn(game.mobs[0]),2.f/3.f));

  arena(game,M_BRUTE,6,7);game.mobs[0].hp=game.mobs[0].maxhp=40;
  game.damage_mob(0,100);
  assert(game.mobs[0].alive && game.mobs[0].hp==0 && game.mobs[0].rage_shield==24);
  for(int i=0;i<6;++i) game.tick_buffs();
  assert(!game.mobs[0].alive && game.mobs[0].has_raged);

  arena(game,M_GUARD,7,7);
  game.spend_action(.1f);
  assert(game.mobs[0].chains_used && game.px==8 && game.debuffs[status_rules::Cripple]);
  arena(game,M_MONK,9,7);
  game.spend_action(.1f);
  assert(game.mobs[0].focused);
  assert(!game.hero_hits(game.mobs[0],false) && !game.mobs[0].focused);
  assert(game.mobs[0].focus_cooldown>=6 && game.mobs[0].focus_cooldown<=7);

  arena(game,M_RAT,3,3);game.ascending=true;game.highest_ascent=26;
  game.floor=24;game.ascent_level_switch();
  assert(game.highest_ascent==24 && game.ascent_stacks==2.f);
  game.ascent_level_switch();assert(game.ascent_stacks==2.f);
  game.ascent_stacks=8.f;game.mobs.clear();
  const int curse_hp=game.hp;game.tick_buffs();
  assert(game.hp==curse_hp-1);
  game.floor=20;game.hunger=400;game.ascent_level_switch();
  assert(game.hunger==0 && game.flat_healing==20);
  assert(game.save(106) && restored.load(106));
  assert(restored.ascending && restored.ascent_stacks==8.f && restored.highest_ascent==20);
  arena(game,M_RAT,3,3);game.ascending=true;game.level=30;game.xpnext=155;
  game.ascent_stacks=2.f;game.ring1=IT_RING_FUROR;game.ring1_flags=0;
  game.damage_mob(0,1000);
  assert(game.xp==0 && game.ring1_identification.used>0.f && game.ascent_stacks==1.f);
  arena(game,M_RAT,3,3);game.ascending=true;game.level=10;game.xpnext=55;
  game.ascent_stacks=2.f;game.damage_mob(0,1000);assert(game.xp==0);
  game.paused=3;assert(game.paused==3);

  arena(game,M_GOO,8,7);game.mobs[0].pumped_up=1;
  const int pump_hp=game.hp;
  game.mob_turn(game.mobs[0]);
  assert(game.mobs[0].pumped_up==2 && game.hp==pump_hp);
  game.mob_turn(game.mobs[0]);
  assert(game.mobs[0].pumped_up==0);
  game.tiles[7][8]=T_WATER;game.mobs[0].hp=50;game.mobs[0].ai=Mob::Sleeping;
  game.invis_t=5;game.mob_turn(game.mobs[0]);
  assert(game.mobs[0].hp==51);
  arena(game,M_SLIME,6,7);
  game.damage_mob(0,25);assert(game.mobs[0].hp==90);
  arena(game,M_RAT,3,3);game.mobs.clear();
  assert(game.save(106));
  std::ifstream good_input("sdmc:/3ds/spd/slot106.bin",std::ios::binary);
  const std::string good_save{std::istreambuf_iterator<char>(good_input),{}};
  game.map_width=0;assert(!game.save(106));
  std::ifstream retained_input("sdmc:/3ds/spd/slot106.bin",std::ios::binary);
  const std::string retained_save{std::istreambuf_iterator<char>(retained_input),{}};
  assert(good_save==retained_save);

  arena(game,M_RAT,3,3);game.mobs.clear();game.inv_n=0;
  game.add_item(IT_WAND_MAGIC);game.inv[0].charges=0;
  const float increment=1.f/(10.f+40.f*std::pow(.875f,3));
  game.recharge_wands();
  assert(same_clock(game.inv[0].partial_charge,increment));
  assert(game.save(106) && restored.load(106));
  assert(same_clock(game.inv[0].partial_charge,restored.inv[0].partial_charge));
  for(int i=0;i<40;++i) {game.recharge_wands();restored.recharge_wands();}
  assert(game.inv[0].charges==restored.inv[0].charges && game.inv[0].charges==1);
  game.drop_item(0);game.pick_item(0);
  assert(game.inv[0].charges==1);
  assert(game.inv[0].partial_charge>0.f);

  arena(game,M_RAT,3,3);game.mobs.clear();
  game.ring1=IT_RING_FUROR;game.ring1_level=2;
  assert(game.melee_delay()<.8f);
  game.ring1=IT_RING_MIGHT;game.ring1_level=0;
  game.update_maxhp();assert(game.maxhp==1035);
  game.ring1=IT_RING_TENACITY;game.ring1_level=2;
  game.update_maxhp();game.hp=100;
  game.take_damage(100);assert(game.hp>0);
  game.ring1=IT_NONE;game.update_maxhp();game.inv_n=0;
  for(int i=0;i<BAG_CAP;++i) game.add_item(IT_DAGGER);
  game.items.push_back({game.px,game.py,IT_LONGSWORD});
  game.pick_item(0);assert(game.items.size()==1 && game.inv_n==BAG_CAP);
  game.inv_n=0;game.add_item(IT_WAND_FIRE);game.inv[0].charges=1;game.inv[0].partial_charge=.3f;
  game.add_item(IT_SCROLL_TRANSMUTE);game.use_item(1);game.select_item(0);
  assert(game.inv[0].id!=IT_WAND_FIRE && game.inv[0].charges<=1 && game.inv[0].partial_charge>.3f);
  game.inv_n=0;game.add_item(IT_PLATE);game.selection_scroll=IT_SCROLL_TRANSMUTE;
  assert(!game.can_select_item(0));
  game.inv_n=0;game.add_item(IT_STONE_AUG);
  game.use_item(0);game.select_item(-1);
  assert(game.dialog && game.dlg_type==22);
  const int augment_level=game.weapon_level;
  game.dialog_choose(0);
  assert(game.weapon_augment==1 && game.weapon_level==augment_level && game.inv_n==0);
  assert(game.melee_delay()<.7f);
  game.armor=IT_CLOTH_ARMOR;
  game.add_item(IT_STONE_AUG);game.use_item(0);game.select_item(-2);game.dialog_choose(1);
  assert(game.armor_augment==2);
  assert(game.save(106) && restored.load(106));
  assert(restored.weapon_augment==1 && restored.armor_augment==2);
  game.add_item(IT_STONE_AUG);game.use_item(0);game.select_item(-1);
  game.dialog_choose(3);assert(game.inv_n==1 && game.weapon_augment==1);
  game.use_item(0);game.select_item(-1);game.dialog_choose(0);
  assert(game.inv_n==1 && game.weapon_augment==1);
  game.inv_n=0;game.add_item(IT_PLATE);game.add_item(IT_STONE_AUG);
  game.use_item(1);game.select_item(0);game.dialog_choose(0);
  assert(game.inv_n==1 && game.inv[0].augment==1 && game.inv[0].level==0);
  game.drop_item(0);game.pick_item(0);assert(game.inv[0].augment==1);
  game.weapon_flags=0;game.weapon_identification={};
  for(int i=0;i<20;++i) game.identify_use(game.weapon_flags,game.weapon_identification,20.f);
  assert(!(game.weapon_flags&IF_IDENTIFIED) && game.weapon_identification.used==10.f);
  game.identify_on_exp(game.xpnext/2);
  for(int i=0;i<10;++i) game.identify_use(game.weapon_flags,game.weapon_identification,20.f);
  assert(game.weapon_flags&IF_IDENTIFIED);
  game.ring1=IT_RING_FUROR;game.ring1_flags=0;game.ring1_identification={};
  game.identify_on_exp(game.xpnext);assert(game.ring1_flags&IF_IDENTIFIED);
  assert(game.save(106) && restored.load(106));
  assert(restored.weapon_identification.used==20.f && restored.ring1_identification.used==1.f);
  // Evasion cloth +3 uses the original fallback before DRMin, not a negative range.
  auto evasion_cloth=hero_rules::armor_range(IT_CLOTH_ARMOR,3,1);
  assert(evasion_cloth.min==1 && evasion_cloth.max==2);
  arena(game,M_RAT,3,3);game.mobs.clear();game.inv_n=0;
  game.add_item(IT_LONGSWORD,1,2,0);game.inv[0].augment=2;
  game.inv[0].identification={7.f,3.f};game.gold=10000;
  game.shop_sell(0);
  assert(game.save(106) && restored.load(106));
  restored.shop_buyback(0);
  assert(restored.inv[0].augment==2 && restored.inv[0].identification.used==7.f);
  assert(restored.inv[0].flags==0 && restored.buyback_n==0);
  game.inv_n=0;game.add_item(IT_WAND_FIRE);game.inv[0].charges=1;
  game.inv[0].partial_charge=.25f;game.inv[0].identification={2.f,1.f};
  game.shop_sell(0);assert(game.save(106) && restored.load(106));
  restored.shop_buyback(0);
  assert(restored.inv[0].charges==1 && restored.inv[0].partial_charge==.25f);
  assert(restored.inv[0].identification.used==2.f);
  game.inv_n=0;for(int i=0;i<BAG_CAP;++i) game.add_item(IT_DAGGER);
  const int full_gold=game.gold;game.shop_buyback(0);
  assert(game.gold==full_gold && game.buyback_n>0);
  arena(game,M_RAT,3,3);game.mobs.clear();
  assert(game.save(106));
  std::ifstream valid_input("sdmc:/3ds/spd/slot106.bin",std::ios::binary);
  std::string corrupt{std::istreambuf_iterator<char>(valid_input),{}};
  const auto archive_start=corrupt.find("FLR2");assert(archive_start!=std::string::npos);
  // A valid file with an invalid archived width must be rejected before mutation.
  const int invalid_width=1000;
  std::memcpy(corrupt.data()+archive_start+8,&invalid_width,sizeof(invalid_width));
  std::ofstream corrupt_out("sdmc:/3ds/spd/slot108.bin",std::ios::binary);
  corrupt_out.write(corrupt.data(),corrupt.size());corrupt_out.close();
  const int previous_floor=restored.floor,previous_hp=restored.hp;
  assert(!restored.load(108) && restored.floor==previous_floor && restored.hp==previous_hp);
  assert(game.save(106));
  game.toxic_gas.left=-1;assert(!game.save(106));
  assert(restored.load(106));
  // v0.1.14 requested mechanics: physical swarm splitting before DR.
  arena(game,M_SWARM,9,7);game.mobs[0].hp=game.mobs[0].maxhp=50;game.mobs[0].def=0;
  game.damage_mob(0,10,DamageKind::Physical);
  assert(game.pending_swarms.size()==1 && game.mobs[0].hp==20 && game.pending_swarms[0].hp==20);
  assert(game.pending_swarms[0].variant==1);
  game.spend_action(.01f);assert(game.mobs.size()==2 && game.pending_swarms.empty());
  game.damage_mob(1,1000);assert(game.xp==0);
  arena(game,M_SWARM,9,7);game.damage_mob(0,10);assert(game.pending_swarms.empty());
  arena(game,M_RAT,3,3);game.mobs.clear();game.inv_n=0;game.add_item(IT_WATERSKIN);
  for(int i=0;i<20;++i) {game.items.push_back({game.px,game.py,IT_DEWDROP});game.pick_item(0);}
  assert(game.inv[0].charges==20 && game.items.empty());
  game.hp=500;game.use_item(0);assert(game.hp==1000 && game.inv[0].charges==10);
  game.items.push_back({game.px,game.py,IT_DEWDROP});game.inv[0].charges=20;
  game.pick_item(0);assert(game.items.size()==1);
  game.hp=950;game.pick_item(0);assert(game.hp==1000 && game.items.empty());
  game.tiles[7][11]=T_HIGH_GRASS;game.trample_grass(11,7,true);assert(game.tiles[7][11]==T_GRASS);
  auto after_grass_rng=game.rng;game.trample_grass(11,7,true);assert(game.rng==after_grass_rng);
  game.init_identities();assert(!game.identity_known(IT_POTION_STR));
  assert(std::string(game.display_name(IT_POTION_STR))!=item_name(IT_POTION_STR));
  Item unknown;unknown.id=IT_POTION_STR;char details[256];game.describe_item(unknown,details,sizeof(details));
  assert(std::string(details).find("永久增加")==std::string::npos);
  unknown.id=IT_RING_MIGHT;unknown.flags=IF_CURSED;unknown.level=5;
  game.describe_item(unknown,details,sizeof(details));assert(std::string(details).find("+5")==std::string::npos);
  auto appearance=game.display_sprite(IT_POTION_STR);
  assert(game.save(106) && restored.load(106));
  assert(restored.inv[0].charges==20 && !restored.identity_known(IT_POTION_STR));
  assert(restored.display_sprite(IT_POTION_STR)==appearance);
  // Genuine fixtures written by the released V13 runtime, not relabeled V14.
  {
    std::ifstream source("legacy-v12.bin",std::ios::binary);
    std::string old{std::istreambuf_iterator<char>(source),{}};
    assert(!old.empty());
    std::ofstream fixture("sdmc:/3ds/spd/slot108.bin",std::ios::binary);
    fixture.write(old.data(),old.size());fixture.close();
    assert(restored.load(108) && restored.identity_known(IT_POTION_STR));
    SlotMeta meta;assert(Game::slot_meta(108,meta));
    std::ifstream v13("legacy-v13.bin",std::ios::binary);
    std::string legacy{std::istreambuf_iterator<char>(v13),{}};
    std::ofstream save13("sdmc:/3ds/spd/slot108.bin",std::ios::binary);
    save13.write(legacy.data(),legacy.size());save13.close();
    assert(restored.load(108) && !restored.identity_known(IT_POTION_STR));
    assert(restored.inv[0].id==IT_WATERSKIN && restored.inv[0].charges==17);
    assert(restored.mobs.size()>0 && restored.weapon_magic.effect==0);
  }
  game.add_item(IT_POTION_STR);game.use_item(1);assert(game.identity_known(IT_POTION_STR));
  const int search_turn=game.turns;game.search_button();assert(game.examining && game.turns==search_turn);
  game.examine_at(game.px,game.py);assert(game.dialog && game.dlg_type==23 && game.turns==search_turn);
  game.dialog_choose(0);game.search_button();assert(!game.examining && game.turns==search_turn+1);
  arena(game,M_RAT,7,7);game.recompute_fov();assert(game.next_step_encounter(9,7));
  const auto safe_rng=game.rng;const auto safe_turn=game.turns;
  game.pvx=game.px;game.pvy=game.py;game.path_goal=7*MW+2;game.tick(100);
  assert(game.px==10 && game.turns==safe_turn && game.rng==safe_rng && game.path_goal==-1);
  arena(game,M_RAT,3,3);game.mobs.clear();game.ghost_floor=2;
  game.spawn_quest_miniboss();assert(game.ghost_state==1 && game.mobs.size()==1);
  assert(!game.vis[game.mobs[0].y][game.mobs[0].x]);
  // Guard must never be injected by SentryRoom in the first region.
  for(uint32_t seed=1;seed<=12;++seed) {
    game.new_run(0,seed);
    for(int depth=1;depth<=4;++depth) {
      game.floor=depth;game.gen_level();
      for(const auto& mob:game.mobs) assert(mob.type!=M_GUARD);
    }
  }
  std::puts("Requested mechanics tests passed: region guards, swarm split/XP, grass, dew/flask, identity/save, inspect, encounter stop and distant quest spawn.");

  // End-to-end entry points for the new item engine.
  arena(game,M_RAT,9,7);
  game.mobs[0].sleep_t=10;
  game.weapon_magic.effect=magic_rules::Crystal;
  game.attack(0,false);
  assert(game.weapon_magic.durability<100 && game.mobs[0].hp<100);
  game.mobs.clear();
  game.add_item(IT_SCROLL_ENCHANT);
  game.use_item(0);game.select_item(-1);
  assert(game.dlg_type==24 && game.dlg_optn==4);
  const int before_cancel=game.turns;
  game.dialog_choose(3);
  assert(game.inv_n==1 && game.turns==before_cancel && game.weapon_magic.effect==magic_rules::Crystal);
  game.use_item(0);game.select_item(-1);
  const int chosen=game.magic_choice[1];game.dialog_choose(1);
  assert(game.inv_n==0 && game.weapon_magic.effect==chosen);
  game.add_item(IT_ARCANE_STYLUS);
  game.use_item(0);assert(!game.can_select_item(-1));
  game.select_item(-2);assert(game.inv_n==1); // arena has no armor
  game.armor=IT_CLOTH_ARMOR;game.use_item(0);game.select_item(-2);
  assert(game.inv_n==0 && game.armor_magic.effect>0);
  game.armor_magic.effect=magic_rules::Stench;
  game.armor_flags=IF_KNOWN|IF_CURSED;
  game.add_item(IT_SCROLL_CURSE);game.use_item(0);game.select_item(-2);
  assert(game.armor_magic.effect==0 && !(game.armor_flags&IF_CURSED));

  game.artifact_generated=0;
  unsigned generated=0;
  for(int i=0;i<11;++i) {
    const uint8_t id=game.generate_artifact();
    assert(artifact_rules::ordinary(id));
    const unsigned bit=1u<<artifact_rules::index(id);
    assert(!(generated&bit));generated|=bit;
  }
  assert(game.generate_artifact()==IT_NONE && game.artifact_generated==generated);
  game.add_item(IT_ART_HOURGLASS);game.use_item(0);
  assert(game.ring1==IT_ART_HOURGLASS && game.ring1_magic.aux==5);
  game.artifact_menu(-3);game.dialog_choose(0);
  assert(game.dlg_type==27);
  const int before_freeze=game.turns;
  game.dialog_choose(1);game.spend_action(1);
  assert(game.freeze_left==1 && game.ring1_magic.aux==4 && game.spell_effects.hourglass_active && game.turns==before_freeze);
  game.add_item(IT_ART_ROSE);game.use_item(0);
  game.artifact_action(-4,0);
  assert(game.rose_ghost>=0 && game.mobs[game.rose_ghost].effects.ally);
  game.add_item(IT_DAGGER);game.inv[0].magic.effect=magic_rules::Blazing;
  assert(game.drop_item(0));
  game.hero_effects.bleeding=4;game.deferred_damage=8;game.loot_remaining[10]=1;
  assert(game.save(106) && restored.load(106));
  assert(restored.ring1==IT_ART_HOURGLASS && restored.freeze_left==game.freeze_left);
  assert(restored.weapon_magic.effect==chosen && restored.items.back().magic.effect==magic_rules::Blazing);
  assert(restored.artifact_generated==game.artifact_generated && restored.loot_remaining[10]==1);
  assert(restored.hero_effects.bleeding==4 && restored.deferred_damage==8);
  assert(restored.rose_ghost==game.rose_ghost && restored.mobs[restored.rose_ghost].rose_ally);
  game.new_run(2,42);assert(game.ring1==IT_ART_CLOAK && game.turns==0);
  game.new_run(5,42);assert(game.ring1==IT_ART_TOME && game.turns==0);
  std::puts("Magic tests passed: Crystal combat, selection/cancel, stylus, cleansing, unique drops, starting artifacts, freeze and V14 persistence.");
  // V15 mechanics exercise real UI dispatch, save continuation and failed paths.
  arena(game,M_RAT,15,7);game.mobs[0].ai=Mob::Sleeping;game.mobs[0].sleep_t=30;
  game.strength=30;game.add_item(IT_KNIFE,3);game.inv[0].magic.effect=magic_rules::Crystal;
  const auto serial=game.inv[0].magic.aux2;
  game.throw_projectile(0,15,7);
  assert(game.inv[0].count==2 && game.inv[0].magic.durability<100 && game.inv[0].magic.aux2==serial);
  assert(game.items.size()==1 && game.items[0].magic.aux==1);
  assert(game.items[0].magic.durability==100); // stack absorbs wear
  game.pick_item(0);assert(game.items.size()==1); // pinned until enemy dies
  assert(game.save(106) && restored.load(106));
  assert(restored.items[0].magic.aux==1 && restored.inv[0].magic.durability==game.inv[0].magic.durability);
  game.damage_mob(0,1000);assert(game.items[0].magic.aux==0);
  game.pick_item(0);assert(game.inv[0].count==3);
  for(const auto& drop:game.items)assert(drop.id!=IT_KNIFE);
  arena(game,M_RAT,15,7);game.mobs.clear();game.add_item(IT_KNIFE);
  game.tiles[7][12]=T_WALL;game.throw_projectile(0,16,7);
  assert(game.inv_n==0 && game.items.size()==1 && game.items[0].x<13);
  assert(game.items[0].magic.durability==100); // miss/empty-cell throws do not wear
  assert(projectile_rules::durability_cost(IT_DART,0,-4,0,0)==0);
  assert(projectile_rules::durability_cost(IT_KNIFE,0,0,0,magic_rules::Crystal)>projectile_rules::durability_cost(IT_KNIFE,0,0,0,0));
  arena(game,M_RAT,20,7);game.mobs.clear();game.ring1=IT_ART_SANDALS;game.ring1_magic={};
  for(int i=1;i<12;++i)game.add_item(seed_rules::ids[i]);
  for(int n=0;n<3;++n)game.artifact_feed(-3,0);
  assert(game.ring1_level==1 && game.ring1_magic.experience==0);
  for(int n=0;n<6;++n)game.artifact_feed(-3,0);
  assert(game.ring1_level==2 && game.ring1_magic.experience==0);
  game.ring1_level=3;game.ring1_magic.aux2=IT_SEED_SUNGRASS;
  game.add_item(IT_SEED_EARTHROOT);const int root_index=game.inv_n-1;
  assert(game.artifact_feedable(-3,root_index));game.artifact_feed(-3,root_index);
  game.ring1_magic.aux=100;game.artifact_action(-3,0,game.py*MW+game.px);
  assert(game.spell_effects.earth>0 && game.ring1_magic.aux==60);
  const int health=game.hp;game.take_damage(5);assert(game.hp>health-5);
  game.px++;game.tick_extended_effects();assert(game.spell_effects.earth==0);
  game.seed_effect(IT_SEED_SUNGRASS,game.py*MW+game.px);game.hp=500;
  for(int n=0;n<5;++n)game.tick_extended_effects();assert(game.hp>500);
  assert(game.save(106) && restored.load(106));
  assert(restored.spell_effects.healing==game.spell_effects.healing);
  arena(game,M_RAT,20,7);game.mobs.clear();game.ring1=IT_ART_ARMBAND;game.ring1_magic.aux=5;
  game.shop_open=true;game.shop_n=1;game.shop_items[0]={IT_FOOD_MEAT};game.shop_sold[0]=false;
  int used=0;assert(game.shop_steal_chance(0,used)==1 && used==2);
  const int prior_gold=game.gold;game.shop_steal(0);
  assert(game.shop_sold[0] && game.gold==prior_gold && game.inv[0].id==IT_FOOD_MEAT && game.ring1_magic.aux==3);
  const auto after_theft=game.rng;game.shop_steal(0);assert(game.rng==after_theft);
  arena(game,M_RAT,20,7);game.mobs.clear();game.ring1=IT_ART_ROSE;game.ring1_magic.aux=100;
  game.add_item(IT_DAGGER);game.rose_outfit(0);assert(game.rose_weapon.id==IT_DAGGER && game.inv_n==0);
  game.add_item(IT_PLATE);game.rose_outfit(0);assert(game.rose_armor.id==IT_NONE && game.inv_n==1);
  game.artifact_action(-3,0);assert(game.rose_ghost>=0);
  game.rose_command=6*MW+7;
  assert(game.save(106) && restored.load(106));
  assert(restored.rose_weapon.id==IT_DAGGER && restored.rose_command==game.rose_command);
  // Cross-floor ownership uses the new actor index, including hidden stasis
  // actors. Ordinary mirror images stay on their original floor.
  const int ghost=game.rose_ghost;
  game.actor_effects(ghost).power_left=75;game.actor_effects(ghost).wall=7;
  game.powered_ally=game.linked_ally=ghost;game.link_left=9;
  game.stasis_ally=ghost;game.stasis_left=60;game.mobs[ghost].alive=false;game.rose_ghost=-1;
  Mob mirror;mirror.type=M_WRAITH;mirror.x=11;mirror.y=7;mirror.effects.ally=true;
  game.mobs.push_back(mirror);game.actor_effects(game.mobs.size()-1).mirror=1;
  game.spell_effects.light_shield=4;game.spell_effects.shield_target=ghost;
  game.spell_effects.earth=30;game.spell_effects.earth_anchor=game.py*MW+game.px;
  game.entrance_cell=game.py*MW+game.px;
  game.descend();assert(game.floor==2 && game.stasis_ally>=0);
  const int carried=game.stasis_ally;
  assert(game.powered_ally==carried && game.linked_ally==carried && game.link_left==9);
  assert(!game.mobs[carried].alive && game.mobs[carried].rose_ally && game.stasis_left==60);
  assert(game.actor_effects(carried).power_left==75 && game.actor_effects(carried).wall==7);
  assert(game.spell_effects.light_shield==0 && game.spell_effects.shield_target==-1);
  assert(game.spell_effects.earth==0 && game.spell_effects.anchor==-1);
  assert(game.save(106) && restored.load(106));
  for(float duration:{.2f,.8f,1.5f}) {
    game.spend_action(duration);restored.spend_action(duration);
    assert(game.rng==restored.rng && game.hp==restored.hp && game.stasis_left==restored.stasis_left);
  }
  game.ascend();assert(game.floor==1 && game.stasis_ally>=0);
  int mirror_count=0;for(size_t i=0;i<game.mobs.size();++i)if(game.mobs[i].alive && game.actor_effects(i).mirror>0)++mirror_count;
  assert(mirror_count==1);
  game.stasis_left=.001f;game.tick_extended_effects();
  assert(game.stasis_ally==-1 && game.rose_ghost>=0 && game.rose_ghost==game.powered_ally);
  assert(game.linked_ally==game.rose_ghost && game.actor_effects(game.rose_ghost).wall==7);
  arena(game,M_RAT,15,7);game.hero=5;game.ring1=IT_ART_TOME;game.ring1_magic.aux=3;
  game.mobs[0].sleep_t=20;game.tome_cast(cleric_rules::GuidingLight,7*MW+15);
  assert(game.mobs[0].hp<100 && game.actor_effects(0).illuminated && game.ring1_magic.aux==2);
  const int turn_before_buff=game.turns;game.tome_cast(cleric_rules::HolyWeapon);
  assert(game.spell_effects.holy_weapon==50 && game.turns==turn_before_buff && game.ring1_magic.aux==0);
  assert(!game.cleric_learn(cleric_rules::Sunray));
  game.level=7;assert(game.cleric_learn(cleric_rules::Sunray));
  assert(game.cleric_unlocked(cleric_rules::Sunray) && !game.cleric_unlocked(cleric_rules::HolyLance));
  game.actor_effects(0).sunray_recent=5;game.actor_effects(0).sunray_used=true;
  assert(game.save(106) && restored.load(106));
  assert(restored.cleric_ranks[cleric_rules::Sunray]==1 && restored.actor_effects(0).sunray_used);
  arena(game,M_RAT,15,7);game.ring1=IT_ART_SPELLBOOK;game.ring1_magic.aux=3;
  game.book_scroll=IT_SCROLL_RECHARGE;game.spellbook_resolve(true);
  assert(game.spell_effects.artifact_recharge>0 && game.ring1_magic.aux<=2);
  game.book_scroll=IT_SCROLL_IDENTIFY;game.spellbook_resolve(false);
  game.add_item(IT_DAGGER,1,0,0);game.select_item(0);
  assert((game.inv[0].flags&IF_KNOWN)==IF_KNOWN && !game.book_empowered && game.book_scroll==IT_NONE);
  game.init_identities();int known_before=0;for(int i=0;i<IT_COUNT;++i)known_before+=game.identity_known(i);
  game.scroll_effect(IT_SCROLL_DIVINATION);int known_after=0;for(int i=0;i<IT_COUNT;++i)known_after+=game.identity_known(i);
  assert(known_after==known_before+4);
  // Cancelling a free book picker must not empower a subsequent physical scroll.
  game.book_scroll=IT_SCROLL_IDENTIFY;game.book_empowered=true;game.transmute_open=true;
  game.artifact_feed_slot=101;game.cancel_item_selection();
  assert(!game.transmute_open && !game.book_empowered && game.book_scroll==IT_NONE && game.artifact_feed_slot==0);
  arena(game,M_RAT,15,7);game.hp=500;game.mobs[0].hp=40;game.mobs[0].maxhp=100;
  game.scroll_effect(IT_SCROLL_RETRIBUTION);
  assert(game.mobs[0].hp==10 && game.mobs[0].effects.blind==10);
  assert(game.hero_effects.blind==10 && game.debuffs[status_rules::Weakness]==20);
  arena(game,M_RAT,15,7);game.hp=100;game.mobs[0].hp=40;game.mobs[0].maxhp=100;
  game.scroll_effect(IT_SCROLL_RETRIBUTION);assert(!game.mobs[0].alive);
  // A prismatic actor still queued at save time keeps its actor extension.
  arena(game,M_RAT,14,7);game.spell_effects.prismatic_guard=12;
  game.tick_extended_effects();assert(game.pending_swarms.size()==1);
  assert(game.save(106) && restored.load(106));
  assert(restored.mobs.size()==game.mobs.size()+1 && restored.actor_effects(restored.mobs.size()-1).prismatic==1);
  arena(game,M_RAT,20,7);game.mobs.clear();game.add_item(IT_SHURIKEN,3);game.strength=30;
  const int initial_turns=game.turns;game.throw_projectile(0,15,7);
  assert(game.turns==initial_turns && game.spell_effects.shuriken_cooldown==19);
  game.throw_projectile(0,15,7);assert(game.turns>initial_turns && game.spell_effects.shuriken_cooldown<19);
  arena(game,M_RAT,20,7);game.mobs.clear();game.hero=5;game.ring1=IT_ART_TOME;
  game.ring1_level=7;game.ring1_magic.aux=10;game.cleric_ability=2;game.cleric_ranks[cleric_rules::SpiritForm]=2;
  game.add_item(IT_RING_MIGHT);const int original_max=game.maxhp,original_str=game.effective_strength();
  game.cleric_form(cleric_rules::SpiritForm,0);
  assert(game.trinity_spirit.id==IT_RING_MIGHT && game.ring1_magic.aux==6 && game.spell_effects.spirit_left==0);
  game.trinity_use(2);
  assert(game.effective_strength()==original_str+3 && game.maxhp>original_max);
  game.spell_effects.spirit_left=1;game.tick_extended_effects();
  assert(game.maxhp==original_max && game.effective_strength()==original_str);
  game.spell_effects.spirit_left=20;game.spell_effects.spirit_ring=IT_RING_FUROR;
  assert(game.melee_delay()<hero_rules::attack_delay(game.weapon,game.weapon_level,game.effective_strength()));
  game.discover_item(game.inv[0]);game.remove_item(0);game.trinity_menu(cleric_rules::SpiritForm);
  assert(!game.trinity_options.empty() && game.trinity_options[0].id==IT_RING_MIGHT);
  game.cleric_ranks[cleric_rules::MindForm]=1;game.ring1_magic.aux=10;game.add_item(IT_WAND_MAGIC);
  game.cleric_form(cleric_rules::MindForm,0);
  assert(game.trinity_mind.id==IT_WAND_MAGIC && game.ring1_magic.aux==7);
  assert(game.save(106) && restored.load(106));
  assert(restored.trinity_mind.id==IT_WAND_MAGIC && restored.trinity_spirit.id==IT_RING_MIGHT && restored.discovered_items[IT_RING_MIGHT]);
  const float armor_before=game.armor_charge;const int charges_before=game.ring1_magic.aux;
  game.trinity_use(1);assert(game.targeting && game.armor_charge==armor_before);
  game.trinity_use(1,game.py*MW+game.px);assert(game.armor_charge==armor_before);
  game.trinity_use(1,7*MW+15);assert(game.armor_charge<armor_before && game.ring1_magic.aux>=charges_before);
  arena(game,M_RAT,20,7);game.mobs.clear();game.strength=30;game.add_item(IT_BOOMERANG);
  game.throw_projectile(0,15,7);assert(game.inv_n==0 && game.items.size()==1 && game.items[0].magic.cooldown>0);
  game.pick_item(0);assert(game.inv_n==0); // cannot pick up an airborne boomerang
  for(int n=0;n<5;++n)game.tick_extended_effects();
  assert(game.inv_n==1 && game.inv[0].id==IT_BOOMERANG && game.items.empty());
  arena(game,M_RAT,10,7);game.px=12;game.strength=30;game.mobs[0].sleep_t=100;
  FloorItem returning{15,7,IT_BOOMERANG};returning.magic.aux=-(7*MW+10)-1;returning.magic.cooldown=1;
  game.items.push_back(returning);const int return_turn=game.turns;
  game.tick_extended_effects();assert(game.turns==return_turn && game.mobs[0].hp<100);
  assert(game.items.size()==1 && game.items[0].magic.aux==0 && game.items[0].magic.cooldown==0 && game.items[0].magic.durability<100);
  arena(game,M_RAT,20,7);game.mobs.clear();game.hero=5;game.ring1=IT_ART_TOME;game.ring1_level=7;game.ring1_magic.aux=10;
  game.cleric_ranks[cleric_rules::RecallInscription]=2;game.recalled_item=IT_SCROLL_ENCHANT;game.recall_left=300;
  game.add_item(IT_DAGGER);game.tome_cast(cleric_rules::RecallInscription);assert(game.transmute_open && game.ring1_magic.aux==2);
  game.select_item(0);assert(game.dlg_type==24);game.dialog_choose(0);
  assert(game.inv_n==1 && game.inv[0].magic.effect>0 && game.recalled_item==IT_NONE && !game.inscription_replay);
  // Physical scrolls and spellbook reads share the same timed effects.
  arena(game,M_RAT,15,7);game.add_item(IT_SCROLL_LULLABY);game.use_item(0);
  assert(game.inv_n==0 && game.spell_effects.drowsy>0 && game.actor_effects(0).drowsy>0 && game.mobs[0].sleep_t==0);
  for(int n=0;n<5;++n)game.tick_extended_effects();
  assert(game.actor_effects(0).magical_sleep);
  game.damage_mob(0,1);assert(!game.actor_effects(0).magical_sleep);
  arena(game,M_RAT,20,7);game.mobs.clear();game.add_item(IT_WAND_MAGIC);game.inv[0].charges=0;
  game.scroll_effect(IT_SCROLL_RECHARGE);game.recharge_wands();
  assert(game.inv[0].charges==0 && game.inv[0].partial_charge>.25f);
  const auto partial=game.inv[0].partial_charge;game.spell_effects.magic_immunity=20;game.recharge_wands();
  assert(game.inv[0].partial_charge==partial);
  arena(game,M_RAT,15,7);game.mobs[0].effects.ally=true;game.mobs[0].effects.poison=4;
  game.hero=5;game.ring1=IT_ART_TOME;game.ring1_magic.aux=3;game.cleric_subclass=1;game.cleric_ranks[cleric_rules::Cleanse]=2;
  game.hero_effects.poison=4;game.tome_cast(cleric_rules::Cleanse);
  assert(game.hero_effects.poison==0 && game.mobs[0].effects.poison==0 && game.shield>=20 && game.actor_effects(0).shield>=20);
  // Ghost kills award XP after restoring the hero's equipment and health.
  arena(game,M_RAT,15,7);game.ring1=IT_ART_ROSE;game.ring1_magic.aux=100;
  game.artifact_action(-3,0);game.rose_weapon.id=IT_GREATSWORD;game.rose_weapon.level=10;
  game.mobs[0].hp=1;game.mobs[0].sleep_t=100;game.xp=9;game.xpnext=10;
  const int ghost_max=game.mobs[game.rose_ghost].maxhp;game.rose_attack(0);
  assert(game.level==2 && game.maxhp==1005 && game.mobs[game.rose_ghost].maxhp==ghost_max);
  assert(game.pending_ally_experience.empty() && game.magic_actor==-1 && game.magic_hero_cell==-1);
  // Viscosity on the ghost owns its deferred damage and cannot change the hero.
  game.rose_armor.id=IT_CLOTH_ARMOR;game.rose_armor.magic.effect=magic_rules::Viscosity;
  Mob attacker;attacker.type=M_RAT;attacker.x=14;attacker.y=7;game.mobs.push_back(attacker);
  game.deferred_damage=7;const int hero_health=game.hp;int incoming=12;
  game.rose_defend(game.mobs.size()-1,incoming);
  assert(incoming==10 && game.deferred_damage==7 && game.hp==hero_health && game.actor_effects(game.rose_ghost).deferred==2);
  arena(game,M_RAT,15,7);game.hero=5;game.cleric_ranks[cleric_rules::LightReading]=3;
  game.cleric_ranks[cleric_rules::SatiatedSpells]=2;game.cleric_ranks[cleric_rules::EnlighteningMeal]=2;
  game.add_item(IT_ART_TOME);game.inv[0].magic.aux=2;game.inv[0].charges=2;
  assert(game.tome_slot()==0);game.cleric_food();assert(game.inv[0].magic.aux==3 && game.spell_effects.satiated_spell);
  game.tome_cast(cleric_rules::HolyWeapon);
  assert(game.inv[0].magic.aux==1 && game.inv[0].charges==1 && game.shield==5 && !game.spell_effects.satiated_spell);
  const float stored=game.inv[0].magic.stored;game.tick_magic();assert(game.inv[0].magic.stored>stored);
  game.use_item(0);assert(game.dialog && game.dlg_type==28);
  assert(game.save(106) && restored.load(106));
  assert(restored.cleric_ranks[cleric_rules::LightReading]==3 && restored.tome_slot()==0 && restored.inv[0].charges==1);
  const auto saved_health=restored.hp;
  game.spell_effects.bless=std::nanf("");assert(!game.save(106));
  assert(restored.load(106) && restored.hp==saved_health && restored.spell_effects.bless==0);
  arena(game,M_RAT,20,7);game.mobs.clear();game.hero=5;game.cleric_ability=2;game.trinity_spirit.id=IT_ART_HOURGLASS;
  game.cleric_ranks[cleric_rules::SpiritForm]=2;game.cleric_ranks[cleric_rules::HeroicEnergy]=4;
  game.trinity_use(2);assert(game.armor_charge==79 && game.spell_effects.bubble_active && game.spell_effects.time_bubble==6);
  // Holy effects run once after the weapon/glyph and the Trinity imprint.
  arena(game,M_RAT,15,7);game.mobs[0].sleep_t=100;game.cleric_subclass=2;
  game.spell_effects.holy_weapon=50;game.spell_effects.body_left=20;
  game.spell_effects.body_effect=magic_rules::Crystal;game.spell_effects.body_durability=30;
  const auto real_weapon=game.weapon;int combined=0;
  game.weapon_proc(0,30,combined);
  assert(game.mobs[0].hp==84 && game.weapon==real_weapon && game.spell_effects.body_durability<30);
  game.spell_effects.body_durability=.001f;game.weapon_proc(0,1,combined);
  assert(game.weapon==real_weapon && game.spell_effects.body_left==0);
  game.spell_effects.holy_ward=50;game.spell_effects.body_left=20;game.spell_effects.body_armor=true;
  game.spell_effects.body_effect=magic_rules::Viscosity;game.armor_magic.effect=0;game.armor_level=0;
  const float deferred_before=game.deferred_damage;
  assert(game.armor_proc(game.mobs[0],12)==7 && game.deferred_damage==deferred_before+2);
  arena(game,M_RAT,15,7);game.weapon=IT_CROSSBOW;game.weapon_magic.effect=magic_rules::Explosive;
  game.weapon_magic.durability=50;game.strength=30;game.mobs[0].sleep_t=100;game.add_item(IT_DART);
  game.throw_projectile(0,15,7);assert(game.weapon==IT_CROSSBOW && game.weapon_magic.durability<50);
  arena(game,M_RAT,15,7);game.magic_target=-4;game.ring1=IT_ART_CHAINS;game.ring1_magic.aux=5;
  game.artifact_action(-3,0);assert(game.targeting && game.magic_target==-1);
  game.targeting=game.artifact_targeting=false;game.magic_target=-3;game.add_item(IT_SCROLL_SIREN);game.use_item(0);
  assert(game.targeting && game.magic_target==-1 && game.book_scroll==IT_SCROLL_SIREN);
  // Foresight scans from the hero, through walls, rather than around its target.
  arena(game,M_RAT,14,7);game.mobs[0].ai=Mob::Sleeping;game.mobs[0].sleep_t=100;
  game.ring1=IT_ART_FORESIGHT;game.ring1_magic.aux=100;
  game.tiles[7][11]=T_WALL;game.tiles[7][13]=T_SECRET;
  Feature hidden;hidden.x=12;hidden.y=7;hidden.type=F_TRAP;game.features.push_back(hidden);
  game.items.push_back({14,7,IT_GOLD,5});game.items.push_back({14,7,IT_DAGGER});
  memset(game.seen,0,sizeof(game.seen));memset(game.vis,0,sizeof(game.vis));
  const auto expected_scan=foresight_rules::scan(7*MW+10,7*MW+15,MW,23,15,0,100);
  const int expected_exp=int(expected_scan.cells.size())+100+10+10+10;
  game.invis_t=10;const auto scan_turn=game.turns;
  game.artifact_action(-3,0,7*MW+15);
  assert(game.tiles[7][13]==T_DOOR && game.features[0].type==F_TRAP_FOUND);
  assert(game.ring1_level==1 && game.ring1_magic.experience==expected_exp-100);
  assert(game.ring1_magic.aux==90 && std::fabs(game.ring1_magic.stored-.655f)<.0001f);
  assert(game.invis_t==0 && game.turns==scan_turn+1 && !(game.seen[7][8]&2));
  assert(game.actor_effects(0).scry==4 && game.scry_heaps.size()==1 && game.scry_heaps[0].left==4);
  assert(game.vis[7][14] && (game.seen[7][14]&4));
  // Awareness follows the actor, while heap awareness stays at the scanned cell.
  game.mobs[0].x=19;game.recompute_fov();assert(game.vis[7][19] && game.vis[7][14]);
  assert(game.save(106) && restored.load(106));
  assert(restored.actor_effects(0).scry==4 && restored.scry_heaps.size()==1 && restored.vis[7][19]);
  FloorState scry_floor;game.capture_floor(scry_floor);game.visited_floors.push_back(scry_floor);
  game.floor=2;game.mobs.clear();game.mob_effects.clear();game.recompute_fov();
  assert(!game.vis[7][14]);game.tick_extended_effects();
  assert(game.visited_floors[0].effects[0].scry==3 && game.scry_heaps[0].left==3);
  assert(game.save(106) && restored.load(106));
  assert(restored.scry_heaps[0].depth==1 && !restored.vis[7][14]);
  game.apply_floor(game.visited_floors[0]);game.recompute_fov();
  assert(game.actor_effects(0).scry==3 && game.vis[7][19] && game.vis[7][14]);
  game.actor_effects(0).scry=0;game.scry_heaps[0].left=1;game.tick_extended_effects();game.recompute_fov();
  assert(game.scry_heaps.empty() && !game.vis[7][19] && !game.vis[7][14]);
  // Previously mapped terrain still grants first-time heap discovery XP.
  arena(game,M_RAT,20,7);game.mobs.clear();game.ring1=IT_ART_FORESIGHT;game.ring1_magic.aux=100;
  memset(game.seen,2,sizeof(game.seen));memset(game.vis,0,sizeof(game.vis));
  game.items.push_back({14,7,IT_GOLD,1});game.items.push_back({14,7,IT_DAGGER});
  game.artifact_action(-3,0,7*MW+15);assert(game.ring1_magic.experience==10);
  game.artifact_action(-3,0,7*MW+15);assert(game.ring1_magic.experience==10);
  // Minimum five charge can leave a fractional debt after a diagonal scan.
  arena(game,M_RAT,20,7);game.mobs.clear();game.ring1=IT_ART_FORESIGHT;game.ring1_magic.aux=5;
  game.artifact_action(-3,0,8*MW+11);
  assert(game.ring1_magic.aux==0 && game.ring1_magic.stored<0);
  assert(game.save(106) && restored.load(106));
  assert(restored.ring1_magic.stored==game.ring1_magic.stored);
  game.add_item(IT_ART_FORESIGHT);game.inv[0].magic=game.ring1_magic;
  assert(game.save(106) && restored.load(106) && restored.inv[0].magic.stored<0);
  const auto debt=game.ring1_magic.stored;game.artifact_charge(1);
  assert(std::fabs(game.ring1_magic.stored-(debt+2))<.0001f && game.ring1_magic.aux==0);
  const auto no_charge_turn=game.turns;game.artifact_action(-3,0);
  assert(!game.targeting && game.turns==no_charge_turn);
  game.ring1_magic.aux=100;game.artifact_action(-3,0,game.py*MW+game.px);
  assert(game.ring1_magic.aux==100 && game.turns==no_charge_turn);
  // Warnings require visible hidden traps and only fire on entry into danger.
  arena(game,M_RAT,20,7);game.mobs.clear();game.ring1=IT_ART_FORESIGHT;
  hidden.x=12;hidden.y=7;game.features.push_back(hidden);
  game.recompute_fov();game.tick_magic();assert(game.ring1_magic.consumed==1);
  game.message("sentinel");game.tick_magic();assert(std::string(game.msg)=="sentinel");
  game.tiles[7][11]=T_WALL;game.recompute_fov();game.tick_magic();assert(game.ring1_magic.consumed==0);
  game.tiles[7][11]=T_FLOOR;game.recompute_fov();game.tick_magic();assert(game.ring1_magic.consumed==1);
  game.spell_effects.magic_immunity=2;game.tick_magic();assert(game.ring1_magic.consumed==0);
  arena(game,M_RAT,14,7);game.hero=5;game.cleric_subclass=1;
  game.ring1=IT_ART_FORESIGHT;game.ring1_magic.aux=100;game.actor_effects(0).illuminated=true;
  game.mobs[0].ai=Mob::Sleeping;game.mobs[0].sleep_t=100;
  game.artifact_action(-3,0,7*MW+15);
  assert(game.mobs[0].hp==94 && !game.actor_effects(0).illuminated);
  game.new_run(0,42);assert(game.scry_heaps.empty());
  std::puts("Foresight passed: cone mapping, wall penetration, discovery XP, actor/heap awareness and expiry, fractional debt/save, low-charge/self-target guards and visibility-gated warning transitions.");
  // Plants preserve longer buffs, and their position-dependent buffs are independent.
  arena(game,M_RAT,15,7);
  const int hero_cell=game.py*MW+game.px;
  game.seed_effect(IT_SEED_STARFLOWER,hero_cell);assert(game.spell_effects.bless==30);
  game.spell_effects.bless=45;game.seed_effect(IT_SEED_STARFLOWER,hero_cell);assert(game.spell_effects.bless==45);
  game.hero_effects.poison=50;game.seed_effect(IT_SEED_SORROWMOSS,hero_cell);assert(game.hero_effects.poison==50);
  game.seed_effect(IT_SEED_SUNGRASS,hero_cell);game.px++;
  game.seed_effect(IT_SEED_EARTHROOT,hero_cell+1);game.tick_extended_effects();
  assert(game.spell_effects.healing==0 && game.spell_effects.earth==1000 && game.spell_effects.earth_anchor==hero_cell+1);
  game.px++;game.seed_effect(IT_SEED_SUNGRASS,hero_cell+2);game.tick_extended_effects();
  assert(game.spell_effects.earth==0 && game.spell_effects.healing>0 && game.spell_effects.anchor==hero_cell+2);
  game.spell_effects.healing=1;game.spell_effects.partial_heal=.9f;game.hp=900;game.tick_extended_effects();
  assert(game.hp==907 && game.spell_effects.healing==0); // final tick is not capped to remaining pool
  game.seed_effect(IT_SEED_EARTHROOT,hero_cell+2);game.spell_effects.earth=1;
  const int root_hp=game.hp;game.take_damage(10);
  assert(game.hp==root_hp-7 && game.spell_effects.earth==0); // final absorption still blocks 3
  game.hero_effects={};game.hero_effects.poison=20;game.hero_effects.poison_delay=3;
  game.hero_effects.bleeding=8;game.hero_effects.blind=10;game.hero_effects.burning=12;
  game.hero_effects.paralysis=6;game.hero_effects.roots=9;game.hero_effects.charm=7;game.hero_effects.chill=5;
  game.spell_effects.drowsy=5;game.spell_effects.vertigo=10;game.spell_effects.slow=9;
  game.spell_effects.magical_sleep=true;
  for(int type:{status_rules::Poison,status_rules::Cripple,status_rules::Weakness,status_rules::Vulnerable,status_rules::Hex})game.debuffs[type]=15;
  game.seed_effect(IT_SEED_MAGEROYAL,hero_cell+2);
  assert(game.hero_effects.poison==0 && game.hero_effects.poison_delay==0 && game.hero_effects.bleeding==0 && game.hero_effects.blind==0);
  assert(game.spell_effects.drowsy==0 && game.spell_effects.vertigo==0 && game.spell_effects.slow==0);
  assert(game.hero_effects.burning==12 && game.hero_effects.paralysis==6 && game.hero_effects.chill==5 && game.hero_effects.charm==7 && game.spell_effects.magical_sleep);
  assert(game.debuffs[status_rules::Poison]==0 && game.debuffs[status_rules::Cripple]==0 && game.debuffs[status_rules::Hex]==15);
  // Healing potion uses the same cure list, without clearing unrelated effects.
  game.hero_effects.poison=20;game.hero_effects.blind=10;game.hero_effects.bleeding=8;game.spell_effects.vertigo=10;
  game.add_item(IT_POTION_HEAL);game.use_item(0);
  assert(game.hero_effects.poison==0 && game.hero_effects.blind==0 && game.hero_effects.bleeding==0 && game.spell_effects.vertigo==0 && game.hero_effects.burning>0);
  arena(game,M_RAT,15,7);game.mobs[0].effects.blind=25;game.mobs[0].effects.chill=4;
  game.seed_effect(IT_SEED_BLINDWEED,7*MW+15);
  assert(game.mobs[0].effects.blind==25 && game.mobs[0].effects.chill==4 && game.actor_effects(0).cripple==10);
  game.mobs[0].effects.chill=game.mobs[0].effects.blind=0;game.mobs[0].ai=Mob::Hunting;
  assert(game.mob_turn(game.mobs[0])==2); // Cripple slows movement, not attack
  game.mobs[0].x=11;assert(game.mob_turn(game.mobs[0])==1);
  game.actor_effects(0).slow=7;game.actor_effects(0).weakness=9;game.actor_effects(0).vulnerable=8;
  game.actor_effects(0).drowsy=6;game.actor_effects(0).vertigo=5;
  game.mobs[0].effects.poison=10;game.mobs[0].effects.blind=10;game.mobs[0].effects.burning=12;
  game.seed_effect(IT_SEED_MAGEROYAL,7*MW+11);
  assert(game.actor_effects(0).cripple==0 && game.actor_effects(0).slow==0 && game.actor_effects(0).weakness==0 && game.actor_effects(0).vulnerable==0);
  assert(game.actor_effects(0).drowsy==0 && game.actor_effects(0).vertigo==0 && game.mobs[0].effects.poison==0 && game.mobs[0].effects.blind==0 && game.mobs[0].effects.burning==12);
  game.mobs[0].effects.paralysis=40;game.mobs[0].effects.chill=4;
  game.seed_effect(IT_SEED_ICECAP,7*MW+11);
  assert(game.mobs[0].effects.paralysis==40 && game.mobs[0].effects.chill==0 && game.mobs[0].effects.burning==0);
  game.seed_effect(IT_SEED_EARTHROOT,game.py*MW+game.px);game.actor_effects(0).cripple=10;
  assert(game.save(106) && restored.load(106));
  assert(restored.spell_effects.earth_anchor==game.spell_effects.earth_anchor && restored.actor_effects(0).cripple==10);
  std::puts("Plant corrections passed: cure list, duration preservation, independent anchors, final heal/block overshoot, Blindweed movement-only cripple, Icecap refresh and V15 persistence.");
  arena(game,M_RAT,20,7);game.mobs.clear();
  game.seed_effect(IT_SEED_SWIFTTHISTLE,game.py*MW+game.px);
  assert(game.spell_effects.bubble_active && game.spell_effects.time_bubble==7 && game.freeze_left==0);
  const int bubble_turn=game.turns,bubble_hunger=game.hunger;
  game.spend_action(1);assert(game.spell_effects.time_bubble==6 && game.turns==bubble_turn);
  game.spend_action(6);assert(game.spell_effects.bubble_active && game.spell_effects.time_bubble==0);
  game.spend_action(.0005f);assert(game.spell_effects.bubble_active);
  assert(game.save(106) && restored.load(106) && restored.spell_effects.time_bubble<0);
  game.spend_action(.001f);assert(!game.spell_effects.bubble_active && game.turns==bubble_turn && game.hunger==bubble_hunger);
  game.spend_action(1);assert(game.turns==bubble_turn+1);
  // Pending presses are unique, survive saving and resolve on detachment.
  arena(game,M_RAT,20,7);game.mobs.clear();
  Feature queued;queued.x=game.px;queued.y=game.py;queued.type=F_TRAP;queued.aux=TR_FIRE;
  game.features.push_back(queued);game.seed_effect(IT_SEED_SWIFTTHISTLE,game.py*MW+game.px);
  const int trapped_hp=game.hp;game.trigger_tile_features();game.trigger_tile_features();
  assert(game.features[0].type==F_TRAP_FOUND && game.bubble_presses.size()==1 && game.hp==trapped_hp);
  assert(game.save(106) && restored.load(106) && restored.bubble_presses==game.bubble_presses);
  game.end_time_bubble();restored.end_time_bubble();
  assert(game.features[0].type==F_TRAP_DISARMED && game.hp==trapped_hp-5 && game.hp==restored.hp);
  // A queued plant affects whoever occupies its cell when time resumes.
  arena(game,M_RAT,20,7);game.mobs.clear();
  queued.type=F_PLANT;queued.aux=IT_SEED_STARFLOWER;queued.x=game.px;queued.y=game.py;game.features.push_back(queued);
  game.seed_effect(IT_SEED_SWIFTTHISTLE,game.py*MW+game.px);game.trigger_tile_features();
  assert(game.spell_effects.bless==0 && game.features.size()==1);
  game.end_time_bubble();assert(game.spell_effects.bless==30 && game.features.empty());
  // Transition disarms pending cells but keeps remaining bubble time.
  queued.type=F_TRAP;queued.aux=TR_DART;game.features.push_back(queued);
  game.seed_effect(IT_SEED_SWIFTTHISTLE,game.py*MW+game.px);game.trigger_tile_features();
  game.end_time_bubble(true);
  assert(game.spell_effects.bubble_active && game.spell_effects.time_bubble==7 && game.bubble_presses.empty() && game.features[0].type==F_TRAP_DISARMED);
  // Sandals dispel the previous effect before creating the new bubble.
  game.ring1=IT_ART_SANDALS;game.ring1_level=3;game.ring1_magic.aux=100;game.ring1_magic.aux2=IT_SEED_SWIFTTHISTLE;
  game.artifact_action(-3,0,game.py*MW+game.px);
  assert(game.spell_effects.bubble_active && game.spell_effects.time_bubble==6 && game.ring1_magic.aux==80);
  // Offensive actions break the bubble before their ordinary time cost.
  arena(game,M_RAT,11,7);game.seed_effect(IT_SEED_SWIFTTHISTLE,game.py*MW+game.px);
  game.attack(0,false);assert(!game.spell_effects.bubble_active);
  game.seed_effect(IT_SEED_SWIFTTHISTLE,game.py*MW+game.px);game.add_item(IT_KNIFE);game.throw_projectile(0,11,7);
  assert(!game.spell_effects.bubble_active && game.turns>0);
  arena(game,M_RAT,15,7);game.seed_effect(IT_SEED_SWIFTTHISTLE,game.py*MW+game.px);
  game.add_item(IT_WAND_MAGIC);game.cast_wand(0,15,7);assert(!game.spell_effects.bubble_active);
  arena(game,M_RAT,20,7);game.seed_effect(IT_SEED_SWIFTTHISTLE,7*MW+20);
  assert(game.actor_effects(0).bubble_active && game.actor_effects(0).haste==0 && !game.spell_effects.bubble_active);
  game.spend_action(.1f);assert(game.mobs[0].x==11 && !game.actor_effects(0).bubble_active);
  std::puts("Time bubble passed: seven-point reset, zero/tolerance boundary, full-action expiry, queued presses/save/disarm, sandals order, offensive detachment and monster-local time.");
  // Hourglass charges are debited as time is used; early cancellation keeps the rest.
  arena(game,M_RAT,20,7);game.mobs.clear();game.ring1=IT_ART_HOURGLASS;game.ring1_magic.aux=5;
  const int hourglass_turn=game.turns;
  game.artifact_action(-3,5);assert(game.ring1_magic.aux==4 && game.freeze_left==2 && game.spell_effects.hourglass_active);
  game.spend_action(2);assert(game.ring1_magic.aux==4 && game.freeze_left==0 && game.spell_effects.hourglass_active);
  game.spend_action(.0005f);assert(game.ring1_magic.aux==4 && game.freeze_left<0 && game.spell_effects.hourglass_active);
  assert(game.save(106) && restored.load(106) && restored.freeze_left==game.freeze_left);
  game.spend_action(.001f);assert(game.ring1_magic.aux==3 && game.freeze_left>1.99f && game.turns==hourglass_turn);
  game.artifact_action(-3,0);assert(game.ring1_magic.aux==3 && !game.spell_effects.hourglass_active && game.freeze_left==0);
  // A single charge freezes the complete action which crosses expiry.
  game.ring1_magic.aux=1;game.artifact_action(-3,5);game.spend_action(3);
  assert(game.ring1_magic.aux==0 && !game.spell_effects.hourglass_active && game.turns==hourglass_turn);
  // Shared pending-press path resolves on manual cancellation and offensive dispel.
  game.ring1_magic.aux=5;game.artifact_action(-3,5);
  queued.x=game.px;queued.y=game.py;queued.type=F_TRAP;queued.aux=TR_FIRE;game.features.push_back(queued);
  const int frozen_hp=game.hp;game.trigger_tile_features();game.trigger_tile_features();
  assert(game.hp==frozen_hp && game.bubble_presses.size()==1);
  assert(game.save(106) && restored.load(106) && restored.spell_effects.hourglass_active);
  game.artifact_action(-3,0);assert(game.hp==frozen_hp-5 && game.features[0].type==F_TRAP_DISARMED && game.bubble_presses.empty() && game.ring1_magic.aux==4);
  game.artifact_action(-3,5);game.dispel_action();assert(!game.spell_effects.hourglass_active && game.ring1_magic.aux==3);
  // Hourglass takes precedence when both time effects are present.
  game.artifact_action(-3,5);game.seed_effect(IT_SEED_SWIFTTHISTLE,game.py*MW+game.px);
  game.spend_action(1);assert(game.freeze_left==1 && game.spell_effects.time_bubble==7);
  game.end_hourglass();game.spend_action(1);assert(game.spell_effects.time_bubble==6);
  game.end_time_bubble();game.ring1_magic.aux=2;game.artifact_action(-3,5);
  game.artifact_action(-3,2);assert(game.ring1==IT_NONE && !game.spell_effects.hourglass_active && game.freeze_left==0);
  std::puts("Hourglass passed: incremental charge, early cancellation, fractional debt save, whole-action expiry, queued presses, offensive dispel, bubble priority and unequip.");
  // Stasis advances existing buffs and world time; protection is not a buff pause.
  arena(game,M_RAT,20,7);game.mobs.clear();game.ring1=IT_ART_HOURGLASS;game.ring1_magic.aux=5;
  game.hp=900;game.hunger=100;game.healing_left=100;
  game.hero_effects.poison=20;game.hero_effects.burning=20;game.hero_effects.bleeding=8;
  game.spell_effects.bless=30;game.spell_effects.vertigo=20;game.haste_t=20;
  const int stasis_turn=game.turns;
  game.artifact_action(-3,4);
  assert(!game.time_stasis && game.turns==stasis_turn+10 && game.ring1_magic.aux==3);
  assert(game.hunger==100 && game.hp>900 && game.healing_left<100);
  assert(game.spell_effects.bless==20 && game.spell_effects.vertigo==10 && game.haste_t==10);
  assert(game.hero_effects.poison==10 && game.hero_effects.burning==10 && game.hero_effects.bleeding<8);
  // No food is created below zero hunger; starving heroes receive no compensation.
  arena(game,M_RAT,20,7);game.mobs.clear();game.ring1=IT_ART_HOURGLASS;game.ring1_magic.aux=1;game.hunger=2;
  game.artifact_action(-3,4);assert(game.turns==5 && game.hunger==5 && game.ring1_magic.aux==0);
  arena(game,M_RAT,20,7);game.mobs.clear();game.ring1=IT_ART_HOURGLASS;game.ring1_magic.aux=2;
  game.hunger=hero_rules::starving;const int starving_hp=game.hp;
  game.artifact_action(-3,4);assert(game.hunger==hero_rules::starving && game.hp==starving_hp);
  // Monster status timers also continue while the hero is protected.
  arena(game,M_RAT,20,7);game.ring1=IT_ART_HOURGLASS;game.ring1_magic.aux=2;
  game.mobs[0].sleep_t=30;game.mobs[0].effects.poison=20;
  game.artifact_action(-3,4);
  assert(game.mobs[0].sleep_t==0 && game.mobs[0].effects.poison==10 && game.mobs[0].hp<100);
  game.time_stasis=true;const int protected_hp=game.hp;
  game.armor_magic.effect=magic_rules::AntiEntropy;
  const auto protected_burning=game.hero_effects.burning;
  game.take_damage(10000);assert(game.hp==protected_hp);
  assert(game.armor_proc(game.mobs[0],100)==0 && game.hero_effects.burning==protected_burning);
  game.time_stasis=false;
  std::puts("Stasis passed: hunger compensation, zero/starving limits, continuing healing/status clocks, damage immunity and monster status progression.");
  arena(game,M_RAT,15,7);game.tiles[7][15]=T_WATER;
  game.mobs[0].effects.paralysis=7;game.mobs[0].effects.burning=8;game.mobs[0].effects.chill=4;
  game.freeze_actor(0);assert(game.actor_effects(0).frost==30 && game.mobs[0].effects.paralysis==7 && game.mobs[0].effects.burning==0 && game.mobs[0].effects.chill==0);
  assert(game.mob_turn(game.mobs[0])==1 && game.mobs[0].x==15);
  assert(game.save(106) && restored.load(106) && restored.actor_effects(0).frost==30);
  game.damage_mob(0,0);assert(game.actor_effects(0).frost==0 && game.mobs[0].effects.chill==5 && game.mobs[0].effects.paralysis==7);
  game.tiles[game.py][game.px]=T_WATER;game.freeze_actor(-1);game.hero_effects.paralysis=6;
  const int frozen_x=game.px;game.move_player(1,0);assert(game.px==frozen_x && game.spell_effects.frost>0);
  game.take_damage(0);assert(game.spell_effects.frost==0 && game.hero_effects.chill==5 && game.hero_effects.paralysis>0);
  game.hero_effects.paralysis=0;game.hero_effects.chill=2;
  const float chilled_move=game.movement_cost(),chilled_attack=game.melee_delay();
  game.hero_effects.chill=0;assert(same_clock(chilled_move,game.movement_cost()/ .8f) && same_clock(chilled_attack,game.melee_delay()/ .8f));
  game.freeze_actor(-1);game.spell_effects.frost=1;game.tick_extended_effects();assert(game.spell_effects.frost==0 && game.hero_effects.chill==5);
  game.time_stasis=true;game.freeze_actor(-1);assert(game.spell_effects.frost==0);game.time_stasis=false;
  std::puts("Frost passed: independent paralysis, water duration/residual chill, damage/zero-damage thaw, movement blocking, expiry, variable chill speed and save/load.");
  std::puts("V15 mechanics passed: projectile wear/recovery/pinning, selected seed feeding, plant anchors, shop theft, rose equipment, tome gates, spellbook and extended saves.");
  std::remove("sdmc:/3ds/spd/slot106.bin");
  std::remove("sdmc:/3ds/spd/slot107.bin");
  std::remove("sdmc:/3ds/spd/slot108.bin");
  std::puts("Runtime actor tests passed: fractional turns, tie priority, movement vs attack speed, status time, healing priority, deterministic save continuation and truncated-save rejection.");
  std::puts("Runtime item tests passed: cursed equipment lock, selected removal/identification/upgrade, cancel, ring penalty, drop/pickup/save state and V11 migration.");
  std::puts("Runtime enemy tests passed: blocked bolts, ranged magic bypasses armor, Scorpio retreats, shaman variants, poison, degradation, healing cure list and saved statuses.");
  std::puts("Eye tests passed: two-turn charge, charged damage reduction, saved aim, dodge fixed beam while invisible, cooldown and deterministic continuation.");
  std::puts("Gas tests passed: trap concentration, depth damage, diffusion, deterministic saved continuation and new-run reset.");
  std::puts("Floor tests passed: return geometry/entities, per-floor keys, saved visited floors, >48 loot entries, floor 26 geometry/Amulet, return and surface victory.");
  std::puts("Abilities/ascent tests passed: necromancer summon ownership, healing, death/XP and save continuation; brute rage; guard chains; monk focus; ascent stacks, damage, boss rest and persistence.");
  std::puts("Goo/Slime tests passed: two-stage pump, reach, water heal and Slime nonlinear damage reduction.");
  std::puts("Failed-save test passed: invalid new snapshot leaves the previous save byte-for-byte intact.");
  std::puts("Wand recharge tests passed: missing-charge formula, fractional persistence, deterministic continuation and drop/pickup state.");
  std::puts("Equipment checks passed: Furor attack delay, Might HT, Tenacity damage, full-bag pickup retention, wand transmutation charge preservation and armor rejection.");
  std::puts("Augmentation tests passed: target selection, attribute choice without upgrading level, delays, consumption and save persistence.");
  std::puts("Identification tests passed: use budget, XP replenishment, equipped ring progress and saved counters.");
}
