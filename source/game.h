#pragma once

#include "common.h"
#include "items.h"
#include "content.h"
#include "status_rules.h"
#include "ballistica.h"
#include "blob_rules.h"
#include "identification_rules.h"
#include "magic_rules.h"
#include "mechanics_rules.h"
#include "cleric_rules.h"

constexpr int MW = 64, MH = 64;
constexpr int LEGACY_MAP_SIZE = 41;
#define BAG_CAP 25
#define BAG_MAX 40

enum ItemFlags : uint8_t {
  IF_IDENTIFIED = 1, IF_CURSED = 2, IF_CURSE_KNOWN = 4,
  IF_KNOWN = IF_IDENTIFIED | IF_CURSE_KNOWN
};

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
  T_STAIRS_UP,
  T_HIGH_GRASS,
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
  F_KEY_WALL,
};

struct Feature {
  int x = 0, y = 0;
  uint8_t type = F_CHEST;
  uint8_t state = 0;
  int16_t aux = 0;
  uint8_t loot_id = 0;
  uint8_t loot_level = 0;
  int16_t loot_gold = 0;
  uint8_t loot_flags = IF_KNOWN;
  uint8_t loot_effect = 0;
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
  float next_action = 0.f;
  uint8_t variant = 0;
  bool beam_charged = false;
  uint8_t beam_cooldown = 0;
  int beam_target = -1;
  bool chains_used=false,focused=false;
  float focus_cooldown=0.f;
  int minion=-1,summoning_pos=-1,owner=-1;
  bool first_summon=true,has_raged=false;
  int rage_shield=0;
  uint8_t adrenaline=0;
  uint8_t pumped_up=0;
  CombatPose combat;
  magic_rules::Effects effects;
  bool lucky=false;
  bool rose_ally=false;
};

struct FloorItem {
  int x = 0, y = 0;
  uint8_t id = IT_NONE;
  int16_t level = 0;  // upgrade level, or coin amount for IT_GOLD
  uint8_t flags = IF_KNOWN;
  uint8_t charges=0;
  float partial_charge=0.f;
  uint8_t augment=0;
  Identification identification{0.f,item_def(id).kind==K_ARMOR || item_def(id).kind==K_WAND ? 5.f : 10.f};
  magic_rules::State magic;
};

