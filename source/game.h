#pragma once

#include "common.h"
#include "items.h"
#include "content.h"

constexpr int MW = 64, MH = 64;
constexpr int LEGACY_MAP_SIZE = 41;
#define BAG_CAP 25
#define BAG_MAX 40

enum TileType : uint8_t {
  T_WALL = 0,
  T_FLOOR,
  T_DOOR,       // closed
  T_SECRET,
  T_STAIRS,
  T_WATER,
  T_GRASS,
  T_DOOR_OPEN,  // walkable, still drawn as a door frame
  T_DOOR_LOCKED,// needs a key
  T_CHASM,      // visible across, not traversable until floor returns exist
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
  F_CHEST_MIMIC,
  F_REMAINS,
  F_MIMIC_LOOT, // hidden payload; aux is the index of the active mimic
  F_TRAP_DISARMED,
  F_CHEST_CRYSTAL,
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

// Feature::aux stores the trap type. Keep the two older IDs readable.
enum TrapKind : int16_t {
  TR_LEGACY, TR_DART, TR_TOXIC, TR_ALARM, TR_CHILL, TR_FIRE, TR_SHOCK, TR_COUNT
};

inline bool trap_feature(uint8_t type) {
  return type == F_TRAP || type == F_TRAP_FOUND || type == F_TRAP_DISARMED;
}

inline TrapKind trap_kind(const Feature& f) {
  return f.aux >= 0 && f.aux < TR_COUNT ? TrapKind(f.aux) : TR_LEGACY;
}

const char* trap_name(TrapKind kind);
constexpr int TRAP_FX_MS = 760;
constexpr int TOXIC_TURNS = 6;
constexpr int TOXIC_RADIUS = 2;

struct TrapFx {
  int x, y, age;
  TrapKind kind;
};

enum class CombatStyle : uint8_t { Melee, Arcane, Fire, Frost, Lightning, Throw, Blast };
enum class DamageKind : uint8_t { Direct, Physical };
constexpr int DEATH_ANIM_MS = 460;
constexpr int COMBAT_MISS = -2;

struct CombatPose {
  int age = -1, hit_age = 1000;
  int death_age = DEATH_ANIM_MS;
  int dx = 0, dy = 0;
  CombatStyle style = CombatStyle::Melee;
};

struct CombatFx {
  int x, y, tx, ty, age, delay, damage;
  CombatStyle style;
  uint8_t item = IT_NONE;
};

struct Mob {
  enum Ai : uint8_t { Sleeping, Wandering, Hunting };
  int x = 0, y = 0;
  float vx = 0, vy = 0;  // visual position (tile units)
  int anim_ms = 0;
  uint8_t type = M_RAT;
  int hp = 1, maxhp = 1, atk = 1, def = 0;
  bool alive = true;
  uint8_t sleep_t = 0, terror_t = 0;
  Ai ai = Wandering;
  int target = -1;
  CombatPose combat;
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
  uint8_t type, alive, sleep_t, terror_t;
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
  uint8_t ring1, ring1_level, ring2, ring2_level;
  uint8_t haste_t, invis_t, might_t, levit_t, mind_t, rage_t;
  uint8_t quest;
  uint8_t tiles[LEGACY_MAP_SIZE][LEGACY_MAP_SIZE];
  uint8_t seen[LEGACY_MAP_SIZE][LEGACY_MAP_SIZE];
  uint16_t mob_n;
  MobDisk mobs[48];
  uint16_t item_n;
  FloorItemDisk items[48];
  uint8_t inv_n;
  ItemDisk inv[BAG_MAX];
  uint8_t feature_n;
  FeatureDisk features[64];
  uint8_t boss_dead[5];
  uint8_t tokens, token_quest, smith_used, imp_state;
  uint8_t shop_n, shop_floor;
  ItemDisk shop_items[20];
  int16_t shop_prices[20];
  uint8_t shop_sold[20];
  ItemDisk buyback_items[3];
  int16_t buyback_prices[3];
  uint8_t buyback_n;
  uint8_t ghost_floor, ghost_state, ghost_boss_dead;
  uint8_t wand_floor, wand_state, have_dust;
  uint8_t smith_floor, smith_state, ore_count;
  uint8_t imp_floor, imp_kills, imp_target;
  uint8_t reward1, reward1_level, reward2, reward2_level;
  uint8_t bag_potion, bag_scroll, bag_wand;
  uint8_t shop_warned[25], shop_gone[25];
  uint8_t dust_timer;
  uint8_t talents[4], talent_tier;
};

struct Game {
  uint32_t seed = 1, rng = 1;
  int floor = 1;
  int hero = 0;
  int px = 1, py = 1;
  int hp = 20, maxhp = 20, level = 1, xp = 0, xpnext = 10;
  int keys = 0, gold = 0, shield = 0;
  int golden_keys = 0;
  int crystal_keys = 0;
  bool legacy_keys = false;
  bool loot_second_deck = false;
  uint8_t loot_remaining[10]{};
  int map_width = LEGACY_MAP_SIZE, map_height = LEGACY_MAP_SIZE;
  bool in_map(int x, int y) const {
    return x >= 0 && y >= 0 && x < map_width && y < map_height;
  }
  int turns = 0;
  uint8_t tiles[MH][MW]{};
  uint8_t seen[MH][MW]{};
  bool vis[MH][MW]{};
  std::vector<Mob> mobs;
  std::vector<FloorItem> items;
  std::vector<Feature> features;
  uint8_t boss_dead[5]{};
  uint8_t tokens = 0;
  uint8_t token_quest = 0;
  uint8_t smith_used = 0;
  uint8_t imp_state = 0;
  bool bag_potion = false, bag_scroll = false, bag_wand = false;
  bool shop_warned[25]{}, shop_gone[25]{};
  int dust_timer = 0;
  int respawn_t = 0;  // countdown in thirds of a turn
  int mob_cap = 0;
  struct SpawnRoom { uint8_t x, y, w, h; };
  std::vector<SpawnRoom> spawn_rooms;
  std::vector<uint8_t> spawn_deck;
  uint8_t next_spawn_type();
  bool try_respawn();
  int room_spawn_limit(const SpawnRoom& room) const;
  int room_spawn_count(const SpawnRoom& room) const;
  int room_count = 0;
  uint8_t layout_builder = 0; // debug: 0 legacy, 1 loop, 2 figure eight
  uint8_t talents[4]{}, talent_tier = 0;
  bool alchemy_open = false;
  bool icon_test = false;
  uint8_t fov_radius = 8;
  float zoom = 1.0f;
  float zoom_target = 1.0f;
  // Deterministic per-run plan (rebuilt from the seed, not saved).
  uint8_t plan_special[25][4]{};
  uint8_t plan_special_n[25]{};
  uint8_t plan_feeling[25]{};
  uint8_t plan_secret[25]{};
  uint8_t plan_guaranteed[25]{};  // bit 0 strength, bit 1 upgrade
  int alchemy_sel = 0;
  bool transmute_open = false;
  int transmute_sel = 0;
  int transmute_page = 0;
  bool shop_open = false;
  int shop_sel = 0, shop_n = 0, shop_floor = -1;
  int shop_mode = 0, shop_page = 0;  // 0 buy, 1 sell, 2 buyback
  Item shop_items[20];
  int shop_prices[20];
  bool shop_sold[20];
  Item buyback_items[3];
  int buyback_prices[3];
  int buyback_n = 0;
  uint8_t ghost_floor = 0, ghost_state = 0, ghost_boss_dead = 0;
  uint8_t wand_floor = 0, wand_state = 0, have_dust = 0;
  uint8_t smith_floor = 0, smith_state = 0, ore_count = 0;
  uint8_t imp_floor = 0, imp_kills = 0, imp_target = 0;
  uint8_t reward1 = 0, reward1_level = 0, reward2 = 0, reward2_level = 0;
  Item inv[BAG_MAX];
  int inv_n = 0;
  uint8_t weapon = IT_DAGGER, weapon_level = 0;
  uint8_t armor = IT_CLOTH_ARMOR, armor_level = 0;
  uint8_t ring1 = IT_NONE, ring1_level = 0;
  uint8_t ring2 = IT_NONE, ring2_level = 0;
  uint8_t haste_t = 0, invis_t = 0, might_t = 0, levit_t = 0, mind_t = 0, rage_t = 0;
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
  int path_goal = -1;
  CombatPose hero_combat;
  std::vector<CombatFx> combat_fx;
  std::vector<TrapFx> trap_fx;
  int trap_notice_ms = 0;
  TrapKind trap_notice = TR_LEGACY;
  int combat_delay = 0;
  void combat_strike(CombatPose& pose, int x, int y, int tx, int ty,
                     CombatStyle style, uint8_t item = IT_NONE);
  void combat_hit(CombatPose& pose, int x, int y, int damage);
  void take_damage(int damage);
  bool hero_hits(Mob& mob, bool ranged);

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
  int water_anim_ms = 0;
  bool demo_lock = false;    // debug: ignore input

