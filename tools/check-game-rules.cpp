#include "game.h"
#include "actor_rules.h"
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
  const auto archive_start=corrupt.find("FLR1");assert(archive_start!=std::string::npos);
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
  // A V12 prefix has the identical floor/item layout and imports old knowledge.
  {
    std::ifstream source("sdmc:/3ds/spd/slot106.bin",std::ios::binary);
    std::string old{std::istreambuf_iterator<char>(source),{}};
    const uint16_t version=12;std::memcpy(old.data()+4,&version,sizeof(version));
    auto marker=old.rfind("DID1");assert(marker!=std::string::npos);old.resize(marker);
    std::ofstream fixture("sdmc:/3ds/spd/slot108.bin",std::ios::binary);
    fixture.write(old.data(),old.size());fixture.close();
    assert(restored.load(108) && restored.identity_known(IT_POTION_STR));
    SlotMeta meta;assert(Game::slot_meta(108,meta));
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