struct Item {
  uint8_t id = IT_NONE;
  uint8_t count = 1;
  uint8_t level = 0;    // upgrades for weapons/armor, unused otherwise
  uint8_t charges = 0;  // wands
  uint8_t flags = IF_KNOWN;
  float partial_charge=0.f;
  uint8_t augment=0;
  Identification identification{0.f,item_def(id).kind==K_ARMOR || item_def(id).kind==K_WAND ? 5.f : 10.f};
  magic_rules::State magic;
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

struct FloorState {
  int depth=0,width=0,height=0,entrance=-1;
  uint8_t tiles[MH][MW]{},seen[MH][MW]{};
  std::vector<Mob> mobs;
  std::vector<FloorItem> items;
  std::vector<Feature> features;
  blob_rules::Gas<MW,MH> gas;
  int keys=0,golden_keys=0,crystal_keys=0,respawn=0,mob_cap=0;
  bool legacy_keys=false;
  struct Room { uint8_t x,y,w,h; };
  std::vector<Room> rooms;
  std::vector<uint8_t> weights,deck;
  int shop_n=0,shop_floor=-1;
  Item shop_items[20]{};
  int shop_prices[20]{};
  bool shop_sold[20]{};
  std::vector<mechanics_rules::Actor> effects;
};

struct Game {
  std::vector<Mob> pending_swarms;
  uint32_t seed = 1, rng = 1;
  int floor = 1;
  int hero = 0;
  int px = 1, py = 1;
  int hp = 20, maxhp = 20, level = 1, xp = 0, xpnext = 10;
  int strength = 10;
  int hp_boost = 0;
  int hunger = 0, starvation_damage = 0, regen_ticks = 0, healing_left = 0;
  int blessed_t = 0;
  float action_remainder = 0.f;
  // Actor clocks share a whole-turn origin; action_remainder is hero time.
  // At a tie, hero acts first.
  float health_time = 0.f, world_time = 0.f, buff_time = 0.f;
  float actor_time = 0.f; // current event time; transient, restored from hero time
  uint8_t debuffs[status_rules::Count]{};
  int keys = 0, gold = 0, shield = 0;
  int golden_keys = 0;
  int crystal_keys = 0;
  bool legacy_keys = false;
  bool loot_second_deck = false;
  uint8_t loot_remaining[11]{};
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
  std::vector<FloorState> visited_floors;
  int entrance_cell=-1;
  bool ascending=false;
  uint8_t highest_ascent=26;
  float ascent_stacks=0.f,ascent_damage=0.f;
  int flat_healing=0;
  blob_rules::Gas<MW,MH> toxic_gas;
  uint8_t boss_dead[5]{};
  uint8_t tokens = 0;
  uint8_t token_quest = 0;
  uint8_t smith_used = 0;
  uint8_t imp_state = 0;
  bool bag_potion = false, bag_scroll = false, bag_wand = false;
  bool shop_warned[27]{}, shop_gone[27]{};
  int dust_timer = 0;
  int respawn_t = 0;  // countdown in thirds of a turn
  int mob_cap = 0;
  struct SpawnRoom { uint8_t x, y, w, h; };
  std::vector<SpawnRoom> spawn_rooms;
  std::vector<uint8_t> spawn_weights;
  std::vector<uint8_t> spawn_deck;
  uint8_t next_spawn_type();
  bool try_respawn();
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
  uint8_t selection_scroll = IT_SCROLL_TRANSMUTE;
  int augment_target=0;
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
  uint8_t weapon_flags = IF_KNOWN, armor_flags = IF_KNOWN;
  uint8_t ring1_flags = IF_KNOWN, ring2_flags = IF_KNOWN;
  uint8_t weapon_augment=0,armor_augment=0;
  Identification weapon_identification,armor_identification,ring1_identification,ring2_identification;
  magic_rules::State weapon_magic,armor_magic,ring1_magic,ring2_magic;
  magic_rules::Effects hero_effects;
  float deferred_damage=0,kinetic_damage=0,freeze_left=0;
  uint16_t artifact_generated=0;
  int alchemy_energy=0;
  int magic_target=-1,magic_choice[3]{},artifact_slot=-3;
  bool artifact_targeting=false;
  bool artifact_brewing=false;
  bool time_stasis=false;
  int rose_ghost=-1;
  mechanics_rules::Actor spell_effects;
  std::vector<mechanics_rules::Actor> mob_effects;
  std::vector<mechanics_rules::HeapAwareness> scry_heaps;
  std::vector<int> bubble_presses;
  void end_time_bubble(bool disarm=false);
  void end_hourglass();
  void resolve_time_presses(bool disarm=false);
  void dispel_action();
  void freeze_actor(int index);
  void thaw_actor(int index);
  Item rose_weapon,rose_armor;
  int rose_command=-1,rose_enemy=-1;
  uint32_t projectile_serial=1;
  bool projectile_proc=false;
  bool returning_projectile=false;
  int artifact_feed_slot=0;
  uint8_t book_scroll=IT_NONE;
  bool book_empowered=false;
  bool inscription_replay=false;
  uint8_t cleric_ranks[cleric_rules::Count]{};
  uint8_t cleric_subclass=0,cleric_ability=0;
  int cleric_page=0,cleric_menu_mode=0;
  int cleric_choices[3]{-1,-1,-1};
  void cleric_menu(int mode=0);
  bool cleric_unlocked(int spell) const;
  bool cleric_learn(int spell);
  float cleric_lance_cooldown=0,cleric_priest_cooldown=0;
  int linked_ally=-1,stasis_ally=-1;
  float link_left=0,stasis_left=0;
  bool extra_proc=false;
  bool virtual_projectile=false;
  Item form_item;
  Item trinity_body,trinity_mind,trinity_spirit;
  bool discovered_items[IT_COUNT]{};
  uint32_t discovered_enchants=0,discovered_glyphs=0;
  int trinity_spell=-1;
  std::vector<Item> trinity_options;
  void discover_item(const Item& item);
  void trinity_menu(int spell);
  void trinity_imbue(const Item& item);
  void trinity_use(int form,int target=-1);
  void finish_tome_cast(int spell,int cost);
  int tome_slot() const;
  magic_rules::State& tome_state();
  uint8_t& tome_level();
  void cleric_food();
  uint8_t recalled_item=IT_NONE;
  float recall_left=0;
  float armor_charge=100,ascended_left=0;
  int ascended_casts=0,ascended_flashes=0,powered_ally=-1;
  bool divine_used=false;
  int magic_actor=-1;
  int magic_hero_cell=-1;
  struct AllyExperience {int amount;bool advance;};
  std::vector<AllyExperience> pending_ally_experience;
  std::vector<int> pending_ally_blasts;
  void award_combat_experience(int amount,bool advance=true);
  void finish_ally_combat();
  uint8_t spellbook_order[10]{};
  void init_spellbook();
  void cleric_armor_action(int target=-1);
  void rose_attack(int target);
  void rose_defend(int attacker,int& damage);
  void carry_allies(bool depart);
  struct CarriedAlly {
    Mob mob;
    mechanics_rules::Actor effects;
    bool powered=false,linked=false,stasis=false;
  };
  std::vector<CarriedAlly> carried_allies;
  void cancel_item_selection();
  void remember_inscription(uint8_t id);
  void cleric_form(int spell,int index);
  void spellbook_read();
  void spellbook_resolve(bool exotic);
  bool scroll_effect(uint8_t id,int target=-1);
  void seed_effect(uint8_t seed,int cell);
  void plant_seed(int index,int cell);
  void tick_extended_effects();
  void release_stasis();
  mechanics_rules::Actor& actor_effects(int mob);
  void shop_steal(int index);
  float shop_steal_chance(int index,int& charges) const;
  void artifact_feed(int slot,int index);
  bool artifact_feedable(int slot,int index) const;
  void rose_outfit(int index);
  void rose_menu();
  void tome_menu();
  void tome_cast(int spell,int target=-1);
  void throw_projectile(int index,int x,int y);
  bool missile_similar(const Item& first,const Item& second) const;
  uint8_t haste_t = 0, invis_t = 0, might_t = 0, levit_t = 0, mind_t = 0, rage_t = 0;
  uint8_t quest = 0;     // 0 none, 1 accepted, 2 complete
  int cursor_x = 1, cursor_y = 1;
  bool targeting = false;
  bool examining = false;
  bool type_known[IT_COUNT]{};
  uint8_t appearances[IT_COUNT]{};
  uint8_t swarm_healing_drops=0;
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
  void take_damage(int damage, bool bypass_shield = false);
  bool hero_hits(Mob& mob, bool ranged);

