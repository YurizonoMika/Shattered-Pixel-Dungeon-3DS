#pragma once

#include "common.h"
#include "items.h"

#define MW 41
#define MH 41

enum TileType : uint8_t {
  T_WALL = 0,
  T_FLOOR,
  T_DOOR,       // closed
  T_SECRET,
  T_STAIRS,
  T_WATER,
  T_GRASS,
  T_DOOR_OPEN,  // walkable, still drawn as a door frame
};

enum MobType : uint8_t {
  M_RAT = 0,
  M_GNOLL,
  M_CRAB,
  M_ALBINO,
  M_BOSS,
  M_NPC,
};

struct Mob {
  int x = 0, y = 0;
  float vx = 0, vy = 0;  // visual position (tile units)
  int anim_ms = 0;
  uint8_t type = M_RAT;
  int hp = 1, maxhp = 1, atk = 1, def = 0;
  bool alive = true;
};

struct FloorItem {
  int x = 0, y = 0;
  uint8_t id = IT_NONE;
  uint8_t level = 0;
};

struct Item {
  uint8_t id = IT_NONE;
  uint8_t count = 1;
  uint8_t level = 0;    // upgrades for weapons/armor, unused otherwise
  uint8_t charges = 0;  // wands
};

// Binary save layout. Fixed-size on purpose: 3DS homebrew, no protobuf.
struct MobDisk {
  int16_t x, y, hp, maxhp, atk, def;
  uint8_t type, alive;
};
struct FloorItemDisk {
  int16_t x, y;
  uint8_t id, level;
};
struct ItemDisk {
  uint8_t id, count, level, charges;
};

struct GameDisk {
  uint32_t magic;
  uint16_t version;
  uint16_t bytes;
  SlotMeta meta;
  uint32_t seed, rng;
  int16_t floor, hero;
  int16_t px, py, hp, maxhp, level, xp, xpnext;
  int16_t keys, gold, turns, shield;
  uint8_t weapon, weapon_level, armor, armor_level;
  uint8_t quest, boss_dead;
  uint8_t tiles[MH][MW];
  uint8_t seen[MH][MW];
  uint16_t mob_n;
  MobDisk mobs[48];
  uint16_t item_n;
  FloorItemDisk items[48];
  uint8_t inv_n;
  ItemDisk inv[8];
};

struct Game {
  uint32_t seed = 1, rng = 1;
  int floor = 1;
  int hero = 0;
  int px = 1, py = 1;
  int hp = 20, maxhp = 20, level = 1, xp = 0, xpnext = 10;
  int keys = 0, gold = 0, shield = 0;
  int turns = 0;
  uint8_t tiles[MH][MW]{};
  bool seen[MH][MW]{};
  bool vis[MH][MW]{};
  std::vector<Mob> mobs;
  std::vector<FloorItem> items;
  Item inv[8];
  int inv_n = 0;
  uint8_t weapon = IT_DAGGER, weapon_level = 0;
  uint8_t armor = IT_CLOTH_ARMOR, armor_level = 0;
  uint8_t quest = 0;     // 0 none, 1 accepted, 2 complete
  uint8_t boss_dead = 0;
  int cursor_x = 1, cursor_y = 1;
  bool targeting = false;
  int target_item = -1;
  int camx = 0, camy = 0;
  int wait_hold = 0;
  float pvx = 0, pvy = 0;   // player visual position
  int panim_ms = 0;
  int16_t path_x[32]{}, path_y[32]{};
  int path_n = 0, path_i = 0;
  int fx_x = -1, fx_y = -1, fx_t = 0;

  bool paused = false, backpack = false;
  bool dialog = false;
  int dlg_type = 0;  // 0 stairs, 1 npc, 2 death, 3 victory
  char dlg_name[32]{};
  char dlg_text[256]{};
  char dlg_opt[4][40]{};
  int dlg_optn = 0;
  bool dead = false;
  bool won = false;
  bool freeze_anim = false;  // debug: hold mid-walk pose

  char msg[128]{};
  int msg_timer = 0;

  void new_run(int hero_class, uint32_t s);
  void gen_level();
  void recompute_fov();
  bool los(int x0, int y0, int x1, int y1) const;
  bool walkable(int x, int y) const;
  bool blocks_sight(int x, int y) const;
  int mob_at(int x, int y) const;
  int item_at(int x, int y) const;
  void move_player(int dx, int dy);
  void act_at(int x, int y, bool interact);
  void interact_at(int x, int y);
  void attack(int mob, bool ranged, int bonus = 0);
  void damage_mob(int idx, int raw);
  void advance_turn();
  void tick(int dt_ms);
  void clear_path();
  void wait_turns(int n);
  void mob_turn(Mob& m);
  void message(const char* s);
  void use_item(int idx);
  void equip_item(int idx);
  void cast_wand(int idx, int tx, int ty);
  void pick_item(int idx);
  void descend();
  void open_dialog(int type, const char* name, const char* text,
                   const char* o0 = nullptr, const char* o1 = nullptr,
                   const char* o2 = nullptr, const char* o3 = nullptr);
  void dialog_choose(int i);
  void add_item(uint8_t id, int count = 1, int level = 0);
  bool remove_item(int idx, int count = 1);
  int weapon_bonus() const;
  int armor_bonus() const;
  int hero_atk() const;
  int hero_def() const;
  void recharge_wands();
  void cast_or_throw(int cx, int cy);
  SlotMeta meta() const;
  bool save(int slot) const;
  bool load(int slot);
  static bool slot_meta(int slot, SlotMeta& out);
};