  char msg[128]{};
  int msg_timer = 0;

  void new_run(int hero_class, uint32_t s);
  void gen_level();
  void build_run_plan();
  void debug_dump();
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
  void damage_mob(int idx, int raw, DamageKind kind = DamageKind::Direct);
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
  void close_door_if_clear(int x, int y);
  bool remove_item(int idx, int count = 1);
  int region() const { return region_of_floor(floor); }
  bool boss_alive() const;
  int feature_at(int x, int y) const;
  bool feature_blocking(int x, int y) const;
  void use_feature(int idx);
  void trigger_tile_features();
  void trigger_trap(int idx, Mob* who = nullptr);
  bool in_toxic_cloud(const Feature& trap, int x, int y) const;
  void tick_trap_clouds();
  void open_shop();
  void shop_buy(int i);
  void shop_sell(int i);
  void shop_buyback(int i);
  void harm_shopkeeper();
  void alchemy_brew(int recipe);
  bool can_transmute(int idx) const;
  void transmute_choose(int idx);
  void check_talents();
  int shop_sell_price(uint8_t id, int count, int level) const;
  void spawn_quest_miniboss();
  void talk_npc(int idx);
  void give_loot(int tier, bool chest);
  int bag_capacity() const;
  int weapon_bonus() const;
  int armor_bonus() const;
  int ring_atk() const;
  bool ring_has(uint8_t id) const;
  void equip_ring(int idx);
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
  static bool delete_slot(int slot);
};