  uint8_t paused = 0;
  bool backpack = false;
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
  bool mob_sees_hero(const Mob& mob) const;
  bool los(int x0, int y0, int x1, int y1) const;
  bool walkable(int x, int y) const;
  bool blocks_sight(int x, int y) const;
  bool visible_trap_at(int x, int y) const;
  int mob_at(int x, int y) const;
  int item_at(int x, int y) const;
  void move_player(int dx, int dy);
  void act_at(int x, int y, bool interact);
  void interact_at(int x, int y);
  void attack(int mob, bool ranged, int bonus = 0);
  void damage_mob(int idx, int raw, DamageKind kind = DamageKind::Direct, int enchantment=0);
  void advance_turn();
  void spend_action(float duration);
  void earn_exp(int amount);
  void identify_on_exp(int amount);
  void identify_use(uint8_t& flags,Identification& progress,float total);
  void tick_survival();
  void tick_health();
  void tick_hunger();
  void tick_world();
  void tick_buffs();
  int effective_strength() const;
  void update_maxhp(bool boost_hp = false);
  int roll_armor();
  int roll_weapon(const Mob& target);
  int roll_mob_damage(const Mob& mob);
  float hero_evasion() const;
  float melee_delay() const;
  int buffed_level(int level) const;
  int wand_capacity(const Item& item) const;
  bool bolt_reaches(int x, int y, int tx, int ty) const;
  ballistica::Ray cast_ray(int from, int to, int flags) const;
  void tick(int dt_ms);
  void clear_path();
  void wait_turns(int n);
  float mob_turn(Mob& m);
  void message(const char* s);
  void use_item(int idx);
  int quickslot_item(int slot) const;
  void use_quickslot(int slot);
  void equip_item(int idx);
  void cast_wand(int idx, int tx, int ty);
  void pick_item(int idx);
  void descend();
  void ascend();
  void remember_floor();
  void capture_floor(FloorState& state) const;
  void apply_floor(const FloorState& state);
  bool restore_floor(int depth,bool at_exit);
  void transition_prompt();
  float mob_modifier(const Mob& mob) const;
  void ascent_level_switch();
  void open_dialog(int type, const char* name, const char* text,
                   const char* o0 = nullptr, const char* o1 = nullptr,
                   const char* o2 = nullptr, const char* o3 = nullptr);
  void dialog_choose(int i);
  void add_item(uint8_t id, int count = 1, int level = 0, uint8_t flags = IF_KNOWN);
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
  Item selection_item(int idx) const;
  bool can_select_item(int idx) const;
  void select_item(int idx);
  void augment_item(int choice);
  void check_talents();
  int shop_sell_price(uint8_t id, int count, int level) const;
  void spawn_quest_miniboss();
  void talk_npc(int idx);
  void give_loot(int tier, bool chest);
  int bag_capacity() const;
  int ring_atk() const;
  bool ring_has(uint8_t id) const;
  void equip_ring(int idx);
  void recharge_wands();
  void search();
  void search_button();
  void examine_at(int x,int y);
  void describe_item(const Item& item,char* out,size_t size) const;
  int random_enchant(bool armor,bool cursed=false,int rarity=-1,int exclude=0);
  void enchant_item(int target,int effect);
  int weapon_proc(int target,int damage,int& enchantment);
  int weapon_enchant_proc(int target,int damage,int& enchantment);
  int weapon_final_damage(Mob& enemy,int damage,int enchantment);
  int armor_proc(Mob& attacker,int damage,bool magic=false);
  int armor_glyph_proc(Mob& attacker,int damage,bool magic);
  void tick_magic();
  float enchant_power() const;
  float movement_cost(int x=-1,int y=-1) const;
  int accessory_bonus(uint8_t ring) const;
  int melee_reach() const;
  bool melee_reaches(int x,int y) const;
  void push_mob(int index,int from_x,int from_y,int distance);
  void teleport_mob(int index);
  void artifact_menu(int slot);
  void artifact_action(int slot,int action,int target=-1);
  void artifact_experience(float portion);
  void artifact_charge(float amount);
  uint8_t generate_artifact();
  bool has_artifact(uint8_t id) const;
  int artifact_level(uint8_t id) const;
  const char* display_name(uint8_t id) const;
  int display_sprite(uint8_t id) const;
  bool identity_known(uint8_t id) const;
  void identify_type(uint8_t id);
  void init_identities(bool legacy=false);
  void trample_grass(int x,int y,bool hero_step=false);
  bool next_step_encounter(int x,int y) const;
  bool drop_item(int idx);
  void cast_or_throw(int cx, int cy);
  SlotMeta meta() const;
  bool save(int slot) const;
  bool load(int slot);
  static bool slot_meta(int slot, SlotMeta& out);
  static bool delete_slot(int slot);
};
