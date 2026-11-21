#pragma once

#include "common.h"
#include "items.h"
#include "content.h"

#define MW 41
#define MH 41
#define BAG_CAP 25

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


enum FeatureType : uint8_t {
  F_CHEST,
  F_CHEST_OPEN,
  F_CHEST_LOCKED,
  F_WELL,
  F_WELL_USED,
  F_BOOKSHELF,
  F_BOOKSHELF_USED,
  F_ALCHEMY,
  F_ALCHEMY_USED,
  F_STATUE,
  F_STATUE_USED,
  F_TOMB,
  F_TOMB_USED,
  F_PLANT,
  F_SIGN,
  F_BARRICADE,
  F_TRAP,
  F_TRAP_FOUND,
  F_TOKEN,
};

struct Feature {
  int x = 0, y = 0;
  uint8_t type = F_CHEST;
  uint8_t state = 0;
  int16_t aux = 0;
  uint8_t loot_id = 0;
  uint8_t loot_level = 0;
  int16_t loot_gold = 0;
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
  int16_t level = 0;  // upgrade level, or coin amount for IT_GOLD
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
  int16_t x, y, level;
  uint8_t id;
};
struct ItemDisk {
  uint8_t id, count, level, charges;
};

struct FeatureDisk {
  int16_t x, y, aux, loot_gold;
  uint8_t type, state, loot_id, loot_level;
};

struct SearchFx {
  float x, y;      // tile coords
  float delay;     // seconds before the shrink starts
  float alpha;     // 0.8 -> 0
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
  uint8_t quest;
  uint8_t tiles[MH][MW];
  uint8_t seen[MH][MW];
  uint16_t mob_n;
  MobDisk mobs[48];
  uint16_t item_n;
  FloorItemDisk items[48];
  uint8_t inv_n;
  ItemDisk inv[BAG_CAP];
  uint8_t feature_n;
  FeatureDisk features[64];
  uint8_t boss_dead[5];
  uint8_t tokens, token_quest, smith_used, imp_state;
  uint8_t shop_n, shop_floor;
  ItemDisk shop_items[8];
  int16_t shop_prices[8];
  uint8_t shop_sold[8];
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
  std::vector<Feature> features;
  uint8_t boss_dead[5]{};
  uint8_t tokens = 0;
  uint8_t token_quest = 0;
  uint8_t smith_used = 0;
  uint8_t imp_state = 0;
  bool shop_open = false;
  int shop_sel = 0, shop_n = 0, shop_floor = -1;
  Item shop_items[8];
  int shop_prices[8];
  bool shop_sold[8];
  Item inv[BAG_CAP];
  int inv_n = 0;
  uint8_t weapon = IT_DAGGER, weapon_level = 0;
  uint8_t armor = IT_CLOTH_ARMOR, armor_level = 0;
  uint8_t quest = 0;     // 0 none, 1 accepted, 2 complete
  int cursor_x = 1, cursor_y = 1;
  bool targeting = false;
  int target_item = -1;
  int camx = 0, camy = 0;
  int wait_hold = 0;
  float pvx = 0, pvy = 0;   // player visual position
  std::vector<SearchFx> search_fx;
  int search_ms = 0;
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
  bool demo_lock = false;    // debug: ignore input

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
  int region() const { return region_of_floor(floor); }
  bool boss_alive() const;
  int feature_at(int x, int y) const;
  bool feature_blocking(int x, int y) const;
  void use_feature(int idx);
  void trigger_tile_features();
  void open_shop();
  void shop_buy(int i);
  void talk_npc(int idx);
  void give_loot(int tier, bool chest);
  int weapon_bonus() const;
  int armor_bonus() const;
  int hero_atk() const;
  int hero_def() const;
  void recharge_wands();
  void search();
  bool drop_item(int idx);
  void cast_or_throw(int cx, int cy);
  SlotMeta meta() const;
  bool save(int slot) const;
  bool load(int slot);
  static bool slot_meta(int slot, SlotMeta& out);
};
