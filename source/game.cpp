#include "game.h"
#include "floor_layout.h"
#include "floor_maze.h"
#include "floor_patch.h"
#include "floor_painter.h"
#include "floor_rooms.h"
#include "pathfinder.h"

void dlog(const char* fmt, ...);

#include <algorithm>
#include <cmath>
#include <cstring>

static inline uint32_t rnd(Game& g) {
  g.rng ^= g.rng << 13;
  g.rng ^= g.rng >> 17;
  g.rng ^= g.rng << 5;
  return g.rng;
}
static inline int ri(Game& g, int a, int b) {
  if (b < a) return a;
  return a + (int)(rnd(g) % (uint32_t)(b - a + 1));
}
static int absi(int v) { return v < 0 ? -v : v; }
static int grid_distance(int x, int y, int tx, int ty) {
  return std::max(absi(x - tx), absi(y - ty));
}

struct GridSearch : PathFinder<MW, MH> {
  uint8_t occupied[MH][MW];

  void load_actors(const Game& g) {
    memset(occupied, 0, sizeof(occupied));
    for (const Mob& m : g.mobs)
      if (m.alive && m.x >= 0 && m.x < MW && m.y >= 0 && m.y < MH)
        occupied[m.y][m.x] = 1;
    occupied[g.py][g.px] = 1;
  }

  template<class F> int toward(int start, int goal, F passable) {
    return passable(goal) ? step(start, goal, passable) : -1;
  }
};
static GridSearch navigation;

static bool is_npc(uint8_t t) { return t >= M_NPC_GHOST && t <= M_NPC_IMP; }

static bool is_shopkeeper(uint8_t t, int floor) {
  return t == M_NPC_SHOP || (t == M_NPC_IMP && floor == 20);
}

static bool is_quest_miniboss(uint8_t t) {
  return t == M_FETID_RAT || t == M_TRICKSTER || t == M_GREAT_CRAB;
}
static uint8_t ghost_tier(Game& g) {
  int r = ri(g, 0, 99);
  if (r < 50) return 2;
  if (r < 80) return 3;
  if (r < 95) return 4;
  return 5;
}
static uint8_t ghost_level(Game& g) {
  int r = ri(g, 0, 99);
  if (r < 50) return 0;
  if (r < 80) return 1;
  if (r < 95) return 2;
  return 3;
}

static void gain_gold(Game& g, int amount) {
  if (g.ring_has(IT_RING_WEALTH)) amount += amount / 4;
  g.gold += amount;
}
static bool is_boss(uint8_t t) { return mob_def(t).boss; }

int Game::hero_atk() const {
  static const int atk[6] = {3, 2, 4, 3, 3, 2};
  return atk[hero % 6];
}
int Game::hero_def() const {
  static const int def[6] = {1, 0, 0, 0, 0, 1};
  return def[hero % 6];
}
int Game::weapon_bonus() const {
  if (weapon == IT_NONE) return 0;
  return item_def(weapon).power + weapon_level * 2;
}
int Game::bag_capacity() const {
  return BAG_CAP + 5 * ((bag_potion ? 1 : 0) + (bag_scroll ? 1 : 0) +
                        (bag_wand ? 1 : 0));
}

int Game::armor_bonus() const {
  if (armor == IT_NONE) return 0;
  return item_def(armor).power + armor_level;
}
bool Game::ring_has(uint8_t id) const {
  return ring1 == id || ring2 == id;
}
int Game::ring_atk() const {
  int b = 0;
  if (ring1 == IT_RING_MIGHT) b += 2 + ring1_level;
  if (ring2 == IT_RING_MIGHT) b += 2 + ring2_level;
  if (ring1 == IT_RING_ACCURACY) b += 1 + ring1_level;
  if (ring2 == IT_RING_ACCURACY) b += 1 + ring2_level;
  return b;
}
int Game::ring_def() const {
  int b = 0;
  if (ring1 == IT_RING_EVASION) b += 1 + ring1_level;
  if (ring2 == IT_RING_EVASION) b += 1 + ring2_level;
  return b;
}
void Game::equip_ring(int idx) {
  if (idx < 0 || idx >= inv_n) return;
  Item& it = inv[idx];
  if (ring1 == IT_NONE) {
    ring1 = it.id; ring1_level = it.level;
    remove_item(idx, 1);
  } else if (ring2 == IT_NONE) {
    ring2 = it.id; ring2_level = it.level;
    remove_item(idx, 1);
  } else {
    uint8_t old = ring1, oldl = ring1_level;
    ring1 = it.id; ring1_level = it.level;
    inv[idx].id = old; inv[idx].level = oldl; inv[idx].count = 1;
  }
  message("你戴上了一枚指环。");
}

// ---------------------------------------------------------------------------
// Map queries
// ---------------------------------------------------------------------------
bool Game::feature_blocking(int x, int y) const {
  for (const Feature& f : features) {
    if (f.x != x || f.y != y) continue;
    switch (f.type) {
      case F_CHEST:
      case F_CHEST_LOCKED:
      case F_CHEST_CRYSTAL:
      case F_CHEST_MIMIC:
      case F_WELL:
      case F_BOOKSHELF:
      case F_ALCHEMY:
      case F_STATUE:
      case F_TOMB:
      case F_SIGN:
      case F_BARRICADE:
        return true;
      default:
        break;
    }
  }
  return false;
}

int Game::feature_at(int x, int y) const {
  for (size_t i = 0; i < features.size(); i++)
    if (features[i].type != F_MIMIC_LOOT && features[i].x == x && features[i].y == y)
      return (int)i;
  return -1;
}

bool Game::boss_alive() const {
  int r = region();
  if (floor != REGIONS[r].boss_floor) return false;
  for (const Mob& m : mobs)
    if (m.alive && is_boss(m.type)) return true;
  return false;
}

bool Game::walkable(int x, int y) const {
  if (!in_map(x, y)) return false;
  if (feature_blocking(x, y)) return false;
  uint8_t t = tiles[y][x];
  return t == T_FLOOR || t == T_DOOR || t == T_DOOR_OPEN || t == T_STAIRS || t == T_WATER ||
         t == T_GRASS;
}
bool Game::blocks_sight(int x, int y) const {
  if (!in_map(x, y)) return true;
  uint8_t t = tiles[y][x];
  return t == T_WALL || t == T_SECRET || t == T_DOOR || t == T_DOOR_LOCKED;
}
int Game::mob_at(int x, int y) const {
  for (size_t i = 0; i < mobs.size(); i++) {
    const Mob& m = mobs[i];
    if (m.alive && m.x == x && m.y == y) return (int)i;
  }
  return -1;
}
int Game::item_at(int x, int y) const {
  for (size_t i = 0; i < items.size(); i++)
    if (items[i].x == x && items[i].y == y) return (int)i;
  return -1;
}

bool Game::los(int x0, int y0, int x1, int y1) const {
  int dx = absi(x1 - x0), dy = absi(y1 - y0);
  int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
  int err = dx - dy;
  int x = x0, y = y0;
  while (!(x == x1 && y == y1)) {
    int e2 = 2 * err;
    if (e2 > -dy) { err -= dy; x += sx; }
    if (e2 < dx) { err += dx; y += sy; }
    if (x == x1 && y == y1) break;
    if (blocks_sight(x, y)) return false;
  }
  return true;
}

void Game::message(const char* s) {
  snprintf(msg, sizeof(msg), "%s", s);
  msg_timer = 150;
}

void Game::recompute_fov() {
  memset(vis, 0, sizeof(vis));
  const int R = fov_radius;
  for (int y = 0; y < map_height; y++) {
    for (int x = 0; x < map_width; x++) {
      int dx = x - px, dy = y - py;
      if (dx * dx + dy * dy > R * R + 1) continue;
      if (los(px, py, x, y)) {
        vis[y][x] = true;
        seen[y][x] |= 1;  // bit0 = visited, bit1 = mapped
      }
    }
  }
  vis[py][px] = true;
  seen[py][px] |= 1;
}

// ---------------------------------------------------------------------------
// Equipment / loot tables
// ---------------------------------------------------------------------------
static uint8_t weapon_for_tier(int t) {
  switch (t) {
    case 1: return IT_SHORTSWORD;
    case 2: return IT_LONGSWORD;
    case 3: return IT_WARHAMMER;
    case 4: return IT_GREATAXE;
    default: return IT_RUNEBLADE;
  }
}
static uint8_t armor_for_tier(int t) {
  switch (t) {
    case 1: return IT_LEATHER_ARMOR;
    case 2: return IT_MAIL;
    case 3: return IT_SCALE;
    case 4: return IT_PLATE;
    default: return IT_RUNE_ARMOR;
  }
}
static uint8_t random_wand(Game& g, int tier) {
  static const uint8_t WANDS[13] = {
      IT_WAND_MAGIC, IT_WAND_FIRE, IT_WAND_FROST, IT_WAND_LIGHTNING,
      IT_WAND_DISINTEGRATION, IT_WAND_CORROSION, IT_WAND_EARTH,
      IT_WAND_BLAST, IT_WAND_CORRUPTION, IT_WAND_WARDING,
      IT_WAND_REGROWTH, IT_WAND_TRANSFUSION, IT_WAND_PRISMATIC};
  int roll = ri(g, 0, 99);
  if (tier >= 4 && roll < 30) return WANDS[12];       // prismatic
  if (tier >= 3 && roll < 45) return WANDS[ri(g, 4, 10)];
  return WANDS[ri(g, 0, tier >= 2 ? 5 : 3)];
}

static uint8_t weapon_for_tier_rand(Game& g, int t) {
  static const uint8_t T[5][3] = {
      {IT_DAGGER, IT_SHORTSWORD, IT_CUDGEL},
      {IT_LONGSWORD, IT_HAND_AXE, IT_SPEAR},
      {IT_WARHAMMER, IT_SCIMITAR, IT_WHIP},
      {IT_GREATAXE, IT_GREATSWORD, IT_HALBERD},
      {IT_RUNEBLADE, IT_BATTLEAXE, IT_GIANT_HAMMER}};
  t = t < 1 ? 1 : (t > 5 ? 5 : t);
  return T[t - 1][ri(g, 0, 2)];
}

struct LootRoll {
  uint8_t id = IT_GOLD;
  uint8_t level = 0;
  int gold = 0;
};

static int tier_for_floor(Game& g, int floor) {
  static const int probs[5][5] = {
      {0, 75, 20, 4, 1}, {0, 25, 50, 20, 5}, {0, 0, 40, 50, 10},
      {0, 0, 20, 40, 40}, {0, 0, 0, 20, 80}};
  int fs = (floor - 1) / 5;
  if (fs < 0) fs = 0;
  if (fs > 4) fs = 4;
  int roll = ri(g, 0, 99);
  int acc = 0;
  for (int tier = 1; tier <= 5; tier++) {
    acc += probs[fs][tier - 1];
    if (roll < acc) return tier;
  }
  return 5;
}

// Generator.random consumes the run's category deck. Default reward draws
// use combined weights without advancing it. Artifacts share the ring slot
// until the native artifact catalog is available.
static LootRoll roll_loot(Game& g, bool allowGold = true, bool consume = false) {
  enum { C_WEP, C_ARMOR, C_WAND, C_RING, C_POTION, C_SCROLL, C_SEED, C_STONE,
         C_GOLD, C_MISSILE, C_COUNT };
  static constexpr uint8_t weights[2][C_COUNT] = {
      {2, 2, 1, 1, 8, 8, 1, 1, 10, 1}, {2, 1, 1, 1, 8, 8, 1, 1, 10, 2}};
  static_assert(sizeof(g.loot_remaining) == C_COUNT);
  if (consume && std::none_of(g.loot_remaining, g.loot_remaining + C_COUNT,
                              [](uint8_t count) { return count != 0; })) {
    std::copy_n(weights[g.loot_second_deck], C_COUNT, g.loot_remaining);
    g.loot_second_deck = !g.loot_second_deck;
  }
  int W[C_COUNT];
  for (int i = 0; i < C_COUNT; ++i)
    W[i] = consume ? g.loot_remaining[i] : weights[0][i] + weights[1][i];
  int total = 0;
  for (int i = 0; i < C_COUNT; i++) {
    if (i == C_GOLD && !allowGold) W[i] = 0;
    total += W[i];
  }
  int roll = ri(g, 0, total - 1);
  int cat = 0, acc = 0;
  for (int i = 0; i < C_COUNT; i++) {
    acc += W[i];
    if (roll < acc) { cat = i; break; }
  }
  if (consume) --g.loot_remaining[cat];
  LootRoll r;
  int floor = g.floor;
  switch (cat) {
    case C_WEP: {
      int t = tier_for_floor(g, floor);
      r.id = weapon_for_tier_rand(g, t);
      r.level = ri(g, 0, t >= 3 ? 1 : 0);
      break;
    }
    case C_ARMOR: {
      int t = tier_for_floor(g, floor);
      r.id = armor_for_tier(t);
      r.level = ri(g, 0, t >= 3 ? 1 : 0);
      break;
    }
    case C_WAND: r.id = random_wand(g, (floor - 1) / 5 + 1); break;
    case C_POTION: {
      static const uint8_t P[11] = {
          IT_POTION_HEAL, IT_POTION_HEAL, IT_POTION_SHIELD, IT_POTION_HASTE,
          IT_POTION_INVIS, IT_POTION_LEVITATE, IT_POTION_MIND, IT_POTION_XP,
          IT_POTION_MIGHT, IT_POTION_TOXIC, IT_POTION_FLAME};
      r.id = P[ri(g, 0, 10)];
      break;
    }
    case C_SCROLL: {
      static const uint8_t S[11] = {
          IT_SCROLL_MAP, IT_SCROLL_MAP, IT_SCROLL_TELE, IT_SCROLL_IDENTIFY,
          IT_SCROLL_CURSE, IT_SCROLL_TERROR, IT_SCROLL_LULLABY,
          IT_SCROLL_RECHARGE, IT_SCROLL_RAGE, IT_SCROLL_CHALLENGE,
          IT_SCROLL_TRANSMUTE};
      r.id = S[ri(g, 0, 10)];
      break;
    }
    case C_STONE: r.id = IT_STONE_AUG; break;
    case C_RING: r.id = (uint8_t)ri(g, IT_RING_MIGHT, IT_RING_ELEMENTS); break;
    case C_MISSILE: r.id = (uint8_t)ri(g, IT_KNIFE, IT_JAVELIN); break;
    case C_SEED: r.id = (uint8_t)ri(g, IT_SEED_SUNGRASS, IT_SEED_BLINDWEED); break;
    default:
      r.id = IT_GOLD;
      r.gold = ri(g, 30 + floor * 10, 60 + floor * 20);
      break;
  }
  return r;
}

static void grant_loot(Game& g, const LootRoll& r) {
  if (r.id == IT_GOLD) {
    gain_gold(g, r.gold);
    g.message("你找到了金币。");
  } else {
    g.add_item(r.id, 1, r.level);
    char b[96];
    snprintf(b, sizeof(b), "你找到了 %s", item_name(r.id));
    g.message(b);
  }
}

void Game::give_loot(int tier, bool chest) {
  (void)tier;
  LootRoll r = roll_loot(*this, true);
  grant_loot(*this, r);
  if (chest && ri(*this, 0, 2) == 0) {
    LootRoll extra = roll_loot(*this, false);
    grant_loot(*this, extra);
  }
}

// ---------------------------------------------------------------------------
// Level generation
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// SPD-style run plan and mob rotation
// ---------------------------------------------------------------------------
enum SpecialRoomType : uint8_t {
  SP_NONE = 0,
  SP_TREASURY, SP_ARMORY, SP_CRYPT, SP_POOL, SP_WELL, SP_GARDEN, SP_LIBRARY,
  SP_STORAGE, SP_LAB, SP_STATUE, SP_SENTRY, SP_TRAPS, SP_RUNESTONE, SP_VAULT,
  SP_TOXIC, SP_FIRE,
};

static bool spec_locked(uint8_t s) {
  switch (s) {
    case SP_TREASURY: case SP_ARMORY: case SP_CRYPT: case SP_WELL:
    case SP_GARDEN: case SP_LIBRARY: case SP_STATUE: case SP_SENTRY:
    case SP_RUNESTONE: case SP_VAULT: case SP_LAB:
      return true;
    default:
      return false;
  }
}

static int pick_weighted_5(uint32_t& s) {
  // {1,3,1} / 5
  uint32_t r = s = s * 1664525u + 1013904223u;
  r %= 5;
  return r == 0 ? 0 : (r <= 3 ? 1 : 2);
}

void Game::build_run_plan() {
  uint32_t s = seed ^ 0x9E3779B9u;
  auto rr = [&]() { s = s * 1664525u + 1013904223u; return s; };
  for (int i = 0; i < 25; i++) {
    plan_special_n[i] = 0;
    plan_feeling[i] = 0;
    plan_secret[i] = 0;
    std::fill_n(plan_special[i], 4, SP_NONE);
    plan_guaranteed[i] = 0;
  }

  // Level.create: each supported feeling occupies one of the 14 outcomes.
  // Chasm is reserved until terrain and fall transitions support it.
  for (int f = 2; f <= 25; f++) {
    if (f % 5 == 0) continue;
    switch ((rr() >> 16) % 14) {
      case 0: plan_feeling[f - 1] = 1; break;  // water
      case 1: plan_feeling[f - 1] = 2; break;  // grass
      case 2: plan_feeling[f - 1] = 3; break;  // large
      case 3: plan_feeling[f - 1] = 4; break;  // traps
      case 4: plan_feeling[f - 1] = 5; break;  // secrets
      case 5: plan_feeling[f - 1] = 6; break;  // dark
      default: break;
    }
  }

  // One laboratory per region, either on its 3rd or 4th floor.
  bool lab[26] = {};
  for (int r = 0; r < 5; r++) lab[r * 5 + 3 + (int)((rr() >> 16) % 2)] = true;

  static const uint8_t EQUIP[] = {
      SP_CRYPT, SP_POOL, SP_ARMORY, SP_SENTRY, SP_STATUE, SP_VAULT};
  static const uint8_t CONS[] = {
      SP_RUNESTONE, SP_GARDEN, SP_LIBRARY, SP_STORAGE, SP_TREASURY,
      SP_WELL, SP_TOXIC, SP_FIRE, SP_TRAPS};
  uint8_t eq[sizeof(EQUIP)];
  uint8_t co[sizeof(CONS)];
  memcpy(eq, EQUIP, sizeof(eq));
  memcpy(co, CONS, sizeof(co));
  for (int i = (int)sizeof(eq) - 1; i > 0; i--) {
    int j = (int)(rr() % (uint32_t)(i + 1));
    uint8_t t = eq[i]; eq[i] = eq[j]; eq[j] = t;
  }
  for (int i = (int)sizeof(co) - 1; i > 0; i--) {
    int j = (int)(rr() % (uint32_t)(i + 1));
    uint8_t t = co[i]; co[i] = co[j]; co[j] = t;
  }
  std::vector<uint8_t> queue;
  queue.push_back(co[0]);                       // SPD starts with a consumable
  for (unsigned i = 0; i < sizeof(co) - 1; i++) {
    if (i < sizeof(eq)) queue.push_back(eq[i]);
    queue.push_back(co[i + 1]);
  }

  auto potion_room = [](uint8_t t) {
    return t == SP_POOL || t == SP_SENTRY || t == SP_STORAGE ||
           t == SP_TOXIC || t == SP_FIRE || t == SP_TRAPS;
  };

  for (int f = 1; f <= 25; f++) {
    if (f % 5 == 0) continue;  // boss floors are separate layouts in SPD
    int reg = (f - 1) / 5;
    int n = 0;
    switch (reg) {
      case 0: n = 1 + ((rr() % 5) ? 1 : 0); break;
      case 1: n = 1 + pick_weighted_5(s); break;
      case 2: n = 2 + ((rr() % 5) == 0 ? 1 : 0); break;
      case 3: n = 2 + ((rr() % 3) == 0 ? 1 : 0); break;
      default: n = 2 + (int)(rr() % 2); break;
    }
    if (plan_feeling[f - 1] == 3) n = reg == 0 ? 3 : 4;
    plan_special_n[f - 1] = (uint8_t)n;
    if (lab[f]) plan_special[f - 1][0] = SP_LAB;
    auto eligible = queue;
    for (int i = lab[f] ? 1 : 0; i < n; i++) {
      int m = (int)eligible.size();
      uint32_t r = rr() % 10;
      int idx = r < 6 ? 0 : (r < 9 ? 1 : 2);
      if (idx >= m) idx = m - 1;
      uint8_t v = eligible[idx];
      eligible.erase(std::remove_if(eligible.begin(), eligible.end(), [&](uint8_t t) {
        return t == v || (potion_room(v) && potion_room(t));
      }), eligible.end());
      queue.erase(std::find(queue.begin(), queue.end(), v));
      queue.push_back(v);  // SPD useType(): move to the back
      plan_special[f - 1][i] = v;
    }
  }

  // Secret rooms: 2/2.25/2.5/2.75/3 per region, spread over its floors.
  static const float BASE_SECRETS[5] = {2.0f, 2.25f, 2.5f, 2.75f, 3.0f};
  for (int reg = 0; reg < 5; reg++) {
    int total = (int)BASE_SECRETS[reg];
    float frac = BASE_SECRETS[reg] - total;
    if ((rr() % 1000) < (uint32_t)(frac * 1000.0f)) total++;
    int left = total;
    for (int j = 1; j <= 4; j++) {
      int depth = reg * 5 + j;
      if (depth == 1) continue;
      int floors = 5 - j;
      int take = left / floors + (int(rr() % floors) < left % floors);
      plan_secret[depth - 1] = (uint8_t)take;
      left -= take;
    }
  }

  // Dungeon.posNeeded / souNeeded, for the default challenge-free run.
  for (int reg = 0; reg < 5; ++reg) {
    for (int j = 1; j <= 3; j += 2)
      plan_guaranteed[reg * 5 + j - 1 + ((rr() >> 16) % 2)] |= 1;
    int upgrades = 3;
    for (int j = 1; j <= 4 && upgrades; ++j) {
      if (int((rr() >> 16) % (5 - j)) >= upgrades) continue;
      plan_guaranteed[reg * 5 + j - 1] |= 2;
      --upgrades;
    }
  }
}

static void mob_rotation(Game& g, int depth, std::vector<uint8_t>& out) {
  out.clear();
  auto add = [&](uint8_t m) { out.push_back(m); };
  switch (depth) {
    case 1:
      add(M_RAT); add(M_RAT); add(M_RAT); add(M_SNAKE); break;
    case 2:
      add(M_RAT); add(M_RAT); add(M_SNAKE); add(M_GNOLL); add(M_GNOLL); break;
    case 3:
      add(M_RAT); add(M_SNAKE); add(M_GNOLL); add(M_GNOLL); add(M_GNOLL);
      add(M_SWARM); add(M_CRAB); break;
    case 4: case 5:
      add(M_GNOLL); add(M_SWARM); add(M_CRAB); add(M_CRAB);
      add(M_SLIME); add(M_SLIME); break;
    case 6:
      add(M_SKELETON); add(M_SKELETON); add(M_SKELETON); add(M_THIEF);
      add(M_SWARM); break;
    case 7:
      add(M_SKELETON); add(M_SKELETON); add(M_SKELETON); add(M_THIEF);
      add(M_DM100); add(M_GUARD); break;
    case 8:
      add(M_SKELETON); add(M_SKELETON); add(M_THIEF); add(M_DM100);
      add(M_DM100); add(M_GUARD); add(M_GUARD); add(M_NECROMANCER); break;
    case 9: case 10:
      add(M_SKELETON); add(M_THIEF); add(M_DM100); add(M_DM100);
      add(M_GUARD); add(M_GUARD); add(M_NECROMANCER); add(M_NECROMANCER);
      break;
    case 11:
      add(M_BAT); add(M_BAT); add(M_BAT); add(M_BRUTE); add(M_SHAMAN); break;
    case 12:
      add(M_BAT); add(M_BAT); add(M_BRUTE); add(M_BRUTE); add(M_SHAMAN);
      add(M_SPINNER); break;
    case 13:
      add(M_BAT); add(M_BRUTE); add(M_BRUTE); add(M_SHAMAN); add(M_SHAMAN);
      add(M_SPINNER); add(M_SPINNER); add(M_DM200); break;
    case 14: case 15:
      add(M_BAT); add(M_BRUTE); add(M_SHAMAN); add(M_SHAMAN);
      add(M_SPINNER); add(M_SPINNER); add(M_DM200); add(M_DM200); break;
    case 16:
      add(M_GHOUL); add(M_GHOUL); add(M_GHOUL); add(M_ELEMENTAL);
      add(M_WARLOCK); break;
    case 17:
      add(M_GHOUL); add(M_ELEMENTAL); add(M_ELEMENTAL); add(M_WARLOCK);
      add(M_MONK); break;
    case 18:
      add(M_GHOUL); add(M_ELEMENTAL); add(M_WARLOCK); add(M_WARLOCK);
      add(M_MONK); add(M_MONK); add(M_GOLEM); break;
    case 19: case 20:
      add(M_ELEMENTAL); add(M_WARLOCK); add(M_WARLOCK); add(M_MONK);
      add(M_MONK); add(M_GOLEM); add(M_GOLEM); add(M_GOLEM); break;
    case 21:
      add(M_SUCCUBUS); add(M_SUCCUBUS); add(M_EYE); break;
    case 22:
      add(M_SUCCUBUS); add(M_EYE); break;
    case 23:
      add(M_SUCCUBUS); add(M_EYE); add(M_EYE); add(M_SCORPIO); break;
    default:
      add(M_SUCCUBUS); add(M_EYE); add(M_EYE);
      add(M_SCORPIO); add(M_SCORPIO); add(M_SCORPIO); break;
  }
  // Rare rotation additions (SPD MobSpawner.addRareMobs).
  if (depth == 4 && (rnd(g) % 1000) < 25) add(M_THIEF);
  if (depth == 9 && (rnd(g) % 1000) < 25) add(M_BAT);
  if (depth == 14 && (rnd(g) % 1000) < 25) add(M_GHOUL);
  if (depth == 19 && (rnd(g) % 1000) < 25) add(M_SUCCUBUS);
  // Rare alts: 1/50 rats become albino (only alt we have assets for).
  for (uint8_t& m : out)
    if (m == M_RAT && (rnd(g) % 50) == 0) m = M_ALBINO;
  for (int i = (int)out.size() - 1; i > 0; i--) {
    int j = ri(g, 0, i);
    uint8_t t = out[i]; out[i] = out[j]; out[j] = t;
  }
}

uint8_t Game::next_spawn_type() {
  if (spawn_deck.empty()) mob_rotation(*this, floor, spawn_deck);
  const uint8_t type = spawn_deck.back();
  spawn_deck.pop_back();
  return type;
}

static Mob make_floor_mob(uint8_t type, int x, int y, Mob::Ai ai) {
  Mob m;
  const MobDef& d = mob_def(type);
  m.x = x; m.y = y; m.vx = x; m.vy = y;
  m.type = type; m.hp = m.maxhp = d.hp; m.atk = d.atk; m.def = d.def;
  m.ai = ai;
  return m;
}

static void spawn_distances(const Game& g, int16_t (&dist)[MH][MW],
                            bool open_doors, int limit = MW * MH) {
  uint16_t queue[MW * MH];
  for (auto& row : dist) std::fill_n(row, MW, int16_t(-1));
  int head = 0, tail = 1;
  queue[0] = g.py * MW + g.px;
  dist[g.py][g.px] = 0;
  while (head < tail) {
    const int p = queue[head++], x = p % MW, y = p / MW;
    if (dist[y][x] >= limit) continue;
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
      int nx = x + dx, ny = y + dy;
      if (!g.walkable(nx, ny) || dist[ny][nx] >= 0 ||
          (!open_doors && g.tiles[ny][nx] == T_DOOR)) continue;
      dist[ny][nx] = dist[y][x] + 1;
      queue[tail++] = ny * MW + nx;
    }
  }
}

static bool floor_spawn_cell(const Game& g, int x, int y, uint8_t type) {
  if (!g.walkable(x, y) || (x == g.px && y == g.py) || g.mob_at(x, y) >= 0) return false;
  const auto t = g.tiles[y][x];
  if (t != T_FLOOR && t != T_GRASS && t != T_WATER) return false;
  if (type == M_DM200 || type == M_GOLEM) {
    for (int dy : {-1, 1}) for (int dx : {-1, 1})
      if (!g.blocks_sight(x + dx, y) && !g.blocks_sight(x, y + dy) &&
          !g.blocks_sight(x + dx, y + dy)) return true;
    return false;
  }
  return true;
}

bool Game::try_respawn() {
  if (spawn_rooms.empty()) return false;
  int16_t dist[MH][MW];
  spawn_distances(*this, dist, true);
  uint8_t type = next_spawn_type();
  for (int attempt = 0; attempt < 30; ++attempt) {
    for (int sample = 0; sample < 30; ++sample) {
      const auto& r = spawn_rooms[ri(*this, 0, int(spawn_rooms.size()) - 1)];
      int x = r.x + ri(*this, 1, r.w - 2), y = r.y + ri(*this, 1, r.h - 2);
      if (vis[y][x] || !floor_spawn_cell(*this, x, y, type)) continue;
      if (dist[y][x] >= 12) {
        mobs.push_back(make_floor_mob(type, x, y, Mob::Wandering));
        return true;
      }
      break;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------
// Level generation
// ---------------------------------------------------------------------------
void Game::gen_level() {
  map_width = map_height = LEGACY_MAP_SIZE;
  hero_combat = {};
  combat_fx.clear();
  combat_delay = 0;
  memset(tiles, T_WALL, sizeof(tiles));
  memset(seen, 0, sizeof(seen));
  memset(vis, 0, sizeof(vis));
  mobs.clear();
  items.clear();
  features.clear();
  search_fx.clear();
  search_ms = 0;
  spawn_rooms.clear();
  spawn_deck.clear();
  respawn_t = 0;
  mob_cap = 0;
  rng = seed ^ (uint32_t)(floor * 2654435761u);
  if (!rng) rng = 1;
  build_run_plan();

  int reg = region();
  int feeling = plan_feeling[floor - 1];
  fov_radius = feeling == 6 ? 5 : 8;
  bool large = feeling == 3;
  bool traps_feeling = feeling == 4;
  bool secret_feeling = feeling == 5;
  bool boss_floor = (floor % 5) == 0;

  struct GRoom {
    int x, y, w, h, cx, cy;
    uint8_t kind;   // 0 entrance, 1 exit, 2 standard, 3 special, 4 secret,
                    // 5 shop, 6 quest
    uint8_t spec;
    bool locked;
    int door_x = -1, door_y = -1;
  };
  enum { RK_ENTRANCE, RK_EXIT, RK_STANDARD, RK_SPECIAL, RK_SECRET, RK_SHOP,
         RK_QUEST, RK_CONNECTOR };
  std::vector<GRoom> rooms;

  int nstd = 5;
  switch (reg) {
    case 0: nstd = 4 + pick_weighted_5(rng); break;
    case 1: nstd = 5 + (int)(rnd(*this) % 2); break;
    case 2: nstd = 6 + ((rnd(*this) % 3) ? 0 : 1); break;
    case 3: nstd = 6 + pick_weighted_5(rng); break;
    default: nstd = 8 + ((rnd(*this) % 3) ? 0 : 1); break;
  }
  if (large) {
    static constexpr int max_std[] = {6, 6, 7, 8, 9};
    nstd = (max_std[reg] * 3 + 1) / 2;
  }
  if (boss_floor) nstd = 4;

  int nspec = boss_floor ? 0 : plan_special_n[floor - 1];
  int nsecret = boss_floor ? 0 : plan_secret[floor - 1] + (secret_feeling ? 1 : 0);
  bool shop_here = (floor == 6 || floor == 11 || floor == 16);
  bool imp_shop = (floor == 20 && imp_state == 2);
  bool quest_extra = !boss_floor && (floor == smith_floor || floor == imp_floor);
  const bool shop = shop_here || imp_shop;
  int entrance_room = 0, exit_room = 0;
  using namespace floor_layout;
  auto layout_rng = floor_random(seed, floor);
  const Shape shape = select_shape(layout_rng);
  Layout layout;
  std::vector<RoomInput> standard;
  layout_builder = 0;
  if (!boss_floor) {
    standard = standard_room_inputs(layout_rng, floor, large);
    std::vector<Spec> specs;
    specs.reserve(standard.size() + nspec + nsecret + int(shop) + int(quest_extra));
    for (const auto& r : standard) specs.push_back(r.spec);
    if (shop) specs.push_back({Kind::Shop,7,10,7,10,1,1,1});
    for (int i = 0; i < nspec; ++i)
      specs.push_back({Kind::Leaf,5,10,5,10,1,1,1});
    for (int i = 0; i < nsecret; ++i)
      specs.push_back({Kind::Secret,5,10,5,10,1,1,1});
    // The original regional overrides append quest rooms after initRooms.
    // These are still the port's quest-room placeholders and run-plan rolls.
    if (quest_extra) specs.push_back({Kind::Leaf,7,10,7,10,1,1,1});
    std::vector<int> order;
    for (int i = 0; i < int(specs.size()); ++i) order.push_back(i);
    layout_rng.shuffle(order);
    Builder builder(layout_rng, shape, floor);
    for (int attempt = 0; attempt < 256; ++attempt) {
      layout = builder.attempt(specs, order);
      if (!layout.rooms.empty() && normalize(layout, 1, MW, MH)) {
        layout_builder = shape.eight ? 2 : 1;
        nstd = int(standard.size()) - 2;
        break;
      }
      layout = {};
    }
  }
  if (!layout.rooms.empty()) {
    map_width = layout.width; map_height = layout.height;
    exit_room = 1;
    for (int i = 0; i < int(layout.rooms.size()); ++i) {
      const auto p = layout.rooms[i].box;
      GRoom r{p.l, p.t, p.w(), p.h(), (p.l+p.r+1)/2, (p.t+p.b+1)/2,
              RK_STANDARD, SP_NONE, false};
      int leaf = i - (nstd + 2);
      if (i == entrance_room) r.kind = RK_ENTRANCE;
      else if (i == exit_room) r.kind = RK_EXIT;
      else if (layout.rooms[i].spec.kind == Kind::Connector) r.kind = RK_CONNECTOR;
      else if (leaf >= 0) {
        if (shop && leaf == 0) r.kind = RK_SHOP;
        else if ((leaf -= int(shop)) < nspec) {
          r.kind = RK_SPECIAL;
          r.spec = plan_special[floor - 1][leaf];
          r.locked = spec_locked(r.spec);
        } else if (leaf < nspec + nsecret) r.kind = RK_SECRET;
        else r.kind = RK_QUEST;
      }
      if (r.kind != RK_CONNECTOR)
        for (int y = r.y + 1; y < r.y + r.h - 1; ++y)
          for (int x = r.x + 1; x < r.x + r.w - 1; ++x) tiles[y][x] = T_FLOOR;
      rooms.push_back(r);
    }
    auto paint_order = layout.order;
    layout_rng.shuffle(paint_order);
    const Point unplaced{-1,-1};
    std::vector<std::vector<Point>> doors(rooms.size(), std::vector<Point>(rooms.size(),unplaced));
    struct DoorTile { Point cell; uint8_t tile; };
    std::vector<DoorTile> door_tiles;
    for (int a : paint_order) for (int b : layout.rooms[a].links) {
      if (doors[a][b].x >= 0) continue;
      const Point d = choose_door(layout.rooms[a], layout.rooms[b], layout_rng);
      doors[a][b] = doors[b][a] = d;
      const uint8_t tile = rooms[a].locked || rooms[b].locked ? T_DOOR_LOCKED :
          rooms[a].kind == RK_SECRET || rooms[b].kind == RK_SECRET ||
          layout.rooms[a].type == Maze || layout.rooms[b].type == Maze ? T_SECRET :
          rooms[a].kind == RK_CONNECTOR && rooms[b].kind == RK_CONNECTOR ? T_FLOOR : T_DOOR;
      door_tiles.push_back({d,tile});
      for (int id : {a,b}) if (rooms[id].kind >= RK_SPECIAL && rooms[id].kind < RK_CONNECTOR) {
        rooms[id].door_x = d.x; rooms[id].door_y = d.y;
      }
    }
    auto copy_interior = [&](RoomPaint&& paint) {
      const auto p = paint.box;
      for (int y = p.t+1; y < p.b; ++y) for (int x = p.l+1; x < p.r; ++x) {
        const auto t = paint.at({x,y});
        tiles[y][x] = t == Paint::Wall ? T_WALL : t == Paint::Chasm ? T_CHASM : T_FLOOR;
      }
    };
    for (int i : paint_order) {
      const auto& r = layout.rooms[i];
      const auto p = r.box;
      if (rooms[i].kind != RK_CONNECTOR &&
          (rooms[i].kind != RK_STANDARD || !has_standard_painter(standard[i].type))) continue;
      std::vector<Point> points;
      for (int n : r.links) points.push_back(doors[i][n]);
      if (rooms[i].kind == RK_STANDARD) {
        copy_interior(paint_standard(p,standard[i].type,r.spec.factor,points,layout_rng));
        continue;
      }
      if (r.type == Maze) {
        auto mask = maze(p,points,layout_rng);
        if (p.w() >= 5 && p.h() >= 5 && (p.w() <= 7 || p.h() <= 7)) {
          // Upstream retries small mazes until the central cell is a wall.
          const int center = (p.h()/2)*p.w()+p.w()/2;
          for (int retry = 0; retry < 16 && !mask[center]; ++retry)
            mask = maze(p,points,layout_rng);
        }
        for (int y = p.t+1; y < p.b; ++y)
          for (int x = p.l+1; x < p.r; ++x)
            if (!mask[(y-p.t)*p.w()+x-p.l]) tiles[y][x] = T_FLOOR;
        continue;
      }
      copy_interior(paint_connector(p,r.type,points,layout_rng));
      if (chasm_connector(r.type)) for (int n : r.neighbors) {
        const auto& other = layout.rooms[n];
        if (!chasm_connector(other.type)) continue;
        const auto q = other.box;
        Rect edge{std::max(p.l,q.l),std::max(p.t,q.t),std::min(p.r,q.r),std::min(p.b,q.b)};
        if (edge.l == edge.r) { ++edge.t; --edge.b; }
        else { ++edge.l; --edge.r; }
        for (int y = edge.t; y <= edge.b; ++y) for (int x = edge.l; x <= edge.r; ++x)
          tiles[y][x] = T_CHASM;
      }
    }
    for (const auto& d : door_tiles) tiles[d.cell.y][d.cell.x] = d.tile;
  } else {
  int required = 2 + nstd + nspec + nsecret + (shop_here ? 1 : 0) +
                 (imp_shop ? 1 : 0) + (quest_extra ? 1 : 0);
  if (required > 24) {
    nstd -= required - 24;
    if (nstd < 2) nstd = 2;
    required = 2 + nstd + nspec + nsecret + (shop_here ? 1 : 0) +
               (imp_shop ? 1 : 0) + (quest_extra ? 1 : 0);
  }

  // Adapt LoopBuilder's main loop + branches to the fixed save-map bounds.
  // Reserve one slot per one-connection room before placing any rooms.
  const int GS = 5, CELL = 8;
  bool chosen[25] = {};
  int parent[25];
  for (int i = 0; i < 25; i++) parent[i] = -1;
  std::vector<int> order;
  const int main_budget = nstd + 2;
  int loop_size, start, exit_slot;
  do {
  order.clear();
  std::fill_n(chosen, 25, false);
  std::fill_n(parent, 25, -1);
  int rw = 2 + ri(*this, 0, std::min(2, (main_budget - 4) / 2));
  int rh = ri(*this, 2, std::min(4, (main_budget + 4) / 2 - rw));
  if (!boss_floor && rw == 2 && rh == 2 && main_budget >= 6) rh = 3;
  const int lx = ri(*this, 0, GS - rw), ly = ri(*this, 0, GS - rh);
  auto loop_cell = [&](int x, int y) { order.push_back((ly + y) * GS + lx + x); };
  for (int x = 0; x < rw; ++x) loop_cell(x, 0);
  for (int y = 1; y < rh; ++y) loop_cell(rw - 1, y);
  for (int x = rw - 2; x >= 0; --x) loop_cell(x, rh - 1);
  for (int y = rh - 2; y > 0; --y) loop_cell(0, y);
  if (rnd(*this) & 1) std::reverse(order.begin(), order.end());
  loop_size = (int)order.size();
  std::rotate(order.begin(), order.begin() + ri(*this, 0, loop_size - 1), order.end());
  start = order.front(); exit_slot = order[loop_size / 2];
  for (int i = 0; i < loop_size; ++i) {
    chosen[order[i]] = true;
    if (i) parent[order[i]] = order[i - 1];
  }
  while ((int)order.size() < required) {
    std::vector<std::pair<int, int>> candidates, entrance_candidates;
    const bool shop = (shop_here || imp_shop) && (int)order.size() == main_budget;
    for (int i = 0; i < std::min((int)order.size(), main_budget); ++i) {
      // The slots after the standard rooms are leaves and can't be branched from.
      const int from = order[i];
      int x = from % GS, y = from / GS;
      const int nb[] = {x ? from - 1 : -1, x + 1 < GS ? from + 1 : -1,
                        y ? from - GS : -1, y + 1 < GS ? from + GS : -1};
      for (int to : nb) if (to >= 0 && !chosen[to]) {
        candidates.push_back({from, to});
        if (shop && from == start) entrance_candidates.push_back({from, to});
      }
    }
    if (candidates.empty()) break;
    const auto& options = entrance_candidates.empty() ? candidates : entrance_candidates;
    auto [from, to] = options[ri(*this, 0, (int)options.size() - 1)];
    parent[to] = from;
    chosen[to] = true;
    order.push_back(to);
  }
  } while ((int)order.size() != required);

  // Assign room kinds to the chosen slots.
  std::vector<std::pair<uint8_t, uint8_t>> assign;
  for (int i = 0; i < nstd; i++) assign.push_back({RK_STANDARD, SP_NONE});
  if (shop_here || imp_shop) assign.push_back({RK_SHOP, SP_NONE});
  if (quest_extra) assign.push_back({RK_QUEST, SP_NONE});
  for (int i = 0; i < nspec; i++)
    assign.push_back({RK_SPECIAL, plan_special[floor - 1][i]});
  for (int i = 0; i < nsecret; i++) assign.push_back({RK_SECRET, SP_NONE});
  // Build rooms in slot order, entrance first.
  int slot_to_room[25];
  for (int i = 0; i < 25; i++) slot_to_room[i] = -1;
  int ki = 0;
  for (int idx : order) {
    int gx = idx % GS, gy = idx / GS;
    int base_x = 1 + gx * CELL, base_y = 1 + gy * CELL;
    int w = ri(*this, 5, 6), h = ri(*this, 5, 6);
    GRoom r;
    r.x = base_x + 1;
    r.y = base_y + 1;
    r.w = w;
    r.h = h;
    r.cx = r.x + w / 2;
    r.cy = r.y + h / 2;
    r.spec = SP_NONE;
    r.locked = false;
    if (idx == start) {
      r.kind = RK_ENTRANCE;
      r.w = 6; r.h = 6;
      r.cx = r.x + 3; r.cy = r.y + 3;
    } else if (idx == exit_slot) {
      r.kind = RK_EXIT;
    } else if (ki < (int)assign.size()) {
      r.kind = assign[ki].first;
      r.spec = assign[ki].second;
      if (r.kind == RK_SPECIAL) r.locked = spec_locked(r.spec);
      ki++;
    } else {
      r.kind = RK_STANDARD;
    }
    slot_to_room[idx] = (int)rooms.size();
    rooms.push_back(r);
  }

  auto one_door_kind = [](uint8_t k) {
    return k == RK_SPECIAL || k == RK_SECRET || k == RK_SHOP || k == RK_QUEST;
  };

  // Carve rooms.
  for (const GRoom& r : rooms)
    for (int y = r.y; y < r.y + r.h; y++)
      for (int x = r.x; x < r.x + r.w; x++) tiles[y][x] = T_FLOOR;

  auto door_value = [&](const GRoom& r) -> uint8_t {
    if (r.kind == RK_SECRET) return T_SECRET;
    if (r.locked) return T_DOOR_LOCKED;
    return T_DOOR;
  };
  auto set_door = [&](int x, int y, GRoom& r) {
    if (x < 1 || y < 1 || x >= MW - 1 || y >= MH - 1) return;
    if (tiles[y][x] == T_FLOOR) tiles[y][x] = door_value(r);
    if (one_door_kind(r.kind)) { r.door_x = x; r.door_y = y; }
  };
  auto connect = [&](GRoom& a, GRoom& b) {
    int ax = a.cx, ay = a.cy, bx = b.cx, by = b.cy;
    if (a.y == b.y) ay = by = a.y + ri(*this, 1, std::min(a.h, b.h) - 2);
    if (a.x == b.x) ax = bx = a.x + ri(*this, 1, std::min(a.w, b.w) - 2);
    int dx = (bx > ax) - (bx < ax);
    int dy = (by > ay) - (by < ay);
    bool hf = (dx != 0 && dy != 0) ? ((rnd(*this) & 1) != 0) : (dy == 0);
    auto inside = [](const GRoom& r, int x, int y) {
      return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    };
    std::vector<std::pair<int, int>> path;
    auto push = [&](int x, int y) {
      if (path.empty() || path.back().first != x || path.back().second != y)
        path.push_back({x, y});
    };
    if (hf) {
      int sx = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
      for (int x = ax;; x += sx) { push(x, ay); if (sx == 0 || x == bx) break; }
      int sy = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
      for (int y = ay;; y += sy) { push(bx, y); if (sy == 0 || y == by) break; }
    } else {
      int sy = dy > 0 ? 1 : (dy < 0 ? -1 : 0);
      for (int y = ay;; y += sy) { push(ax, y); if (sy == 0 || y == by) break; }
      int sx = dx > 0 ? 1 : (dx < 0 ? -1 : 0);
      for (int x = ax;; x += sx) { push(x, by); if (sx == 0 || x == bx) break; }
    }
    for (auto& p : path)
      if (p.first > 0 && p.second > 0 && p.first < MW && p.second < MH &&
          tiles[p.second][p.first] == T_WALL)
        tiles[p.second][p.first] = T_FLOOR;
    for (int i = 1; i < (int)path.size(); i++) {
      if (inside(a, path[i].first, path[i].second)) continue;
      if (!one_door_kind(b.kind)) set_door(path[i].first, path[i].second, a);
      break;
    }
    for (int i = (int)path.size() - 2; i >= 0; i--) {
      if (inside(b, path[i].first, path[i].second)) continue;
      if (!one_door_kind(a.kind)) set_door(path[i].first, path[i].second, b);
      break;
    }
  };


  // Build the loop and attach the branches. Only ordinary rooms can gain shortcuts.
  std::vector<std::pair<int, int>> edges;
  for (int idx : order) {
    if (parent[idx] < 0) continue;
    int a = slot_to_room[parent[idx]], b = slot_to_room[idx];
    connect(rooms[a], rooms[b]);
    edges.push_back({a, b});
  }
  const int last = slot_to_room[order[loop_size - 1]];
  const int first = slot_to_room[start];
  connect(rooms[last], rooms[first]);
  edges.push_back({last, first});
  int loops = ri(*this, 0, 1);
  for (int tries = 0; tries < 24 && loops > 0; tries++) {
    int idx = order[(int)(rnd(*this) % order.size())];
    int gx = idx % GS, gy = idx / GS;
    int nb[2] = {gx < GS - 1 ? idx + 1 : -1, gy < GS - 1 ? idx + GS : -1};
    int ni = nb[(int)(rnd(*this) % 2)];
    if (ni < 0 || !chosen[ni]) continue;
    int a = slot_to_room[idx], b = slot_to_room[ni];
    if (one_door_kind(rooms[a].kind) || one_door_kind(rooms[b].kind)) continue;
    bool dup = false;
    for (auto& e : edges)
      if ((e.first == a && e.second == b) || (e.first == b && e.second == a))
        dup = true;
    if (dup) continue;
    connect(rooms[a], rooms[b]);
    edges.push_back({a, b});
    loops--;
  }

  entrance_room = slot_to_room[start];
  exit_room = slot_to_room[exit_slot];
  }

  // Stairs, start position.
  room_count = (int)rooms.size();
  const GRoom& er = rooms[entrance_room];
  const GRoom& xr = rooms[exit_room];
  px = er.cx; py = er.cy;
  tiles[xr.cy][xr.cx] = T_STAIRS;

  // Helpers for room contents.
  auto room_cell = [&](const GRoom& r, int margin = 1) {
    for (int t = 0; t < 80; t++) {
      int x = r.x + ri(*this, margin, r.w - 1 - margin);
      int y = r.y + ri(*this, margin, r.h - 1 - margin);
      if (tiles[y][x] != T_FLOOR && tiles[y][x] != T_GRASS &&
          tiles[y][x] != T_WATER)
        continue;
      if (x == px && y == py) continue;
      if (mob_at(x, y) >= 0 || item_at(x, y) >= 0 || feature_at(x, y) >= 0)
        continue;
      return std::pair<int, int>(x, y);
    }
    return std::pair<int, int>(-1, -1);
  };
  auto add_feature = [&](uint8_t type, int x, int y, int state = 0,
                         int aux = 0) {
    if (x < 1 || y < 1 || x >= MW - 1 || y >= MH - 1) return;
    uint8_t tt = tiles[y][x];
    if (tt != T_FLOOR && tt != T_GRASS && tt != T_WATER) return;
    if (feature_at(x, y) >= 0 || mob_at(x, y) >= 0) return;
    if (x == px && y == py) return;
    features.push_back({x, y, type, (uint8_t)state, (int16_t)aux});
  };
  auto add_item_at = [&](int x, int y, uint8_t id, int level) {
    if (x < 1 || y < 1 || x >= MW - 1 || y >= MH - 1) return;
    items.push_back({x, y, id, (int16_t)level});
  };
  auto add_random_item = [&](const GRoom& r, uint8_t id, int level = 0) {
    auto p = room_cell(r);
    if (p.first >= 0) add_item_at(p.first, p.second, id, level);
  };
  auto place_loot = [&](int x, int y, const LootRoll& loot, int container = -1) {
    if (container < 0) {
      add_item_at(x, y, loot.id, loot.id == IT_GOLD ? loot.gold : loot.level);
    } else {
      Feature f;
      f.x = x; f.y = y; f.type = container;
      f.loot_id = loot.id; f.loot_level = loot.level; f.loot_gold = loot.gold;
      features.push_back(f);
    }
  };

  // RegularPainter's independent terrain RNG and regional Patch parameters.
  if (!boss_floor) {
    struct TerrainPatch { float normal, feeling; int smooth; };
    static constexpr TerrainPatch water[] = {
        {.30f,.85f,5}, {.30f,.90f,4}, {.30f,.85f,6}, {.30f,.90f,4}, {.15f,.70f,6}};
    static constexpr TerrainPatch grass[] = {
        {.20f,.80f,4}, {.20f,.80f,3}, {.15f,.65f,3}, {.20f,.80f,3}, {.10f,.65f,3}};
    Random painter_rng(layout_rng.next_long());
    auto paint_patch = [&](TerrainPatch p, bool boosted, uint8_t tile) {
      const auto mask = patch(map_width,map_height,boosted ? p.feeling : p.normal,p.smooth,true,painter_rng);
      for (const GRoom& r : rooms) {
        // Until class-specific painter vetoes are ported, do not overwrite
        // quest, corridor or special-room mechanics with regional features.
        if (r.kind != RK_STANDARD && r.kind != RK_ENTRANCE && r.kind != RK_EXIT) continue;
        for (int y = r.y+1; y < r.y+r.h-1; ++y) for (int x = r.x+1; x < r.x+r.w-1; ++x)
          if (tiles[y][x] == T_FLOOR && mask[y*map_width+x] && !(x == px && y == py))
            tiles[y][x] = tile;
      }
    };
    paint_patch(water[reg],feeling == 1,T_WATER);
    paint_patch(grass[reg],feeling == 2,T_GRASS);
  } else {
    // Leave the legacy boss substrate alone until its individual painters land.
    for (const GRoom& r : rooms) {
      if (r.kind != RK_STANDARD && r.kind != RK_ENTRANCE) continue;
      auto boss_patch = [&](uint8_t tile) {
        const int w = 2 + int(rnd(*this)%2), h = 2 + int(rnd(*this)%2);
        const int x0 = r.x + 1 + int(rnd(*this)%std::max(1,r.w-1-w));
        const int y0 = r.y + 1 + int(rnd(*this)%std::max(1,r.h-1-h));
        for (int y = y0; y < std::min(y0+h,r.y+r.h-1); ++y)
          for (int x = x0; x < std::min(x0+w,r.x+r.w-1); ++x)
            if (tiles[y][x] == T_FLOOR && !(x == px && y == py)) tiles[y][x] = tile;
      };
      if (feeling == 1 || rnd(*this)%100 < 25) boss_patch(T_WATER);
      if (feeling == 2 || rnd(*this)%100 < 20) {
        boss_patch(T_GRASS);
        for (int i = 0; i < 2; ++i) {
          auto p = room_cell(r);
          if (p.first >= 0 && tiles[p.second][p.first] == T_GRASS) add_feature(F_PLANT,p.first,p.second);
        }
      }
    }
  }

  // Special rooms.
  for (const GRoom& r : rooms) {
    if (r.kind == RK_SECRET) {
      add_feature(F_CHEST, r.cx, r.cy);
      add_random_item(r, IT_GOLD, 20 + floor * 10);
      add_random_item(r, (rnd(*this) & 1) ? IT_POTION_HEAL : IT_SCROLL_MAP, 0);
      continue;
    }
    if (r.kind == RK_SHOP || r.kind == RK_QUEST) continue;
    if (r.kind != RK_SPECIAL) continue;
    switch (r.spec) {
      case SP_TREASURY:
        add_feature(F_CHEST, r.cx, r.cy);
        add_feature(F_CHEST, r.x + 1, r.y + 1);
        add_feature(F_CHEST, r.x + r.w - 2, r.y + r.h - 2);
        add_random_item(r, IT_GOLD, 40 + floor * 15);
        add_random_item(r, IT_GOLD, 40 + floor * 15);
        break;
      case SP_ARMORY:
        add_feature(F_STATUE, r.x + 1, r.y + 1);
        add_random_item(r, weapon_for_tier_rand(*this, (floor - 1) / 5 + 1), 0);
        add_random_item(r, armor_for_tier((floor - 1) / 5 + 1), 0);
        add_random_item(r, (uint8_t)ri(*this, IT_KNIFE, IT_JAVELIN), 0);
        break;
      case SP_CRYPT:
        add_feature(F_TOMB, r.x + 1, r.y + 1);
        add_feature(F_TOMB, r.x + r.w - 2, r.y + 1);
        add_feature(F_TOMB, r.x + 1, r.y + r.h - 2);
        add_feature(F_TOMB, r.x + r.w - 2, r.y + r.h - 2);
        add_random_item(r, weapon_for_tier_rand(*this, (floor - 1) / 5 + 1), 0);
        break;
      case SP_POOL:
        for (int y = r.y + 1; y < r.y + r.h - 1; y++)
          for (int x = r.x + 1; x < r.x + r.w - 1; x++)
            if ((x + y) % 2 == 0) tiles[y][x] = T_WATER;
        tiles[r.cy][r.cx] = T_FLOOR;
        add_feature(F_CHEST, r.cx, r.cy);
        add_random_item(r, IT_POTION_INVIS, 0);
        break;
      case SP_WELL:
        add_feature(F_WELL, r.cx, r.cy);
        add_random_item(r, IT_POTION_HEAL, 0);
        break;
      case SP_GARDEN:
        for (int y = r.y + 1; y < r.y + r.h - 1; y++)
          for (int x = r.x + 1; x < r.x + r.w - 1; x++)
            if (tiles[y][x] == T_FLOOR) tiles[y][x] = T_GRASS;
        for (int i = 0; i < 4; i++) {
          auto p = room_cell(r);
          if (p.first >= 0) add_feature(F_PLANT, p.first, p.second);
        }
        add_random_item(r, IT_SEED_SUNGRASS, 0);
        add_random_item(r, IT_SEED_FIREBLOOM, 0);
        break;
      case SP_LIBRARY: {
        const int book_y = r.y + 1;
        const int approach_x = std::clamp(r.door_x, r.x + 1, r.x + r.w - 2);
        const int approach_y = std::clamp(r.door_y, r.y + 1, r.y + r.h - 2);
        for (int x = r.x + 1; x < r.x + r.w - 1; x++)
          if (x != approach_x || book_y != approach_y)
            add_feature(F_BOOKSHELF, x, book_y);
        add_random_item(r, ri(*this, 0, 1) ? IT_SCROLL_IDENTIFY : IT_SCROLL_CURSE);
        static constexpr uint8_t scrolls[] = {IT_SCROLL_IDENTIFY, IT_SCROLL_CURSE,
            IT_SCROLL_MAP, IT_SCROLL_TELE, IT_SCROLL_RECHARGE, IT_SCROLL_TERROR};
        for (int i = 1, n = (ri(*this, 1, 3) + ri(*this, 1, 3)) / 2; i < n; ++i)
          add_random_item(r, scrolls[ri(*this, 0, 5)]);
        break;
      }
      case SP_STORAGE:
        tiles[r.door_y][r.door_x] = T_FLOOR;
        add_feature(F_BARRICADE, r.door_x, r.door_y, 0, 2);
        for (int i = 0, count = ri(*this, 3, 4); i < count; ++i) {
          const uint8_t prizes[] = {IT_FOOD_RATION, IT_POTION_HEAL,
                                    IT_SCROLL_IDENTIFY, IT_GOLD};
          uint8_t id = prizes[ri(*this, 0, 3)];
          add_random_item(r, id, id == IT_GOLD ? ri(*this, 30, 80) : 0);
        }
        break;
      case SP_LAB: {
        int x = (rnd(*this) & 1) ? r.x + 1 : r.x + r.w - 2;
        int y = (rnd(*this) & 1) ? r.y + 1 : r.y + r.h - 2;
        if (r.door_x <= r.x) x = r.x + r.w - 2;
        else if (r.door_x >= r.x + r.w - 1) x = r.x + 1;
        else if (r.door_y <= r.y) y = r.y + r.h - 2;
        else y = r.y + 1;
        add_feature(F_ALCHEMY, x, y);
        add_random_item(r, IT_POTION_HEAL, 0);
        add_random_item(r, IT_STONE_AUG, 0);
        break;
      }
      case SP_STATUE:
        add_feature(F_STATUE, r.cx, r.cy);
        add_random_item(r, weapon_for_tier_rand(*this, (floor - 1) / 5 + 1), 0);
        add_random_item(r, armor_for_tier((floor - 1) / 5 + 1), 0);
        break;
      case SP_SENTRY:
        add_feature(F_STATUE, r.x + 1, r.y + 1);
        add_feature(F_STATUE, r.x + r.w - 2, r.y + 1);
        add_feature(F_CHEST, r.cx, r.cy);
        {
          auto p = room_cell(r);
          if (p.first >= 0) {
            Mob m;
            m.x = p.first; m.y = p.second;
            m.vx = (float)m.x; m.vy = (float)m.y;
            m.type = M_GUARD;
            m.maxhp = mob_def(M_GUARD).hp;
            m.hp = m.maxhp;
            m.atk = mob_def(M_GUARD).atk;
            m.def = mob_def(M_GUARD).def;
            mobs.push_back(m);
          }
        }
        break;
      case SP_TRAPS:
      case SP_TOXIC:
      case SP_FIRE:
        for (int i = 0; i < 4; i++) {
          auto p = room_cell(r);
          if (p.first >= 0) add_feature(F_TRAP, p.first, p.second);
        }
        add_feature(F_CHEST, r.cx, r.cy);
        add_random_item(r, IT_POTION_HEAL, 0);
        break;
      case SP_RUNESTONE:
        add_random_item(r, IT_STONE_AUG, 0);
        add_random_item(r, (uint8_t)ri(*this, IT_RING_MIGHT, IT_RING_ELEMENTS), 0);
        break;
      case SP_VAULT: {
        // Two prizes on opposite sides, neither immediately inside the door.
        // The second category falls back to a ring until artifacts exist.
        int dx = 1, dy = 0;
        if (grid_distance(r.cx - 1, r.cy, r.door_x, r.door_y) <= 1 ||
            grid_distance(r.cx + 1, r.cy, r.door_x, r.door_y) <= 1) { dx = 0; dy = 1; }
        const uint8_t wand = random_wand(*this, region() + 1);
        const uint8_t ring = uint8_t(ri(*this, IT_RING_MIGHT, IT_RING_ELEMENTS));
        const bool swap = ri(*this, 0, 1);
        place_loot(r.cx - dx, r.cy - dy, {swap ? ring : wand}, F_CHEST_CRYSTAL);
        place_loot(r.cx + dx, r.cy + dy, {swap ? wand : ring}, F_CHEST_CRYSTAL);
        break;
      }
      default:
        break;
    }
  }

  // NPCs / shop / quest rooms.
  auto spawn_npc = [&](uint8_t type, int x, int y) {
    Mob m;
    m.x = x; m.y = y; m.vx = (float)x; m.vy = (float)y;
    m.type = type;
    m.maxhp = m.hp = 999;
    m.atk = m.def = 0;
    mobs.push_back(m);
  };
  if (!boss_floor) {
    if (floor == ghost_floor) spawn_npc(M_NPC_GHOST, xr.cx, xr.cy - 1);
    if (floor == wand_floor) spawn_npc(M_NPC_WANDMAKER, er.cx, er.cy - 1);
  }
  for (const GRoom& r : rooms) {
    if (r.kind == RK_QUEST) {
      if (floor == smith_floor) spawn_npc(M_NPC_SMITH, r.cx, r.cy);
      else if (floor == imp_floor) spawn_npc(M_NPC_IMP, r.cx, r.cy);
    } else if (r.kind == RK_SHOP) {
      spawn_npc(imp_shop ? M_NPC_IMP : M_NPC_SHOP, r.cx, r.cy);
    }
  }

  // Locked special rooms get a key somewhere in a standard room.
  int locked_count = 0, locked_chests = 0;
  for (int y = 1; y < map_height - 1; y++)
    for (int x = 1; x < map_width - 1; x++)
      if (tiles[y][x] == T_DOOR_LOCKED) locked_count++;
  for (const Feature& f : features)
    if (f.type == F_CHEST_LOCKED) locked_chests++;
  std::vector<int> std_rooms;
  for (int i = 0; i < (int)rooms.size(); i++)
    if (rooms[i].kind == RK_STANDARD) std_rooms.push_back(i);
  for (int i : std_rooms) {
    const auto& r = rooms[i];
    spawn_rooms.push_back({uint8_t(r.x), uint8_t(r.y), uint8_t(r.w), uint8_t(r.h)});
  }
  auto drop_cell = [&]() {
    if (!std_rooms.empty()) {
      int start_room = ri(*this, 0, (int)std_rooms.size() - 1);
      for (int i = 0; i < (int)std_rooms.size(); ++i) {
        auto p = room_cell(rooms[std_rooms[(start_room + i) % std_rooms.size()]]);
        if (p.first >= 0) return p;
      }
    }
    return std::pair<int, int>{-1, -1};
  };
  auto drop_key = [&](uint8_t id) {
    auto p = drop_cell();
    if (p.first >= 0) add_item_at(p.first, p.second, id, 0);
    return p.first >= 0;
  };
  // Standard loot (SPD RegularLevel.createItems).
  auto add_floor_item = [&](const LootRoll& lr) {
    auto p = drop_cell();
    if (p.first < 0) return;
    int roll = ri(*this, 0, 19);
    bool upgradable = item_def(lr.id).kind == K_WEAPON ||
                      item_def(lr.id).kind == K_ARMOR ||
                      item_def(lr.id).kind == K_WAND || item_def(lr.id).kind == K_RING;
    if (roll == 5 && floor > 1) {
      place_loot(p.first, p.second, lr, F_CHEST_MIMIC);
    } else if (upgradable && ri(*this, 0, 3 - std::min<int>(lr.level, 3)) == 0) {
      int type = floor > 1 && ri(*this, 0, 9) == 0 ? F_CHEST_MIMIC : F_CHEST_LOCKED;
      place_loot(p.first, p.second, lr, type);
      if (type == F_CHEST_LOCKED && !drop_key(IT_GOLDEN_KEY)) features.back().type = F_CHEST;
    } else {
      place_loot(p.first, p.second, lr, roll == 0 ? int(F_REMAINS) :
                 roll <= 5 ? int(F_CHEST) : -1);
    }
  };
  // Traps: Random.NormalIntRange(2, 3 + depth/5), doubled on trap floors.
  const auto trap_roll_a = rnd(*this) >> 16;
  const auto trap_roll_b = rnd(*this) >> 16;
  int nTraps = boss_floor ? 0 : 2 + (trap_roll_a + trap_roll_b) * (2 + floor / 5) / 131072;
  if (traps_feeling) nTraps *= 2;
  for (int i = 0; i < nTraps; i++) {
    if (std_rooms.empty()) break;
    const GRoom& r = rooms[std_rooms[(int)(rnd(*this) % std_rooms.size())]];
    auto p = room_cell(r);
    if (p.first >= 0) add_feature(floor == 1 ? F_TRAP_FOUND : F_TRAP,
                                  p.first, p.second, 0, floor == 1 ? 1 : 0);
  }

  // Quest token target.
  if (token_quest == 1 && tokens < 3 && floor >= 16 && !std_rooms.empty()) {
    const GRoom& r = rooms[std_rooms[(int)(rnd(*this) % std_rooms.size())]];
    auto p = room_cell(r);
    if (p.first >= 0) add_feature(F_TOKEN, p.first, p.second);
  }

  // Monsters.
  if (boss_floor) {
    if (!boss_dead[reg]) {
      static const int OFF[8][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1},
                                    {-1, -1}, {1, 1}, {-1, 1}, {1, -1}};
      for (int i = 0; i < 8; i++) {
        int bx = xr.cx + OFF[i][0], by = xr.cy + OFF[i][1];
        if (floor_spawn_cell(*this, bx, by, REGIONS[reg].boss)) {
          mobs.push_back(make_floor_mob(REGIONS[reg].boss, bx, by, Mob::Wandering));
          break;
        }
      }
    }
  } else {
    int limit = floor == 1 ? 8 : 3 + floor % 5 + ri(*this, 0, 2);
    if (large) limit = (int)ceilf(limit * 1.33f);
    mob_cap = (floor == 1) ? 0 : limit;
    respawn_t = plan_feeling[floor - 1] == 6 ? 100 : 150;
    int16_t dist[MH][MW];
    spawn_distances(*this, dist, false, 8);
    for (int i = int(std_rooms.size()) - 1; i > 0; --i)
      std::swap(std_rooms[i], std_rooms[ri(*this, 0, i)]);
    int remaining = limit;
    uint8_t type = next_spawn_type();
    auto place = [&](const GRoom& r) {
      for (int attempt = 0; attempt < 30; ++attempt) {
        int x = r.x + ri(*this, 1, r.w - 2), y = r.y + ri(*this, 1, r.h - 2);
        if (dist[y][x] >= 0 || !floor_spawn_cell(*this, x, y, type) || item_at(x, y) >= 0) continue;
        if ((x - px) * (x - px) + (y - py) * (y - py) <= 65 && los(px, py, x, y)) continue;
        int f = feature_at(x, y);
        if (f >= 0 && (features[f].type == F_TRAP || features[f].type == F_TRAP_FOUND ||
                       features[f].type == F_PLANT)) continue;
        mobs.push_back(make_floor_mob(type, x, y, Mob::Sleeping));
        if (--remaining) type = next_spawn_type();
        return true;
      }
      return false;
    };
    for (int pass = 0; pass < 30 && remaining; ++pass) {
      for (int i : std_rooms) {
        if (!remaining) break;
        if (place(rooms[i]) && remaining && floor > 1 && ri(*this, 0, 3) == 0)
          place(rooms[i]);
      }
    }

    // Guaranteed supplies are loose heaps, after population and traps are placed.
    auto supply = [&](uint8_t id) {
      auto p = drop_cell();
      if (p.first >= 0) add_item_at(p.first, p.second, id, 0);
    };
    for (int i = 0; i < locked_count; i++) drop_key(IT_KEY);
    for (int i = 0; i < locked_chests; i++) drop_key(IT_GOLDEN_KEY);
    for (const GRoom& r : rooms) {
      switch (r.spec) {
        case SP_POOL: supply(IT_POTION_INVIS); break;
        case SP_SENTRY: supply(IT_POTION_HASTE); break;
        case SP_STORAGE: supply(IT_POTION_FLAME); break;
        case SP_TRAPS: supply(IT_POTION_LEVITATE); break;
        case SP_VAULT: supply(IT_CRYSTAL_KEY); break;
      }
    }
    supply(ri(*this, 0, 4) ? IT_FOOD_RATION : IT_FOOD_PASTY);
    if (large) supply(ri(*this, 0, 4) ? IT_FOOD_RATION : IT_FOOD_PASTY);
    uint8_t guaranteed = plan_guaranteed[floor - 1];
    if (guaranteed & 1) supply(IT_POTION_STR);
    if (guaranteed & 2) supply(IT_SCROLL_UP);
    int itemRoll = ri(*this, 0, 99);
    int nItems = 3 + (itemRoll < 60 ? 0 : (itemRoll < 90 ? 1 : 2)) + (large ? 2 : 0);
    for (int i = 0; i < nItems; i++) add_floor_item(roll_loot(*this, true, true));
  }

  cursor_x = px;
  cursor_y = py;
  camx = px * 16 - TOP_W / 2;
  camy = py * 16 - TOP_H / 2;
  recompute_fov();
}

void Game::debug_dump() {
  int nstd = 0, nspec = 0, nsec = 0, nshop = 0, nquest = 0;
  int doors = 0, locked = 0, secrets = 0, chests = 0, traps = 0;
  for (int y = 0; y < MH; y++)
    for (int x = 0; x < MW; x++) {
      if (tiles[y][x] == T_DOOR) doors++;
      if (tiles[y][x] == T_DOOR_LOCKED) locked++;
      if (tiles[y][x] == T_SECRET) secrets++;
    }
  for (const Feature& f : features) {
    if (f.type == F_CHEST || f.type == F_CHEST_LOCKED) chests++;
    if (f.type == F_TRAP) traps++;
  }
  // Count rooms by flood-filling floor components bigger than 8 cells.
  bool seen2[MH][MW] = {};
  for (int y = 0; y < MH; y++)
    for (int x = 0; x < MW; x++) {
      if (seen2[y][x] || tiles[y][x] == T_WALL || tiles[y][x] == T_SECRET || tiles[y][x] == T_CHASM)
        continue;
      int n = 0, sx = x, sy = y;
      std::vector<std::pair<int, int>> st{{x, y}};
      seen2[y][x] = true;
      while (!st.empty()) {
        auto p = st.back(); st.pop_back(); n++;
        static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
          int nx = p.first + DX[k], ny = p.second + DY[k];
          if (nx < 0 || ny < 0 || nx >= MW || ny >= MH || seen2[ny][nx])
            continue;
          uint8_t tt = tiles[ny][nx];
          if (tt == T_WALL || tt == T_SECRET || tt == T_CHASM) continue;
          seen2[ny][nx] = true;
          st.push_back({nx, ny});
        }
      }
      if (n >= 9) nstd++;
    }
  int floor_keys = 0;
  for (const FloorItem& it : items)
    if (it.id == IT_KEY) floor_keys++;
  dlog("[gen] f=%d room_count=%d doors=%d locked=%d floor_keys=%d secret_tiles=%d "
       "items=%d mobs=%d feats=%d chests=%d traps=%d",
       floor, room_count, doors, locked, floor_keys, secrets, (int)items.size(),
       (int)mobs.size(), (int)features.size(), chests, traps);
  if (floor <= 5) {
    for (int y = 0; y < MH; y++) {
      char line[MW + 1];
      for (int x = 0; x < MW; x++) {
        uint8_t tt = tiles[y][x];
        char c = '#';
        if (tt == T_FLOOR) c = '.';
        else if (tt == T_GRASS) c = ',';
        else if (tt == T_WATER) c = '~';
        else if (tt == T_CHASM) c = 'O';
        else if (tt == T_DOOR) c = '+';
        else if (tt == T_DOOR_OPEN) c = '\'';
        else if (tt == T_DOOR_LOCKED) c = 'L';
        else if (tt == T_SECRET) c = 'S';
        else if (tt == T_STAIRS) c = '>';
        if (x == px && y == py) c = '@';
        line[x] = c;
      }
      line[MW] = 0;
      dlog("[map] %s", line);
    }
  }
}

void Game::new_run(int hero_class, uint32_t s) {
  hero = hero_class;
  seed = s ? s : 0x9E3779B9u;
  rng = seed;
  floor = 1;
  zoom = zoom_target = 1.0f;
  turns = 0;
  keys = golden_keys = crystal_keys = gold = shield = 0;
  legacy_keys = false;
  loot_second_deck = (seed >> 31) != 0;
  memset(loot_remaining, 0, sizeof(loot_remaining));
  level = 1;
  xp = 0;
  xpnext = 10;
  quest = 0;
  tokens = token_quest = smith_used = imp_state = 0;
  ghost_floor = (uint8_t)(2 + ((seed >> 8) % 3));
  wand_floor = (uint8_t)(7 + ((seed >> 12) % 3));
  smith_floor = (uint8_t)(12 + ((seed >> 16) % 3));
  imp_floor = (uint8_t)(17 + ((seed >> 20) % 3));
  ghost_state = ghost_boss_dead = 0;
  wand_state = have_dust = 0;
  smith_state = ore_count = 0;
  imp_kills = imp_target = 0;
  buyback_n = 0;
  memset(boss_dead, 0, sizeof(boss_dead));
  static const int hp_base[6] = {25, 20, 18, 20, 22, 20};
  maxhp = hp_base[hero % 6];
  hp = maxhp;
  inv_n = 0;
  weapon = IT_DAGGER;
  weapon_level = 0;
  armor = IT_CLOTH_ARMOR;
  armor_level = 0;
  add_item(IT_POTION_HEAL, 2);
  add_item(IT_FOOD_RATION, 1);
  targeting = false;
  dead = false;
  won = false;
  dialog = false;
  paused = backpack = shop_open = false;
  clear_path();
  gen_level();
  pvx = (float)px; pvy = (float)py; panim_ms = 0;
  message("你来到了地牢入口。");
}

// ---------------------------------------------------------------------------
// Inventory
// ---------------------------------------------------------------------------
bool Game::remove_item(int idx, int count) {
  if (idx < 0 || idx >= inv_n) return false;
  inv[idx].count = (uint8_t)std::max(0, inv[idx].count - count);
  if (inv[idx].count <= 0) {
    for (int i = idx; i < inv_n - 1; i++) inv[i] = inv[i + 1];
    inv_n--;
  }
  return true;
}

void Game::add_item(uint8_t id, int count, int level) {
  if (id == IT_GOLD) { gold += count; return; }
  if (id == IT_KEY) { keys += count; return; }
  if (id == IT_GOLDEN_KEY) { golden_keys += count; return; }
  if (id == IT_CRYSTAL_KEY) { crystal_keys += count; return; }
  const ItemDef& d = item_def(id);
  if (item_is_stackable(id)) {
    for (int i = 0; i < inv_n; i++) {
      if (inv[i].id == id) {
        inv[i].count = (uint8_t)std::min(99, inv[i].count + count);
        return;
      }
    }
  }
  if (inv_n >= bag_capacity()) { message("背包满了！"); return; }
  Item it;
  it.id = id;
  it.count = (uint8_t)count;
  it.level = (uint8_t)level;
  it.charges = d.kind == K_WAND ? d.max_charge : 0;
  inv[inv_n++] = it;
}

void Game::equip_item(int idx) {
  if (idx < 0 || idx >= inv_n) return;
  Item& it = inv[idx];
  uint8_t kind = item_def(it.id).kind;
  if (kind == K_WEAPON) {
    uint8_t old = weapon, oldl = weapon_level;
    weapon = it.id;
    weapon_level = it.level;
    char b[96];
    snprintf(b, sizeof(b), "装备了 %s", item_def(weapon).name);
    message(b);
    inv[idx].id = old;
    inv[idx].level = oldl;
    inv[idx].count = 1;
    if (old == IT_NONE) remove_item(idx, 1);
  } else if (kind == K_ARMOR) {
    uint8_t old = armor, oldl = armor_level;
    armor = it.id;
    armor_level = it.level;
    char b[96];
    snprintf(b, sizeof(b), "穿上了 %s", item_def(armor).name);
    message(b);
    inv[idx].id = old;
    inv[idx].level = oldl;
    inv[idx].count = 1;
    if (old == IT_NONE) remove_item(idx, 1);
  }
}

void Game::use_item(int idx) {
  if (idx < 0 || idx >= inv_n) return;
  Item& it = inv[idx];
  const ItemDef& d = item_def(it.id);
  switch (d.kind) {
    case K_USE:
      if (it.id == IT_STONE_AUG) {
        if (weapon != IT_NONE) { weapon_level++; message("强化石强化了武器。"); }
        else { armor_level++; message("强化石强化了护甲。"); }
      } else if (it.id == IT_ANKH) {
        message("安卡会在你死亡时自动生效。");
        return;
      }
      if (it.id == IT_POTION_HEAL) {
        if (hp >= maxhp) { message("生命值已满。"); return; }
        hp = std::min(maxhp, hp + d.power);
        message("你喝下治疗药水。");
      } else if (it.id == IT_POTION_STR) {
        maxhp += 2; hp = std::min(maxhp, hp + 2);
        message("力量涌入身体，最大生命 +2。");
      } else if (it.id == IT_POTION_SHIELD) {
        shield += d.power;
        message("一层护盾包围了你。");
      } else if (it.id == IT_POTION_HASTE) {
        haste_t = d.power;
        message("你感到行动变得迅捷。");
      } else if (it.id == IT_POTION_INVIS) {
        invis_t = d.power;
        message("你的身影逐渐消失。");
      } else if (it.id == IT_POTION_LEVITATE) {
        levit_t = d.power;
        message("你的身体漂浮起来。");
      } else if (it.id == IT_POTION_MIND) {
        mind_t = d.power;
        for (int y = 0; y < MH; y++)
          for (int x = 0; x < MW; x++) seen[y][x] |= 2;
        message("你感知到了整层地牢。");
      } else if (it.id == IT_POTION_XP) {
        xp += d.power + floor;
        while (xp >= xpnext) {
          xp -= xpnext; level++; xpnext = level * 10;
          maxhp += 5; hp = std::min(maxhp, hp + 5);
        }
        message("经验涌入你的身体。");
      } else if (it.id == IT_POTION_MIGHT) {
        might_t = d.power;
        message("你的攻击变得更有力。");
      } else if (it.id == IT_POTION_TOXIC || it.id == IT_POTION_FLAME) {
        int dmg = d.power + floor / 3;
        for (size_t i = 0; i < mobs.size(); i++) {
          if (!mobs[i].alive) continue;
          if (is_shopkeeper(mobs[i].type, floor) &&
              absi(mobs[i].x - px) <= 3 && absi(mobs[i].y - py) <= 3) {
            harm_shopkeeper();
            continue;
          }
          if (!is_npc(mobs[i].type) && absi(mobs[i].x - px) <= 3 &&
              absi(mobs[i].y - py) <= 3)
            damage_mob((int)i, dmg);
        }
        message(it.id == IT_POTION_TOXIC ? "毒气弥漫开来。" : "烈焰席卷四周。");
      } else if (it.id == IT_SCROLL_MAP) {
        for (int y = 0; y < MH; y++)
          for (int x = 0; x < MW; x++) seen[y][x] |= 2;
        message("卷轴揭示了本层地图。");
      } else if (it.id == IT_SCROLL_UP) {
        if (weapon != IT_NONE) { weapon_level++; message("你的武器变得更强了。"); }
        else { armor_level++; message("你的护甲变得更强了。"); }
      } else if (it.id == IT_SCROLL_TELE) {
        for (int tries = 0; tries < 200; ++tries) {
          int x = ri(*this, 1, map_width - 2), y = ri(*this, 1, map_height - 2);
          if (!walkable(x, y) || mob_at(x, y) >= 0) continue;
          px = x; py = y;
          break;
        }
        pvx = (float)px; pvy = (float)py;
        cursor_x = px; cursor_y = py;
        recompute_fov();
        message("你被传送到本层的另一个角落。");
      } else if (it.id == IT_SCROLL_IDENTIFY || it.id == IT_SCROLL_CURSE) {
        message(it.id == IT_SCROLL_IDENTIFY ? "所有物品都已被鉴定。"
                                           : "装备上的诅咒被驱散了。");
      } else if (it.id == IT_SCROLL_TERROR || it.id == IT_SCROLL_LULLABY) {
        for (Mob& m : mobs) {
          if (!m.alive) continue;
          if (absi(m.x - px) > 6 || absi(m.y - py) > 6) continue;
          if (is_shopkeeper(m.type, floor)) { harm_shopkeeper(); continue; }
          if (is_npc(m.type)) continue;
          if (it.id == IT_SCROLL_TERROR) m.terror_t = 15;
          else m.sleep_t = 15;
        }
        message(it.id == IT_SCROLL_TERROR ? "敌人陷入恐惧！" : "敌人陷入沉睡。");
      } else if (it.id == IT_SCROLL_RECHARGE) {
        for (int i = 0; i < inv_n; i++)
          if (item_def(inv[i].id).kind == K_WAND)
            inv[i].charges = std::min<int>(item_def(inv[i].id).max_charge,
                                           inv[i].charges + 2);
        message("法杖的充能被补满了。");
      } else if (it.id == IT_SCROLL_RAGE) {
        rage_t = 10;
        message("敌人陷入狂暴，开始互相攻击。");
      } else if (it.id == IT_SCROLL_CHALLENGE) {
        int made = 0;
        for (int t = 0; t < 20 && made < 3; t++) {
          int x = px + ri(*this, -2, 2), y = py + ri(*this, -2, 2);
          if (x < 0 || y < 0 || x >= MW || y >= MH) continue;
          if (!walkable(x, y) || mob_at(x, y) >= 0) continue;
          Mob m;
          m.x = x; m.y = y; m.vx = (float)x; m.vy = (float)y;
          m.type = REGIONS[region()].mobs[ri(*this, 0, 3)];
          const MobDef& md = mob_def(m.type);
          m.maxhp = md.hp + floor * 2; m.atk = md.atk + floor / 8;
          m.def = md.def + floor / 10; m.hp = m.maxhp;
          mobs.push_back(m); made++;
        }
        message("更多敌人被召唤了出来！");
      } else if (it.id == IT_SCROLL_TRANSMUTE) {
        int n = 0;
        for (int i = 0; i < inv_n; i++) if (can_transmute(i)) n++;
        if (n == 0) { message("没有可以嬗变的物品。"); return; }
        transmute_open = true;
        transmute_sel = 0;
        transmute_page = 0;
        return;
      }
      break;
    case K_FOOD:
      hp = std::min(maxhp, hp + d.power);
      message("你吃了一些食物。");
      break;
    case K_SEED:
      if (it.id == IT_SEED_SUNGRASS) {
        hp = std::min(maxhp, hp + d.power);
        message("阳草治愈了你的伤口。");
      } else if (it.id == IT_SEED_FIREBLOOM) {
        for (size_t i = 0; i < mobs.size(); i++)
          if (mobs[i].alive && !is_npc(mobs[i].type) &&
              absi(mobs[i].x - px) <= 2 && absi(mobs[i].y - py) <= 2)
            damage_mob((int)i, d.power + floor / 3);
        message("火焰花在脚下爆开！");
      } else if (it.id == IT_SEED_BLINDWEED) {
        for (Mob& m : mobs)
          if (m.alive && !is_npc(m.type) && absi(m.x - px) <= 4 &&
              absi(m.y - py) <= 4)
            m.sleep_t = 12;
        message("致盲花粉让敌人昏昏欲睡。");
      }
      break;
    case K_WAND:
      if (it.charges == 0) { message("法杖还没有充能。"); return; }
      targeting = !targeting;
      target_item = idx;
      message(targeting ? "选择施法目标（B 取消）。" : "取消施法。");
      return;
    case K_BOMB:
    case K_MISSILE:
      targeting = !targeting;
      target_item = idx;
      message(targeting ? "选择投掷目标（B 取消）。" : "取消投掷。");
      return;
    case K_BAG:
      if (it.id == IT_BAG_POTION) bag_potion = true;
      else if (it.id == IT_BAG_SCROLL) bag_scroll = true;
      else bag_wand = true;
      message("背包容量增加了 5 格。");
      break;
    case K_RING:
      equip_ring(idx);
      return;
    case K_WEAPON:
    case K_ARMOR:
      equip_item(idx);
      return;
    default:
      message("这个东西不能直接使用。");
      return;
  }
  remove_item(idx, 1);
  advance_turn();
}

static CombatStyle wand_style(uint8_t id) {
  switch (id) {
    case IT_WAND_FIRE: return CombatStyle::Fire;
    case IT_WAND_FROST: return CombatStyle::Frost;
    case IT_WAND_LIGHTNING: return CombatStyle::Lightning;
    case IT_WAND_BLAST: return CombatStyle::Blast;
    default: return CombatStyle::Arcane;
  }
}

void Game::cast_wand(int idx, int tx, int ty) {
  if (idx < 0 || idx >= inv_n) { targeting = false; return; }
  Item& it = inv[idx];
  const ItemDef& d = item_def(it.id);
  if (d.kind != K_WAND || it.charges == 0) { targeting = false; return; }
  if (absi(tx - px) + absi(ty - py) > 6 || !los(px, py, tx, ty)) {
    message("目标不可达。");
    return;
  }
  for (Mob& m : mobs)
    if (m.alive && is_shopkeeper(m.type, floor) && absi(m.x - tx) <= 2 &&
        absi(m.y - ty) <= 2)
      harm_shopkeeper();
  combat_strike(hero_combat, px, py, tx, ty, wand_style(it.id));
  combat_delay = 170;
  int dmg = d.power + it.level * 2 + ri(*this, 0, 2);
  auto hit_area = [&](int radius, int bonus) {
    int hits = 0;
    for (size_t i = 0; i < mobs.size(); i++) {
      Mob& m = mobs[i];
      if (!m.alive || is_npc(m.type)) continue;
      if (absi(m.x - tx) > radius || absi(m.y - ty) > radius) continue;
      if (!los(px, py, m.x, m.y)) continue;
      damage_mob((int)i, dmg + bonus);
      hits++;
    }
    return hits;
  };
  auto hit_one = [&](int bonus) {
    for (size_t i = 0; i < mobs.size(); i++) {
      Mob& m = mobs[i];
      if (!m.alive || is_npc(m.type)) continue;
      if (absi(m.x - tx) > 1 || absi(m.y - ty) > 1) continue;
      if (!los(px, py, m.x, m.y)) continue;
      damage_mob((int)i, dmg + bonus);
      return (int)i;
    }
    return -1;
  };

  switch (it.id) {
    case IT_WAND_WARDING:
      shield += 8 + it.level * 2;
      message("守护法杖为你附上了护盾。");
      break;
    case IT_WAND_REGROWTH:
      hp = std::min(maxhp, hp + 10 + it.level * 2);
      message("再生法杖治愈了你的伤口。");
      break;
    case IT_WAND_TRANSFUSION: {
      int before = 0;
      (void)before;
      int mi = hit_one(0);
      if (mi >= 0) {
        hp = std::min(maxhp, hp + dmg / 2 + 2);
        (void)mi;
        message("你吸取了目标的生命。");
      }
      break;
    }
    case IT_WAND_DISINTEGRATION:
      for (size_t i = 0; i < mobs.size(); i++) {
        Mob& m = mobs[i];
        if (!m.alive || is_npc(m.type)) continue;
        if (absi(m.x - tx) <= 1 && absi(m.y - ty) <= 1 &&
            los(px, py, m.x, m.y))
          damage_mob((int)i, dmg + 3);
      }
      message("瓦解射线贯穿了目标区域。");
      break;
    case IT_WAND_FIRE:
    case IT_WAND_LIGHTNING:
      if (hit_area(1, 2) == 0) message("法术击中了空处。");
      break;
    case IT_WAND_PRISMATIC:
      hit_area(1, 5);
      break;
    case IT_WAND_BLAST: {
      for (size_t i = 0; i < mobs.size(); i++) {
        Mob& m = mobs[i];
        if (!m.alive || is_npc(m.type)) continue;
        if (absi(m.x - tx) > 1 || absi(m.y - ty) > 1) continue;
        damage_mob((int)i, dmg);
        if (m.alive) {
          int dx = (m.x > tx) - (m.x < tx), dy = (m.y > ty) - (m.y < ty);
          for (int step = 0; step < 2; step++) {
            int nx = m.x + dx, ny = m.y + dy;
            if (walkable(nx, ny) && mob_at(nx, ny) < 0) { m.x = nx; m.y = ny; }
          }
          m.vx = (float)m.x; m.vy = (float)m.y;
        }
      }
      message("冲击波把敌人推开了。");
      break;
    }
    case IT_WAND_CORROSION:
      for (size_t i = 0; i < mobs.size(); i++) {
        Mob& m = mobs[i];
        if (!m.alive || is_npc(m.type)) continue;
        if (absi(m.x - tx) > 1 || absi(m.y - ty) > 1) continue;
        if (m.def > 0) m.def--;
        damage_mob((int)i, dmg);
      }
      break;
    case IT_WAND_EARTH: {
      int mi = hit_one(0);
      if (mi >= 0) mobs[mi].sleep_t = 10;
      message("藤蔓缠住了目标。");
      break;
    }
    case IT_WAND_FROST:
      for (size_t i = 0; i < mobs.size(); i++) {
        Mob& m = mobs[i];
        if (!m.alive || is_npc(m.type)) continue;
        if (absi(m.x - tx) > 1 || absi(m.y - ty) > 1) continue;
        damage_mob((int)i, dmg);
        if (m.alive) m.sleep_t = 8;
      }
      break;
    case IT_WAND_CORRUPTION:
      hit_area(1, 0);
      break;
    default:
      if (hit_one(0) < 0) message("法术击中了空处。");
      break;
  }
  it.charges--;
  combat_delay = 0;
  targeting = false;
  target_item = -1;
  advance_turn();
}

void Game::use_feature(int idx) {
  if (idx < 0 || idx >= (int)features.size()) return;
  Feature& f = features[idx];
  int tier = region() + 1;
  switch (f.type) {
    case F_CHEST_MIMIC: {
      Mob m;
      m.x = f.x; m.y = f.y; m.vx = (float)f.x; m.vy = (float)f.y;
      m.type = M_MIMIC;
      m.maxhp = m.hp = mob_def(M_MIMIC).hp;
      m.atk = mob_def(M_MIMIC).atk;
      m.def = mob_def(M_MIMIC).def;
      m.ai = Mob::Hunting;
      m.target = py * MW + px;
      f.type = F_MIMIC_LOOT;
      f.aux = (int16_t)mobs.size();
      mobs.push_back(m);
      message("宝箱怪扑了出来！");
      advance_turn();
      break;
    }
    case F_CHEST:
    case F_REMAINS:
    case F_CHEST_CRYSTAL:
    case F_CHEST_LOCKED: {
      if (f.type == F_CHEST_CRYSTAL) {
        if (crystal_keys <= 0) { message("这个宝箱需要一把水晶钥匙。"); return; }
        --crystal_keys;
      }
      if (f.type == F_CHEST_LOCKED) {
        int& held = legacy_keys ? keys : golden_keys;
        if (held <= 0) { message("箱子锁住了，需要一把金钥匙。"); return; }
        held--;
      }
      if (f.loot_id == IT_GOLD) {
        gain_gold(*this, f.loot_gold);
        message("箱子里装满了金币。");
      } else if (f.loot_id != IT_NONE) {
        add_item(f.loot_id, 1, f.loot_level);
        char b[96];
        snprintf(b, sizeof(b), "你找到了 %s", item_name(f.loot_id));
        message(b);
      } else {
        give_loot(tier, true);
      }
      f.loot_id = IT_NONE;
      f.loot_gold = 0;
      if (f.type == F_REMAINS) features.erase(features.begin() + idx);
      else f.type = F_CHEST_OPEN;
      advance_turn();
      break;
    }
    case F_WELL:
      hp = maxhp;
      f.type = F_WELL_USED;
      message("井水恢复了你的全部生命。");
      advance_turn();
      break;
    case F_BOOKSHELF:
      message("这里没有可以交互的东西。");
      break;
    case F_ALCHEMY:
      alchemy_open = true;
      alchemy_sel = 0;
      break;
    case F_STATUE:
      gold += 15 + tier * 5;
      f.type = F_STATUE_USED;
      message("你在雕像下找到了供品金币。");
      advance_turn();
      break;
    case F_TOMB:
      give_loot(tier, true);
      hp -= 3;
      f.type = F_TOMB_USED;
      message("你撬开了古墓，灰尘让你受了点伤。");
      advance_turn();
      break;
    case F_PLANT:
      hp = std::min(maxhp, hp + 6);
      features.erase(features.begin() + idx);
      message("你吃下植物，恢复了少量生命。");
      advance_turn();
      break;
    case F_SIGN: {
      static const char* signs[5] = {
          "下水道的墙壁上刻着：小心史莱姆王。",
          "监狱告示：囚犯不得离开牢房。",
          "矿工留言：洞穴深处有 DM-300 在巡逻。",
          "矮人布告：国王的宝库禁止入内。",
          "恶魔的刻痕：尤格-泽瓦在等待献祭。"};
      message(signs[region()]);
      break;
    }
    case F_BARRICADE:
      if (--f.aux <= 0) {
        features.erase(features.begin() + idx);
        message("路障被破坏了。");
      } else {
        message("路障挡住了去路，再攻击一次。");
      }
      advance_turn();
      break;
    case F_TOKEN:
      tokens++;
      features.erase(features.begin() + idx);
      message(tokens >= 3 ? "你集齐了 3 枚徽记，回去找小鬼。"
                          : "你拾取了一枚徽记。");
      advance_turn();
      break;
    default:
      message("这里没有可以交互的东西。");
      break;
  }
}

void Game::trigger_trap(int idx, Mob* who) {
  Feature& f = features[idx];
  if (f.type != F_TRAP_FOUND && (f.type != F_TRAP || who)) return;
  if ((who && (who->type == M_BAT || who->type == M_SWARM || who->type == M_WRAITH ||
               who->type == M_EYE)) || (!who && levit_t > 0)) return;
  const bool dart = f.aux == 1;
  int raw = 3 + floor / 4;
  if (dart) {
    const uint32_t a = rnd(*this) >> 16, b = rnd(*this) >> 16;
    raw = 4 + int((a + b) * 5 / 131072);
  }
  f.type = F_TRAP_DISARMED;
  if (who) {
    damage_mob(int(who - mobs.data()), raw);
    return;
  }
  int dmg = dart ? std::max(0, raw - ri(*this, 0, armor_bonus())) : raw;
  if (!dart && ring_has(IT_RING_ELEMENTS)) dmg /= 2;
  const int absorbed = std::min(shield, dmg);
  shield -= absorbed;
  hp -= dmg - absorbed;
  message(dart ? "陷阱射出了一支飞镖！" : "你踩中了陷阱！");
}

void Game::trigger_tile_features() {
  const int idx = feature_at(px, py);
  if (idx < 0) return;
  if (features[idx].type == F_PLANT) use_feature(idx);
  else trigger_trap(idx);
}

void Game::harm_shopkeeper() {
  if (shop_gone[floor]) return;
  if (!shop_warned[floor]) {
    shop_warned[floor] = 1;
    message("店主：别乱来！再这样我就走了！");
  } else {
    shop_gone[floor] = 1;
    shop_open = false;
    shop_n = 0;
    for (Mob& m : mobs)
      if (is_shopkeeper(m.type, floor)) m.alive = false;
    message("店主带着所有货物逃走了！");
  }
}

void Game::open_shop() {
  if (shop_gone[floor]) { message("商店已经空了。"); return; }

  shop_open = true;
  shop_sel = 0;
  shop_mode = 0;
  shop_page = 0;
  if (shop_floor == floor) return;
  shop_floor = floor;
  shop_n = 0;
  int tier = std::min(5, floor / 5 + 1);  // 6->2, 11->3, 16->4, 20->5
  int mult = floor / 5 + 1;
  auto add = [&](uint8_t id, int count, int level) {
    if (shop_n >= 20) return;
    Item it;
    it.id = id; it.count = (uint8_t)count; it.level = (uint8_t)level;
    it.charges = item_def(id).kind == K_WAND ? item_def(id).max_charge : 0;
    shop_items[shop_n] = it;
    shop_prices[shop_n] = item_value(id) * count * 5 * mult;
    shop_sold[shop_n] = false;
    shop_n++;
  };
  add(weapon_for_tier_rand(*this, tier), 1, 0);
  add(armor_for_tier(tier), 1, 0);
  add((uint8_t)ri(*this, IT_KNIFE, IT_JAVELIN), 3, 0);
  add(IT_POTION_HEAL, 1, 0);
  for (int i = 0; i < 2; i++) {
    static const uint8_t P[11] = {
        IT_POTION_HEAL, IT_POTION_STR, IT_POTION_SHIELD, IT_POTION_HASTE,
        IT_POTION_INVIS, IT_POTION_LEVITATE, IT_POTION_MIND, IT_POTION_XP,
        IT_POTION_MIGHT, IT_POTION_TOXIC, IT_POTION_FLAME};
    add(P[ri(*this, 0, 10)], 1, 0);
  }
  add(IT_SCROLL_MAP, 1, 0);
  add(IT_SCROLL_IDENTIFY, 1, 0);
  add(IT_SCROLL_CURSE, 1, 0);
  static const uint8_t S[8] = {IT_SCROLL_UP, IT_SCROLL_TELE, IT_SCROLL_TERROR,
                               IT_SCROLL_LULLABY, IT_SCROLL_RECHARGE,
                               IT_SCROLL_RAGE, IT_SCROLL_CHALLENGE,
                               IT_SCROLL_TRANSMUTE};
  add(S[ri(*this, 0, 7)], 1, 0);
  add(IT_FOOD_RATION, 2, 0);
  int roll = ri(*this, 0, 99);
  add(roll < 55 ? IT_BOMB : (roll < 80 ? IT_BOMB_FIRE : IT_BOMB_FROST), 1, 0);
  add(IT_ANKH, 1, 0);
  add(IT_STONE_AUG, 1, 0);
  add(IT_DART_TIPPED, 2, 0);
  int pots = 0, scrs = 0, wnds = 0;
  for (int i = 0; i < inv_n; i++) {
    uint8_t k = item_def(inv[i].id).kind;
    if (k == K_USE && inv[i].id >= IT_POTION_HEAL && inv[i].id <= IT_POTION_FLAME) pots += inv[i].count;
    else if (k == K_USE) scrs += inv[i].count;
    else if (k == K_WAND) wnds++;
  }
  if (pots >= scrs && pots >= wnds) add(IT_BAG_POTION, 1, 0);
  else if (scrs >= wnds) add(IT_BAG_SCROLL, 1, 0);
  else add(IT_BAG_WAND, 1, 0);
  if (ri(*this, 0, 99) < 30)
    add(ri(*this, 0, 1) ? random_wand(*this, tier)
                        : (uint8_t)ri(*this, IT_RING_MIGHT, IT_RING_ELEMENTS),
        1, 0);
  message("店主：看看有什么需要的吧。");
}
void Game::shop_buy(int i) {
  if (!shop_open || i < 0 || i >= shop_n || shop_sold[i]) return;
  if (gold < shop_prices[i]) { message("金币不够。"); return; }
  gold -= shop_prices[i];
  shop_sold[i] = true;
  add_item(shop_items[i].id, shop_items[i].count, shop_items[i].level);
  char b[96];
  snprintf(b, sizeof(b), "买下了 %s", item_name(shop_items[i].id));
  message(b);
}

int Game::shop_sell_price(uint8_t id, int count, int level) const {
  int mult = floor / 5 + 1;
  int price = (item_value(id) + level * 10) * count * mult / 2;
  return std::max(1, price);
}

void Game::shop_sell(int i) {
  if (i < 0 || i >= inv_n) return;
  if (item_def(inv[i].id).kind == K_QUEST) {
    message("这是任务物品，不能出售。");
    return;
  }
  int price = shop_sell_price(inv[i].id, inv[i].count, inv[i].level);
  Item sold = inv[i];
  remove_item(i, inv[i].count);
  gold += price;
  for (int k = 2; k > 0; k--) {
    buyback_items[k] = buyback_items[k - 1];
    buyback_prices[k] = buyback_prices[k - 1];
  }
  buyback_items[0] = sold;
  buyback_prices[0] = price;
  if (buyback_n < 3) buyback_n++;
  char b[96];
  snprintf(b, sizeof(b), "卖出了 %s，得到 %d 金", item_name(sold.id), price);
  message(b);
}

void Game::alchemy_brew(int recipe) {
  static const uint8_t NEED[3] = {IT_SEED_SUNGRASS, IT_SEED_FIREBLOOM,
                                  IT_SEED_BLINDWEED};
  static const uint8_t OUT[3] = {IT_POTION_HEAL, IT_POTION_FLAME,
                                 IT_POTION_INVIS};
  if (recipe < 0 || recipe > 2) return;
  int have = 0;
  for (int i = 0; i < inv_n; i++)
    if (inv[i].id == NEED[recipe]) have += inv[i].count;
  if (have < 3) { message("材料不足，需要 3 个同类种子。"); return; }
  int need = 3;
  for (int i = inv_n - 1; i >= 0 && need > 0; i--) {
    if (inv[i].id != NEED[recipe]) continue;
    int take = std::min((int)inv[i].count, need);
    remove_item(i, take);
    need -= take;
  }
  add_item(OUT[recipe], 1, 0);
  message("炼金成功！");
}

void Game::shop_buyback(int i) {
  if (i < 0 || i >= buyback_n) return;
  if (gold < buyback_prices[i]) { message("金币不够。"); return; }
  gold -= buyback_prices[i];
  add_item(buyback_items[i].id, buyback_items[i].count, buyback_items[i].level);
  for (int k = i; k < buyback_n - 1; k++) {
    buyback_items[k] = buyback_items[k + 1];
    buyback_prices[k] = buyback_prices[k + 1];
  }
  buyback_n--;
  message("你买回了物品。");
}

void Game::spawn_quest_miniboss() {
  uint8_t type = ghost_floor == 2 ? M_FETID_RAT
                 : ghost_floor == 3 ? M_TRICKSTER
                                    : M_GREAT_CRAB;
  for (int a = 0; a < 50; a++) {
    int x = px + ri(*this, -4, 4), y = py + ri(*this, -4, 4);
    if (x < 0 || y < 0 || x >= MW || y >= MH) continue;
    if (!walkable(x, y) || mob_at(x, y) >= 0) continue;
    Mob m;
    m.x = x; m.y = y; m.vx = (float)x; m.vy = (float)y;
    m.type = type;
    const MobDef& d = mob_def(type);
    m.hp = m.maxhp = d.hp + floor * 2;
    m.atk = d.atk + floor / 8;
    m.def = d.def;
    mobs.push_back(m);
    message("幽灵的仇敌出现了！");
    return;
  }
}

void Game::talk_npc(int idx) {
  Mob& m = mobs[idx];
  if (m.type == M_NPC_GHOST) {
    if (ghost_state == 0)
      open_dialog(1, "幽魂", "帮我复仇吧，地牢里有一个夺走我性命的怪物。\n它就在这附近。",
                  "接受复仇", "以后再说");
    else if (ghost_state == 1) {
      if (ghost_boss_dead)
        open_dialog(6, "幽魂", "你做到了……谢谢你。\n挑一件我生前的装备吧。",
                    "武器 (A)", "护甲 (B)");
      else
        open_dialog(1, "幽魂", "凶手还没有倒下，就在这层地牢里。", "继续寻找");
    } else
      open_dialog(1, "幽魂", "愿灯火指引你，旅行者。", "离开");
  } else if (m.type == M_NPC_WANDMAKER) {
    if (wand_state == 0)
      open_dialog(9, "制杖师", "我需要一份尸尘来制作法杖。\n它就在这层的某个路障后面。",
                  "接受任务", "以后再说");
    else if (wand_state == 1) {
      if (have_dust) {
        reward1 = random_wand(*this, 5);
        reward1_level = ri(*this, 1, 3);
        reward2 = random_wand(*this, 5);
        reward2_level = ri(*this, 1, 3);
        char o0[40], o1[40];
        snprintf(o0, sizeof(o0), "%s +%d", item_name(reward1), reward1_level);
        snprintf(o1, sizeof(o1), "%s +%d", item_name(reward2), reward2_level);
        open_dialog(7, "制杖师", "你把尸尘带来了！\n挑一根法杖吧。", o0, o1);
      } else
        open_dialog(9, "制杖师", "尸尘在路障后面，小心那些怨灵。", "继续寻找");
    } else
      open_dialog(9, "制杖师", "愿你的法杖永远充能。", "离开");
  } else if (m.type == M_NPC_SMITH) {
    if (smith_state == 0)
      open_dialog(10, "铁匠", "帮我挖 15 块暗金矿石，我就帮你强化装备。",
                  "接受任务", "以后再说");
    else if (smith_state == 1) {
      if (ore_count >= 15)
        open_dialog(8, "铁匠", "矿石够了！要强化哪一件？",
                    "强化武器 (A)", "强化护甲 (B)");
      else {
        char b[80];
        snprintf(b, sizeof(b), "还差 %d 块暗金矿石。", 15 - ore_count);
        open_dialog(10, "铁匠", b, "继续寻找");
      }
    } else
      open_dialog(10, "铁匠", "我已经帮你强化过了，走吧。", "离开");
  } else if (m.type == M_NPC_IMP) {
    if (floor == 20 && imp_state == 2) {
      open_shop();
    } else if (imp_state == 0)
      open_dialog(11, "小鬼", "帮我清理一下这层的僧侣/魔像，\n我就把传家宝指环送给你。",
                  "接受委托", "拒绝");
    else if (imp_state == 1) {
      if (imp_kills >= imp_target)
        open_dialog(12, "小鬼", "干得漂亮！这枚指环归你了。", "收下奖励");
      else {
        char b[96];
        snprintf(b, sizeof(b), "还差 %d 个目标。", imp_target - imp_kills);
        open_dialog(11, "小鬼", b, "继续");
      }
    } else
      open_dialog(11, "小鬼", "我的商店在 20 层，欢迎光临。", "离开");
  } else if (m.type == M_NPC_SHOP) {
    open_shop();
  }
}

void Game::pick_item(int idx) {
  FloorItem fi = items[idx];
  items.erase(items.begin() + idx);
  if (fi.id == IT_GOLD) {
    gain_gold(*this, fi.level);
    message("你拾取了一堆金币。");
    return;
  }
  if (fi.id == IT_CORPSE_DUST) {
    have_dust = 1;
    message("一股寒意顺着脊背爬上来。");
    return;
  }
  if (fi.id == IT_DARK_ORE) {
    ore_count++;
    char b[64];
    snprintf(b, sizeof(b), "暗金矿石 %d/15", ore_count);
    message(b);
    return;
  }
  if (fi.id == IT_AMULET) {
    add_item(IT_AMULET, 1);
    if (quest == 1) quest = 2;
    message("你找到了幽魂的护符！");
    return;
  }
  add_item(fi.id, 1, fi.level);
  char b[96];
  snprintf(b, sizeof(b), "拾取了 %s", item_name(fi.id));
  message(b);
}

void Game::close_door_if_clear(int x, int y) {
  if (x < 0 || y < 0 || x >= MW || y >= MH) return;
  if (tiles[y][x] != T_DOOR_OPEN) return;
  if (x == px && y == py) return;
  for (const Mob& m : mobs)
    if (m.alive && m.x == x && m.y == y) return;
  if (item_at(x, y) >= 0) return;  // SPD keeps doors open under heaps
  tiles[y][x] = T_DOOR;
}

void Game::move_player(int dx, int dy) {
  if ((!dx && !dy) || absi(dx) > 1 || absi(dy) > 1) return;
  int nx = px + dx, ny = py + dy;
  if (!in_map(nx, ny)) return;
  if (feature_blocking(nx, ny)) return;
  if (tiles[ny][nx] == T_DOOR) {
    tiles[ny][nx] = T_DOOR_OPEN;
    message("你推开了门。");
  } else if (tiles[ny][nx] == T_DOOR_LOCKED) {
    if (keys > 0) {
      keys--;
      tiles[ny][nx] = T_DOOR;
      message("你用铁钥匙解开了门锁。");
      recompute_fov();
      advance_turn();
      return;
    } else {
      message("门锁住了，需要一把钥匙。");
      return;
    }
  }
  if (!walkable(nx, ny)) return;
  int m = mob_at(nx, ny);
  if (m >= 0) {
    if (is_npc(mobs[m].type)) { talk_npc(m); return; }
    attack(m, false);
    advance_turn();
    return;
  }
  pvx = (float)px; pvy = (float)py;
  int ox = px, oy = py;
  px = nx;
  py = ny;
  close_door_if_clear(ox, oy);
  recompute_fov();
  int it = item_at(px, py);
  if (it >= 0) pick_item(it);
  trigger_tile_features();
  if (tiles[py][px] == T_STAIRS) {
    if (boss_alive()) {
      message("首领还活着，楼梯被封锁了。");
    } else if (floor >= 25) {
      open_dialog(0, "地牢出口", "你击败了尤格-泽瓦。\n要带着战利品离开吗？",
                  "离开地牢 (A)", "继续探索 (B)");
    } else {
      open_dialog(0, "向下的楼梯", "要进入更深的一层吗？", "下去 (A)", "留下 (B)");
    }
  }
  advance_turn();
}

void Game::combat_strike(CombatPose& pose, int x, int y, int tx, int ty,
                         CombatStyle style, uint8_t item) {
  pose.age = 0;
  pose.dx = (tx > x) - (tx < x);
  pose.dy = (ty > y) - (ty < y);
  pose.style = style;
  if (combat_fx.size() < 48)
    combat_fx.push_back({x, y, tx, ty, 0, 0, -1, style, item});
}

void Game::combat_hit(CombatPose& pose, int x, int y, int damage) {
  pose.hit_age = -combat_delay;
  if (combat_fx.size() < 48)
    combat_fx.push_back({x, y, x, y, 0, combat_delay, damage, CombatStyle::Melee});
}

void Game::damage_mob(int idx, int raw) {
  if (idx < 0 || idx >= (int)mobs.size()) return;
  Mob& m = mobs[idx];
  if (!m.alive) return;
  if (!is_npc(m.type) && los(m.x, m.y, px, py) && invis_t == 0) {
    m.ai = Mob::Hunting;
    m.target = py * MW + px;
  }
  int dmg = raw - m.def;
  if (dmg < 1) dmg = 1;
  m.hp -= dmg;
  combat_hit(m.combat, m.x, m.y, dmg);
  char b[96];
  snprintf(b, sizeof(b), "%s受到了 %d 点伤害", mob_def(m.type).name, dmg);
  message(b);
  if (m.hp > 0) return;
  m.alive = false;
  if (is_quest_miniboss(m.type)) ghost_boss_dead = 1;
  if (imp_state == 1 && (m.type == M_MONK || m.type == M_GOLEM)) {
    if (imp_kills < 99) imp_kills++;
  }
  xp += mob_def(m.type).xp;
  snprintf(b, sizeof(b), "%s被击败了！", mob_def(m.type).name);
  message(b);
  if (is_boss(m.type)) {
    boss_dead[region()] = 1;
    int tier = region() + 1;
    uint8_t drop = ((rnd(*this) & 1) ? weapon_for_tier(tier + 1)
                                     : armor_for_tier(tier + 1));
    items.push_back({m.x, m.y, drop, (int16_t)tier});
    gain_gold(*this, 150 + region() * 100);
    message("首领倒下了，掉落了强力装备。");
  } else if (m.type == M_MIMIC) {
    auto payload = std::find_if(features.begin(), features.end(), [&](const Feature& f) {
      return f.type == F_MIMIC_LOOT && f.aux == idx;
    });
    if (payload != features.end()) {
      if (payload->loot_id != IT_NONE)
        items.push_back({m.x, m.y, payload->loot_id, (int16_t)(payload->loot_id == IT_GOLD
                         ? payload->loot_gold : payload->loot_level)});
      features.erase(payload);
    } else {
      items.push_back({m.x, m.y, IT_GOLD,
                       (int16_t)ri(*this, 20 + floor * 5, 40 + floor * 8)});
    }
  } else {
    if ((rnd(*this) % 100) < 10) {
      items.push_back({m.x, m.y, IT_GOLD,
                       (int16_t)ri(*this, 10 + floor * 5, 20 + floor * 8)});
    }
    int meatChance = (m.type == M_RAT || m.type == M_SNAKE ||
                      m.type == M_CRAB || m.type == M_ALBINO) ? 18 : 7;
    if ((rnd(*this) % 100) < meatChance)
      items.push_back({m.x, m.y, IT_FOOD_MEAT, 0});
  }
  while (xp >= xpnext) {
    xp -= xpnext;
    level++;
    xpnext = level * 10;
    maxhp += 5;
    hp = std::min(maxhp, hp + 5);
    message("升级了！");
  }
}

void Game::attack(int idx, bool ranged, int bonus) {
  Mob& m = mobs[idx];
  if (!m.alive) return;
  if (is_shopkeeper(m.type, floor)) { harm_shopkeeper(); return; }
  if (is_npc(m.type)) { talk_npc(idx); return; }
  int raw = hero_atk() + weapon_bonus() + ring_atk() + talents[1] +
            ri(*this, 0, 2) + bonus;
  if (might_t > 0) raw += 3;
  if (ranged) raw += 1;
  combat_strike(hero_combat, px, py, m.x, m.y,
                ranged ? CombatStyle::Arcane : CombatStyle::Melee);
  combat_delay = ranged ? 170 : 70;
  damage_mob(idx, raw);
  combat_delay = 0;
}

void Game::interact_at(int x, int y) {
  if (!in_map(x, y)) return;
  if (tiles[y][x] == T_DOOR_LOCKED && grid_distance(x, y, px, py) == 1) {
    move_player(x - px, y - py);
    return;
  }
  int m = mob_at(x, y);
  if (m >= 0) {
    if (is_npc(mobs[m].type)) { talk_npc(m); return; }
    if (grid_distance(x, y, px, py) > 1) { message("距离太远，先靠近一点。"); return; }
    attack(m, false);
    advance_turn();
    return;
  }
  int fi = feature_at(x, y);
  if (fi >= 0) {
    if (grid_distance(x, y, px, py) > 1) { message("走近一点才能交互。"); return; }
    use_feature(fi);
    return;
  }
  int it = item_at(x, y);
  if (it >= 0) {
    if (absi(x - px) + absi(y - py) > 1) { message("走过去才能拾取。"); return; }
    pick_item(it);
    advance_turn();
    return;
  }
  if (tiles[y][x] == T_STAIRS) {
    if (boss_alive()) { message("首领还活着，楼梯被封锁了。"); return; }
    if (floor >= 25)
      open_dialog(0, "地牢出口", "你击败了尤格-泽瓦。\n要带着战利品离开吗？",
                  "离开地牢 (A)", "继续探索 (B)");
    else
      open_dialog(0, "向下的楼梯", "要进入更深的一层吗？", "下去 (A)", "留下 (B)");
    return;
  }
  for (int dy = -1; dy <= 1; dy++)
    for (int dx = -1; dx <= 1; dx++) {
      int nx = x + dx, ny = y + dy;
      if (nx < 0 || ny < 0 || nx >= MW || ny >= MH) continue;
      if (tiles[ny][nx] == T_SECRET) {
        tiles[ny][nx] = T_DOOR;
        seen[ny][nx] |= 1;
        message("你发现了一扇暗门！");
        recompute_fov();
        advance_turn();
        return;
      }
    }
  message("这里没有可以交互的东西。");
}

void Game::act_at(int x, int y, bool interact) {
  if (dialog || dead || shop_open) return;
  if (!in_map(x, y)) return;
  if (interact) { interact_at(x, y); return; }
  if (targeting) {
    if (target_item >= 0 && item_def(inv[target_item].id).kind == K_WAND)
      cast_wand(target_item, x, y);
    else
      cast_or_throw(x, y);
    return;
  }
  clear_path();
  if (x == px && y == py) {
    advance_turn();
    message("你等待了一回合。");
    return;
  }
  if (grid_distance(px, py, x, y) == 1) {
    move_player(x - px, y - py);
  } else {
    path_goal = y * MW + x;
  }
}

void Game::clear_path() { path_goal = -1; }

void Game::tick(int dt_ms) {
  if (dt_ms < 1) dt_ms = 1;
  if (dt_ms > 100) dt_ms = 100;
  const float sp = dt_ms * 0.014f;
  if (!freeze_anim) {
    water_anim_ms = (water_anim_ms + dt_ms) % 6400; // 32 pixels at 5 px/s
    auto advance_pose = [&](CombatPose& p) {
      if (p.age >= 0 && (p.age += dt_ms) >= 420) p.age = -1;
      if (p.hit_age < 200) p.hit_age += dt_ms;
    };
    advance_pose(hero_combat);
    for (Mob& m : mobs) advance_pose(m.combat);
    for (CombatFx& f : combat_fx) f.age += dt_ms;
    combat_fx.erase(std::remove_if(combat_fx.begin(), combat_fx.end(),
        [](const CombatFx& f) { return f.age >= f.delay + 640; }), combat_fx.end());
    if (pvx < px) pvx = std::min((float)px, pvx + sp);
    else if (pvx > px) pvx = std::max((float)px, pvx - sp);
    if (pvy < py) pvy = std::min((float)py, pvy + sp);
    else if (pvy > py) pvy = std::max((float)py, pvy - sp);
  }
  panim_ms += dt_ms;
  if (!freeze_anim) {
    if (search_ms > 0) search_ms -= dt_ms;
    for (size_t i = 0; i < search_fx.size();) {
      SearchFx& f = search_fx[i];
      if (f.delay > 0) f.delay -= dt_ms / 1000.0f;
      else f.alpha -= dt_ms / 1000.0f;
      if (f.delay <= 0 && f.alpha <= 0) search_fx.erase(search_fx.begin() + i);
      else i++;
    }
  }
  for (Mob& m : mobs) {
    if (!freeze_anim) {
      if (m.vx < m.x) m.vx = std::min((float)m.x, m.vx + sp);
      else if (m.vx > m.x) m.vx = std::max((float)m.x, m.vx - sp);
      if (m.vy < m.y) m.vy = std::min((float)m.y, m.vy + sp);
      else if (m.vy > m.y) m.vy = std::max((float)m.y, m.vy - sp);
    }
    m.anim_ms += dt_ms;
  }
  if (!dialog && !dead && !paused && !backpack && !shop_open &&
      path_goal >= 0 && fabsf(pvx - px) < 0.02f && fabsf(pvy - py) < 0.02f) {
    navigation.load_actors(*this);
    const int goal = path_goal;
    const int next = navigation.toward(py * MW + px, goal, [&](int p) {
      const int x = p % MW, y = p / MW;
      return (seen[y][x] || vis[y][x]) &&
             (!navigation.occupied[y][x] || (p == goal && vis[y][x])) &&
             (walkable(x, y) || (p == goal && keys > 0 &&
               tiles[y][x] == T_DOOR_LOCKED && !feature_blocking(x, y)));
    });
    if (next < 0) { clear_path(); message("无法到达那里。"); return; }
    if (next == goal) clear_path();
    int nx = next % MW, ny = next / MW;
    int ox = px, oy = py;
    move_player(nx - px, ny - py);
    if ((px == ox && py == oy) || dialog || dead) clear_path();
  }
}

void Game::mob_turn(Mob& m) {
  if (!m.alive || is_npc(m.type)) return;
  if (m.sleep_t > 0) { m.sleep_t--; return; }
  const int start = m.y * MW + m.x, hero = py * MW + px;
  const int dist = grid_distance(m.x, m.y, px, py);
  navigation.load_actors(*this);
  auto passable = [&](int p) {
    return !navigation.occupied[p / MW][p % MW] && walkable(p % MW, p / MW);
  };
  auto step_to = [&](int step) {
    if (step < 0 || !passable(step)) return false;
    int ox = m.x, oy = m.y;
    m.vx = ox; m.vy = oy;
    m.x = step % MW; m.y = step / MW;
    if (tiles[m.y][m.x] == T_DOOR) tiles[m.y][m.x] = T_DOOR_OPEN;
    close_door_if_clear(ox, oy);
    const int trap = feature_at(m.x, m.y);
    if (trap >= 0) trigger_trap(trap, &m);
    return true;
  };
  if (m.terror_t > 0) {
    m.terror_t--;
    // Dungeon.flee first ignores actors, then excludes any occupied step
    // and retries. Terror uses four cells and cannot approach the source.
    bool excluded[MW * MH]{};
    for (;;) {
      int next = navigation.retreat(start, hero, 4, false, [&](int p) {
        return p == start || (!excluded[p] && walkable(p % MW, p / MW));
      });
      if (next < 0 || next == start || step_to(next)) break;
      excluded[next] = true;
    }
    return;
  }
  if (rage_t > 0) {
    for (size_t i = 0; i < mobs.size(); i++) {
      Mob& o = mobs[i];
      if (!o.alive || &o == &m || is_npc(o.type)) continue;
      if (absi(o.x - m.x) <= 1 && absi(o.y - m.y) <= 1) {
        combat_strike(m.combat, m.x, m.y, o.x, o.y, CombatStyle::Melee);
        combat_delay = 70;
        damage_mob((int)i, m.atk + ri(*this, 0, 2));
        combat_delay = 0;
        return;
      }
    }
  }
  const bool sees = dist <= fov_radius && invis_t == 0 && los(m.x, m.y, px, py);
  if (m.ai != Mob::Hunting && sees &&
      ri(*this, 0, (m.ai == Mob::Sleeping ? 2 * dist : dist) + 1) < 2) {
    m.ai = Mob::Hunting;
    m.target = hero;
    return;
  }
  if (m.ai == Mob::Sleeping) return;
  if (m.ai == Mob::Hunting && sees) m.target = hero;
  if (m.ai == Mob::Hunting && sees && dist == 1) {
    int dmg = std::max(1, m.atk + ri(*this, 0, 1) - hero_def() - armor_bonus() -
                            ring_def() - talents[2]);
    if (shield > 0) {
      int a = std::min(shield, dmg);
      shield -= a; dmg -= a;
    }
    if (dmg > 0) hp -= dmg;
    combat_strike(m.combat, m.x, m.y, px, py, CombatStyle::Melee);
    combat_delay = 70;
    combat_hit(hero_combat, px, py, dmg);
    combat_delay = 0;
    char b[96];
    snprintf(b, sizeof(b), "%s攻击了你，造成 %d 点伤害", mob_def(m.type).name, dmg);
    message(b);
    return;
  }
  if (m.target >= 0) {
    const int goal = m.target;
    const int step = navigation.toward(start, goal, [&](int p) {
      // An occupied goal can guide the route, but step_to never enters it.
      return passable(p) || (p == goal && walkable(p % MW, p / MW));
    });
    if (step_to(step)) return;
    // Keep a visible target when another unit is temporarily in the way.
    if (m.ai == Mob::Hunting && sees) return;
  }
  m.ai = Mob::Wandering;
  m.target = -1;
  for (int tries = 0; tries < 16; ++tries) {
    const int x = ri(*this, 1, map_width - 2), y = ri(*this, 1, map_height - 2);
    const int p = y * MW + x;
    if (passable(p)) { m.target = p; break; }
  }
}

void Game::recharge_wands() {
  int period = ring_has(IT_RING_ENERGY) ? 10 : 15;
  if (turns % period != 0) return;
  for (int i = 0; i < inv_n; i++)
    if (item_def(inv[i].id).kind == K_WAND) {
      uint8_t maxc = item_def(inv[i].id).max_charge;
      if (inv[i].charges < maxc) inv[i].charges++;
    }
}

void Game::search() {
  clear_path();
  const int R = 2;
  bool found = false;
  search_fx.clear();
  for (int y = std::max(0, py - R); y <= std::min(MH - 1, py + R); y++) {
    for (int x = std::max(0, px - R); x <= std::min(MW - 1, px + R); x++) {
      int dist = std::max(absi(x - px), absi(y - py));
      if (dist == 0) continue;
      if (!vis[y][x]) continue;
      float delay = dist > 1 ? powf((float)(dist - 1), 0.67f) / 10.0f : 0.0f;
      search_fx.push_back({(float)x, (float)y, delay, 0.8f});
      if (tiles[y][x] == T_SECRET) {
        tiles[y][x] = T_DOOR;
        seen[y][x] |= 1;
        found = true;
      }
    }
  }
  for (Feature& f : features) {
    if (f.type == F_TRAP &&
        vis[f.y][f.x] && grid_distance(f.x, f.y, px, py) <= R) {
      f.type = F_TRAP_FOUND;
      found = true;
    }
  }
  search_ms = 500;
  message(found ? "你发现了隐藏的东西！" : "你仔细搜索了周围，什么也没找到。");
  advance_turn();
}

bool Game::drop_item(int idx) {
  if (idx < 0 || idx >= inv_n) return false;
  if (inv[idx].id == IT_CORPSE_DUST || inv[idx].id == IT_DARK_ORE ||
      inv[idx].id == IT_DWARF_TOKEN) {
    message("任务物品不能丢弃。");
    return false;
  }
  if (item_at(px, py) >= 0) { message("这里已经有东西了。"); return false; }
  items.push_back({px, py, inv[idx].id, inv[idx].level});
  char b[96];
  snprintf(b, sizeof(b), "你丢下了 %s", item_name(inv[idx].id));
  message(b);
  remove_item(idx, 1);
  return true;
}

static uint8_t transmute_result(Game& g, uint8_t id) {
  static const uint8_t WEP[5][3] = {
      {IT_DAGGER, IT_SHORTSWORD, IT_CUDGEL},
      {IT_LONGSWORD, IT_HAND_AXE, IT_SPEAR},
      {IT_WARHAMMER, IT_SCIMITAR, IT_WHIP},
      {IT_GREATAXE, IT_GREATSWORD, IT_HALBERD},
      {IT_RUNEBLADE, IT_BATTLEAXE, IT_GIANT_HAMMER}};
  static const uint8_t ARM[6] = {IT_CLOTH_ARMOR, IT_LEATHER_ARMOR, IT_MAIL,
                                 IT_SCALE, IT_PLATE, IT_RUNE_ARMOR};
  static const uint8_t WANDS[13] = {
      IT_WAND_MAGIC, IT_WAND_FIRE, IT_WAND_FROST, IT_WAND_LIGHTNING,
      IT_WAND_DISINTEGRATION, IT_WAND_CORROSION, IT_WAND_EARTH,
      IT_WAND_BLAST, IT_WAND_CORRUPTION, IT_WAND_WARDING,
      IT_WAND_REGROWTH, IT_WAND_TRANSFUSION, IT_WAND_PRISMATIC};
  static const uint8_t RINGS[8] = {
      IT_RING_MIGHT, IT_RING_ACCURACY, IT_RING_EVASION, IT_RING_HASTE,
      IT_RING_WEALTH, IT_RING_ENERGY, IT_RING_FORCE, IT_RING_ELEMENTS};
  static const uint8_t POTS[11] = {
      IT_POTION_HEAL, IT_POTION_STR, IT_POTION_SHIELD, IT_POTION_HASTE,
      IT_POTION_INVIS, IT_POTION_LEVITATE, IT_POTION_MIND, IT_POTION_XP,
      IT_POTION_MIGHT, IT_POTION_TOXIC, IT_POTION_FLAME};
  static const uint8_t SCRS[10] = {
      IT_SCROLL_MAP, IT_SCROLL_UP, IT_SCROLL_TELE, IT_SCROLL_IDENTIFY,
      IT_SCROLL_CURSE, IT_SCROLL_TERROR, IT_SCROLL_LULLABY,
      IT_SCROLL_RECHARGE, IT_SCROLL_RAGE, IT_SCROLL_CHALLENGE};
  static const uint8_t SEEDS[3] = {IT_SEED_SUNGRASS, IT_SEED_FIREBLOOM,
                                   IT_SEED_BLINDWEED};
  static const uint8_t MISS[4] = {IT_KNIFE, IT_DART, IT_JAVELIN, IT_DART_TIPPED};
  static const uint8_t BOMBS[3] = {IT_BOMB, IT_BOMB_FIRE, IT_BOMB_FROST};
  uint8_t kind = item_def(id).kind;
  if (kind == K_WEAPON) {
    int tier = (id == IT_RUNEBLADE || id == IT_BATTLEAXE ||
                id == IT_GIANT_HAMMER) ? 5
             : (id == IT_GREATAXE || id == IT_GREATSWORD ||
                id == IT_HALBERD) ? 4
             : (id == IT_WARHAMMER || id == IT_SCIMITAR || id == IT_WHIP) ? 3
             : (id == IT_LONGSWORD || id == IT_HAND_AXE ||
                id == IT_SPEAR) ? 2 : 1;
    if (id == IT_PICKAXE) tier = 1;
    return WEP[tier - 1][ri(g, 0, 2)];
  }
  if (kind == K_ARMOR) {
    int idx = 0;
    for (int i = 0; i < 6; i++) if (ARM[i] == id) idx = i;
    int pick = ri(g, 0, 5);
    if (pick == idx) pick = (pick + 1) % 6;
    return ARM[pick];
  }
  if (kind == K_WAND) return WANDS[ri(g, 0, 12)];
  if (kind == K_RING) return RINGS[ri(g, 0, 7)];
  if (kind == K_USE) {
    if (id >= IT_SCROLL_MAP && id <= IT_SCROLL_TRANSMUTE) {
      int idx = ri(g, 0, 9);
      return SCRS[idx];
    }
    return POTS[ri(g, 0, 10)];
  }
  if (kind == K_SEED) return SEEDS[ri(g, 0, 2)];
  if (kind == K_MISSILE) return MISS[ri(g, 0, 3)];
  if (kind == K_BOMB) return BOMBS[ri(g, 0, 2)];
  return id;
}

bool Game::can_transmute(int idx) const {
  if (idx < 0 || idx >= inv_n) return false;
  uint8_t id = inv[idx].id;
  if (id == IT_NONE || item_def(id).kind == K_KEY || id == IT_GOLD || id == IT_AMULET ||
      id == IT_PICKAXE || id == IT_ANKH || id == IT_SCROLL_TRANSMUTE ||
      item_def(id).kind == K_QUEST || item_def(id).kind == K_BAG)
    return false;
  uint8_t k = item_def(id).kind;
  return k == K_WEAPON || k == K_ARMOR || k == K_WAND || k == K_RING ||
         k == K_USE || k == K_SEED || k == K_MISSILE || k == K_BOMB;
}

void Game::transmute_choose(int idx) {
  if (idx < 0 || idx >= inv_n || !can_transmute(idx)) return;
  uint8_t old = inv[idx].id;
  uint8_t nw = old;
  for (int tries = 0; tries < 10 && nw == old; tries++)
    nw = transmute_result(*this, old);
  Item& item = inv[idx];
  item.id = nw;
  item.charges = item_def(nw).kind == K_WAND ? item_def(nw).max_charge : 0;
  // consume one transmutation scroll
  for (int i = 0; i < inv_n; i++)
    if (inv[i].id == IT_SCROLL_TRANSMUTE) { remove_item(i, 1); break; }
  transmute_open = false;
  char b[96];
  snprintf(b, sizeof(b), "%s 变成了 %s", item_name(old), item_name(nw));
  message(b);
  advance_turn();
}

void Game::check_talents() {
  static const uint8_t NEED[4] = {3, 6, 9, 12};
  static const char* NAMES[4] = {"坚韧", "强击", "护甲", "精通"};
  while (talent_tier < 4 && level >= NEED[talent_tier]) {
    talents[talent_tier] = 1;
    if (talent_tier == 0) { maxhp += 4; hp += 4; }
    else if (talent_tier == 1) { /* +1 attack handled below */ }
    else if (talent_tier == 2) { /* +1 defense handled below */ }
    else { maxhp += 4; hp += 4; }
    char b[64];
    snprintf(b, sizeof(b), "获得天赋：%s", NAMES[talent_tier]);
    message(b);
    talent_tier++;
  }
}

void Game::advance_turn() {
  if (dead) return;
  turns++;
  if (turns % 12 == 0 && hp < maxhp) hp++;
  if (haste_t > 0) haste_t--;
  if (invis_t > 0) invis_t--;
  if (might_t > 0) might_t--;
  if (levit_t > 0) levit_t--;
  if (mind_t > 0) mind_t--;
  if (rage_t > 0) rage_t--;
  if (ring_has(IT_RING_FORCE) && turns % 10 == 0) shield += 3;
  if (have_dust && wand_state == 1) {
    dust_timer++;
    int wraiths = 1;
    for (const Mob& m : mobs)
      if (m.alive && m.type == M_WRAITH) wraiths++;
    const int power = std::min(49, wraiths * wraiths);
    if (dust_timer >= power) {
      std::vector<int> cells;
      const int min_dist = (fov_radius + 1) / 3;
      for (int y = std::max(1, py - fov_radius); y <= std::min(MH - 2, py + fov_radius); ++y)
        for (int x = std::max(1, px - fov_radius); x <= std::min(MW - 2, px + fov_radius); ++x)
          if (vis[y][x] && grid_distance(x, y, px, py) > min_dist &&
              walkable(x, y) && tiles[y][x] != T_DOOR && mob_at(x, y) < 0)
            cells.push_back(y * MW + x);
      if (!cells.empty()) {
        const int p = cells[ri(*this, 0, int(cells.size()) - 1)];
        mobs.push_back(make_floor_mob(M_WRAITH, p % MW, p / MW, Mob::Hunting));
        dust_timer -= power;
        message("尸尘中冒出了一只怨灵！");
      } else {
        dust_timer = std::min(dust_timer, 2 * wraiths);
      }
    }
  }
  if (mob_cap > 0 && (respawn_t -= 3) <= 0) {
    int half_weight = 0;
    for (const Mob& m : mobs) {
      if (!m.alive || is_npc(m.type) || is_boss(m.type) || m.type == M_MIMIC ||
          m.type == M_WRAITH || m.type == M_RIPPER || m.type == M_FETID_RAT ||
          m.type == M_TRICKSTER || m.type == M_GREAT_CRAB) continue;
      half_weight += m.type == M_GHOUL ? 1 : 2;
    }
    const int cap = 3 + floor % 5 + ri(*this, 0, 2);
    const int limit = plan_feeling[floor - 1] == 3 ? (cap * 133 + 99) / 100 : cap;
    if ((half_weight + 1) / 2 < limit && !try_respawn()) respawn_t += 3;
    else respawn_t += plan_feeling[floor - 1] == 6 ? 100 : 150;
  }
  recharge_wands();
  bool skip = (haste_t > 0 || ring_has(IT_RING_HASTE)) && (turns % 2 == 0);
  if (!skip) {
    for (Mob& m : mobs) {
      if (dead) break;
      mob_turn(m);
    }
  }
  recompute_fov();
  if (hp <= 0) {
    int ankh = -1;
    for (int i = 0; i < inv_n; i++)
      if (inv[i].id == IT_ANKH) { ankh = i; break; }
    if (ankh >= 0) {
      remove_item(ankh, 1);
      hp = std::max(1, maxhp / 2);
      shield = 0;
      message("安卡碎裂，你从死亡边缘被拉了回来！");
      return;
    }
    hp = 0;
    dead = true;
    open_dialog(2, "你死了", "你的冒险到此为止。\n要回到主菜单吗？", "结束 (A)");
  }
}

void Game::wait_turns(int n) {
  for (int i = 0; i < n && !dead && !dialog; i++) {
    int hp0 = hp;
    advance_turn();
    if (hp != hp0) break;
  }
}

void Game::descend() {
  if (boss_alive()) {
    message("首领还活着，楼梯被封锁了。");
    return;
  }
  if (floor >= 25) {
    won = true;
    open_dialog(3, "试玩完成", "你击败了尤格-泽瓦，走出了地牢！\n感谢试玩这个 3DS 移植原型。",
                "带着战利品离开 (A)");
    return;
  }
  floor++;
  keys = golden_keys = crystal_keys = 0;
  legacy_keys = false;
  gen_level();
  pvx = (float)px; pvy = (float)py;
  clear_path();
  hp = std::min(maxhp, hp + 3);
  char b[64];
  snprintf(b, sizeof(b), "你来到了第 %d 层。", floor);
  message(b);
}

void Game::open_dialog(int type, const char* name, const char* text, const char* o0,
                       const char* o1, const char* o2, const char* o3) {
  dialog = true;
  dlg_type = type;
  snprintf(dlg_name, sizeof(dlg_name), "%s", name);
  snprintf(dlg_text, sizeof(dlg_text), "%s", text);
  dlg_optn = 0;
  const char* opts[4] = {o0, o1, o2, o3};
  for (int i = 0; i < 4; i++) {
    if (!opts[i]) break;
    snprintf(dlg_opt[i], sizeof(dlg_opt[i]), "%s", opts[i]);
    dlg_optn++;
  }
  targeting = false;
}

void Game::dialog_choose(int i) {
  if (!dialog || i < 0 || i >= dlg_optn) return;
  if (dlg_type == 0) {
    if (i == 0) { dialog = false; descend(); }
    else dialog = false;
  } else if (dlg_type == 1) {
    dialog = false;
    if (ghost_state == 0 && i == 0) {
      ghost_state = 1;
      spawn_quest_miniboss();
    }
  } else if (dlg_type == 6) {  // ghost reward
    dialog = false;
    ghost_state = 2;
    uint8_t tier = ghost_tier(*this);
    uint8_t lvl = ghost_level(*this);
    if (i == 0) add_item(weapon_for_tier_rand(*this, tier), 1, lvl);
    else add_item(armor_for_tier(tier), 1, lvl);
    for (Mob& m : mobs)
      if (m.type == M_NPC_GHOST) m.alive = false;
    message("幽魂消失了，留下了一件遗物。");
  } else if (dlg_type == 7) {  // wandmaker reward
    dialog = false;
    wand_state = 2;
    have_dust = 0;
    dust_timer = 0;
    for (Mob& m : mobs)
      if (m.type == M_WRAITH) m.alive = false;
    if (i == 0) add_item(reward1, 1, reward1_level);
    else add_item(reward2, 1, reward2_level);
    for (Mob& m : mobs)
      if (m.type == M_NPC_WANDMAKER) m.alive = false;
    message("制杖师消失了。");
  } else if (dlg_type == 8) {  // smith reward
    dialog = false;
    smith_state = 2;
    for (int k = 0; k < inv_n; k++)
      if (inv[k].id == IT_PICKAXE) { remove_item(k, 1); break; }
    if (i == 0) weapon_level++;
    else armor_level++;
    for (Mob& m : mobs)
      if (m.type == M_NPC_SMITH) m.alive = false;
    message("铁匠完成了强化。");
  } else if (dlg_type == 9) {  // wand quest accept
    dialog = false;
    if (i == 0 && wand_state == 0) {
      wand_state = 1;
      for (int t = 0; t < 200; t++) {
        int x = ri(*this, 1, MW - 2), y = ri(*this, 1, MH - 2);
        if (tiles[y][x] != T_FLOOR || feature_at(x, y) >= 0) continue;
        if (x == px && y == py) continue;
        items.push_back({x, y, IT_CORPSE_DUST, 0});
        for (int k = 0; k < 2; k++) {
          int bx = x + (k ? 1 : -1), by = y;
          if (bx < 1 || bx >= MW - 1 || tiles[by][bx] != T_FLOOR) continue;
          Feature f;
          f.x = bx; f.y = by; f.type = F_BARRICADE; f.aux = 2;
          features.push_back(f);
        }
        break;
      }
      message("制杖师：尸尘就在路障后面。");
    }
  } else if (dlg_type == 10) {  // smith quest accept
    dialog = false;
    if (i == 0 && smith_state == 0) {
      smith_state = 1;
      add_item(IT_PICKAXE, 1, 0);
      for (int n = 0; n < 4; n++) {
        for (int a = 0; a < 60; a++) {
          int x = ri(*this, 1, MW - 2), y = ri(*this, 1, MH - 2);
          if (tiles[y][x] != T_FLOOR || feature_at(x, y) >= 0) continue;
          Feature f;
          f.x = x; f.y = y; f.type = F_BARRICADE; f.aux = 2;
          features.push_back(f);
          break;
        }
      }
      for (int n = 0; n < 15; n++) {
        for (int t = 0; t < 100; t++) {
          int x = ri(*this, 1, MW - 2), y = ri(*this, 1, MH - 2);
          if (tiles[y][x] != T_FLOOR || item_at(x, y) >= 0 ||
              feature_at(x, y) >= 0 || (x == px && y == py))
            continue;
          items.push_back({x, y, IT_DARK_ORE, 0});
          break;
        }
      }
      message("铁匠：去挖 15 块暗金矿石吧。");
    }
  } else if (dlg_type == 11) {  // imp quest accept
    dialog = false;
    if (i == 0 && imp_state == 0) {
      imp_state = 1;
      imp_kills = 0;
      imp_target = (imp_floor == 17) ? 5 : 4;
      message(imp_floor == 17 ? "小鬼：去杀 5 个武僧。"
                              : "小鬼：去杀 4 个石魔像。");
    }
  } else if (dlg_type == 12) {  // imp reward
    dialog = false;
    imp_state = 2;
    uint8_t ring = (uint8_t)ri(*this, IT_RING_MIGHT, IT_RING_ELEMENTS);
    add_item(ring, 1, 2);
    for (Mob& m : mobs)
      if (m.type == M_NPC_IMP && floor != 20) m.alive = false;
    message("小鬼：20 层的商店归我管了！");
  } else {
    dialog = false;
  }
}

void Game::cast_or_throw(int x, int y) {
  if (target_item < 0 || target_item >= inv_n) { targeting = false; return; }
  Item& it = inv[target_item];
  uint8_t kind = item_def(it.id).kind;
  if (kind != K_BOMB && kind != K_MISSILE) { targeting = false; return; }
  if (absi(x - px) + absi(y - py) > 6) { message("太远了，够不到。"); return; }
  const CombatStyle style = kind == K_MISSILE ? CombatStyle::Throw :
      it.id == IT_BOMB_FIRE ? CombatStyle::Fire :
      it.id == IT_BOMB_FROST ? CombatStyle::Frost : CombatStyle::Blast;
  combat_strike(hero_combat, px, py, x, y, style, it.id);
  combat_delay = 170;

  if (kind == K_MISSILE) {
    int mi = -1;
    for (size_t i = 0; i < mobs.size(); i++) {
      Mob& m = mobs[i];
      if (!m.alive || is_npc(m.type)) continue;
      if (absi(m.x - x) > 1 || absi(m.y - y) > 1) continue;
      mi = (int)i;
      break;
    }
    if (mi < 0) { message("投掷物没有命中。"); }
    else {
      int dmg = item_def(it.id).power + it.level * 2 + ri(*this, 0, 2);
      if (it.id == IT_DART_TIPPED) {
        dmg += 3;
        if (mobs[mi].alive) mobs[mi].sleep_t = 8;
      }
      damage_mob(mi, dmg);
      message("投掷物命中了目标。");
    }
  } else {
    int base = item_def(it.id).power;
    if (it.id == IT_BOMB_FIRE) base += 3;
    if (it.id == IT_BOMB_FROST) base += 1;
    for (size_t i = 0; i < mobs.size(); i++) {
      Mob& m = mobs[i];
      if (!m.alive || is_npc(m.type)) continue;
      if (absi(m.x - x) <= 1 && absi(m.y - y) <= 1) {
        damage_mob((int)i, base + floor / 4);
        if (it.id == IT_BOMB_FROST && m.alive) m.sleep_t = 10;
      }
    }
    message("炸弹爆炸了！");
  }
  it.count--;
  if (it.count <= 0) remove_item(target_item, 1);
  targeting = false;
  target_item = -1;
  combat_delay = 0;
  advance_turn();
}

static const uint32_t SAVE_MAGIC = 0x33445053u;  // "SPD3"
static const uint16_t SAVE_VERSION = 10, LEGACY_SAVE_VERSION = 9;
// Optional trailer: the original V9 header and slot metadata stay readable.
struct SpawnDisk {
  uint32_t magic = 0x31575053; // SPW1
  uint16_t remaining = 0;
  uint8_t room_count = 0, deck_count = 0;
  Game::SpawnRoom rooms[25]{};
  uint8_t deck[16]{};
};
struct MapDisk {
  uint32_t magic = 0x3150414D; // MAP1
  uint8_t width = 0, height = 0;
  uint16_t golden_keys = 0;
  uint8_t tiles[MH][MW]{};
  uint8_t seen[MH][MW]{};
  uint8_t legacy_keys = 0, loot_second_deck = 0;
  uint8_t loot_remaining[10]{};
  uint16_t crystal_keys = 0;
};

static void slot_path(int slot, char* out, size_t n) {
  snprintf(out, n, SPD_DATA_DIR "/slot%d.bin", slot);
}

SlotMeta Game::meta() const {
  SlotMeta m;
  m.used = true;
  m.hero = (uint8_t)hero;
  m.depth = (uint8_t)floor;
  m.level = (uint8_t)level;
  m.hp = (int16_t)hp;
  m.maxhp = (int16_t)maxhp;
  m.score = floor * 100 + gold + level * 20 + turns / 10 + (boss_dead[region()] ? 200 : 0);
  m.seed = seed;
  m.turns = (uint32_t)turns;
  m.gold = gold;
  m.stamp = (uint32_t)osGetTime() / 1000;
  return m;
}

bool Game::save(int slot) const {
  GameDisk d{};
  d.magic = SAVE_MAGIC;
  d.version = SAVE_VERSION;
  d.bytes = (uint16_t)sizeof(GameDisk);
  d.meta = meta();
  d.seed = seed; d.rng = rng;
  d.floor = (int16_t)floor; d.hero = (int16_t)hero;
  d.px = (int16_t)px; d.py = (int16_t)py;
  d.hp = (int16_t)hp; d.maxhp = (int16_t)maxhp;
  d.level = (int16_t)level; d.xp = (int16_t)xp; d.xpnext = (int16_t)xpnext;
  d.keys = (int16_t)keys; d.gold = (int16_t)gold;
  d.turns = (int16_t)std::min(turns, 32000);
  d.shield = (int16_t)shield;
  d.weapon = weapon; d.weapon_level = weapon_level;
  d.armor = armor; d.armor_level = armor_level;
  d.ring1 = ring1; d.ring1_level = ring1_level;
  d.ring2 = ring2; d.ring2_level = ring2_level;
  d.haste_t = haste_t; d.invis_t = invis_t; d.might_t = might_t;
  d.levit_t = levit_t; d.mind_t = mind_t; d.rage_t = rage_t;
  d.quest = quest;
  for (int y = 0; y < LEGACY_MAP_SIZE; ++y) {
    memcpy(d.tiles[y], tiles[y], sizeof(d.tiles[y]));
    memcpy(d.seen[y], seen[y], sizeof(d.seen[y]));
  }
  d.mob_n = (uint16_t)std::min((int)mobs.size(), 48);
  for (int i = 0; i < d.mob_n; i++)
    d.mobs[i] = {(int16_t)mobs[i].x, (int16_t)mobs[i].y, (int16_t)mobs[i].hp,
                 (int16_t)mobs[i].maxhp, (int16_t)mobs[i].atk,
                 (int16_t)mobs[i].def, mobs[i].type, (uint8_t)mobs[i].alive,
                 mobs[i].sleep_t, mobs[i].terror_t};
  d.item_n = (uint16_t)std::min((int)items.size(), 48);
  for (int i = 0; i < d.item_n; i++)
    d.items[i] = {(int16_t)items[i].x, (int16_t)items[i].y,
                  (int16_t)items[i].level, items[i].id};
  d.inv_n = (uint8_t)std::min(inv_n, BAG_MAX);
  for (int i = 0; i < d.inv_n; i++)
    d.inv[i] = {inv[i].id, inv[i].count, inv[i].level, inv[i].charges};
  d.feature_n = (uint8_t)std::min((int)features.size(), 64);
  for (int i = 0; i < d.feature_n; i++)
    d.features[i] = {(int16_t)features[i].x, (int16_t)features[i].y,
                     features[i].aux, features[i].loot_gold, features[i].type,
                     features[i].state, features[i].loot_id,
                     features[i].loot_level};
  memcpy(d.boss_dead, boss_dead, sizeof(boss_dead));
  d.tokens = tokens; d.token_quest = token_quest;
  d.smith_used = smith_used; d.imp_state = imp_state;
  d.ghost_floor = ghost_floor; d.ghost_state = ghost_state;
  d.ghost_boss_dead = ghost_boss_dead;
  d.wand_floor = wand_floor; d.wand_state = wand_state; d.have_dust = have_dust;
  d.smith_floor = smith_floor; d.smith_state = smith_state; d.ore_count = ore_count;
  d.imp_floor = imp_floor; d.imp_kills = imp_kills; d.imp_target = imp_target;
  d.reward1 = reward1; d.reward1_level = reward1_level;
  d.reward2 = reward2; d.reward2_level = reward2_level;
  d.bag_potion = bag_potion ? 1 : 0;
  d.bag_scroll = bag_scroll ? 1 : 0;
  d.bag_wand = bag_wand ? 1 : 0;
  memcpy(d.shop_warned, shop_warned, sizeof(shop_warned));
  memcpy(d.shop_gone, shop_gone, sizeof(shop_gone));
  d.dust_timer = (uint8_t)std::min(dust_timer, 255);
  memcpy(d.talents, talents, sizeof(talents));
  d.talent_tier = talent_tier;
  d.buyback_n = (uint8_t)buyback_n;
  for (int i = 0; i < buyback_n && i < 3; i++) {
    d.buyback_items[i] = {buyback_items[i].id, buyback_items[i].count,
                          buyback_items[i].level, buyback_items[i].charges};
    d.buyback_prices[i] = (int16_t)buyback_prices[i];
  }
  d.shop_n = (uint8_t)std::min(shop_n, 20);
  d.shop_floor = (uint8_t)std::max(0, std::min(shop_floor, 25));
  for (int i = 0; i < d.shop_n; i++) {
    d.shop_items[i] = {shop_items[i].id, shop_items[i].count,
                       shop_items[i].level, shop_items[i].charges};
    d.shop_prices[i] = (int16_t)shop_prices[i];
    d.shop_sold[i] = shop_sold[i] ? 1 : 0;
  }
  char path[64];
  slot_path(slot, path, sizeof(path));
  FILE* f = fopen(path, "wb");
  if (!f) return false;
  SpawnDisk sp;
  sp.remaining = uint16_t(std::clamp(respawn_t, 0, 150));
  sp.room_count = uint8_t(std::min<size_t>(25, spawn_rooms.size()));
  sp.deck_count = uint8_t(std::min<size_t>(16, spawn_deck.size()));
  std::copy_n(spawn_rooms.begin(), sp.room_count, sp.rooms);
  std::copy_n(spawn_deck.begin(), sp.deck_count, sp.deck);
  MapDisk map;
  map.width = map_width; map.height = map_height;
  map.golden_keys = std::clamp(golden_keys, 0, 65535);
  map.crystal_keys = std::clamp(crystal_keys, 0, 65535);
  map.legacy_keys = legacy_keys;
  map.loot_second_deck = loot_second_deck;
  memcpy(map.loot_remaining, loot_remaining, sizeof(loot_remaining));
  memcpy(map.tiles, tiles, sizeof(tiles));
  memcpy(map.seen, seen, sizeof(seen));
  const bool ok = fwrite(&d, sizeof(d), 1, f) == 1 && fwrite(&sp, sizeof(sp), 1, f) == 1 &&
                  fwrite(&map, sizeof(map), 1, f) == 1;
  return fclose(f) == 0 && ok;
}

bool Game::load(int slot) {
  char path[64];
  slot_path(slot, path, sizeof(path));
  FILE* f = fopen(path, "rb");
  if (!f) return false;
  GameDisk d{};
  bool ok = fread(&d, sizeof(d), 1, f) == 1;
  SpawnDisk sp;
  const bool have_spawn = fread(&sp, sizeof(sp), 1, f) == 1 &&
      sp.magic == 0x31575053 && sp.room_count <= 25 && sp.deck_count <= 16 && sp.remaining <= 150;
  MapDisk map;
  const bool have_map = fread(&map, sizeof(map), 1, f) == 1 && map.magic == 0x3150414D &&
                        map.width >= 3 && map.width <= MW && map.height >= 3 && map.height <= MH;
  fclose(f);
  if (!ok || d.magic != SAVE_MAGIC ||
      (d.version != LEGACY_SAVE_VERSION && (d.version != SAVE_VERSION || !have_map))) return false;
  if (d.floor < 1 || d.floor > 25 || d.px < 0 || d.py < 0 ||
      d.px >= (have_map ? map.width : LEGACY_MAP_SIZE) ||
      d.py >= (have_map ? map.height : LEGACY_MAP_SIZE)) return false;
  seed = d.seed; rng = d.rng;
  floor = d.floor; hero = d.hero;
  px = d.px; py = d.py;
  hp = d.hp; maxhp = d.maxhp; level = d.level; xp = d.xp; xpnext = d.xpnext;
  keys = d.keys; gold = d.gold; turns = d.turns; shield = d.shield;
  weapon = d.weapon; weapon_level = d.weapon_level;
  armor = d.armor; armor_level = d.armor_level;
  ring1 = d.ring1; ring1_level = d.ring1_level;
  ring2 = d.ring2; ring2_level = d.ring2_level;
  haste_t = d.haste_t; invis_t = d.invis_t; might_t = d.might_t;
  levit_t = d.levit_t; mind_t = d.mind_t; rage_t = d.rage_t;
  quest = d.quest;
  map_width = have_map ? map.width : LEGACY_MAP_SIZE;
  map_height = have_map ? map.height : LEGACY_MAP_SIZE;
  golden_keys = have_map ? map.golden_keys : 0;
  crystal_keys = have_map ? map.crystal_keys : 0;
  legacy_keys = !have_map || map.legacy_keys;
  loot_second_deck = have_map ? map.loot_second_deck != 0 : (seed >> 31) != 0;
  memcpy(loot_remaining, map.loot_remaining, sizeof(loot_remaining));
  if (!have_map) memset(loot_remaining, 0, sizeof(loot_remaining));
  if (have_map) {
    memcpy(tiles, map.tiles, sizeof(tiles));
    memcpy(seen, map.seen, sizeof(seen));
  } else {
    memset(tiles, T_WALL, sizeof(tiles));
    memset(seen, 0, sizeof(seen));
    for (int y = 0; y < LEGACY_MAP_SIZE; ++y) {
      memcpy(tiles[y], d.tiles[y], sizeof(d.tiles[y]));
      memcpy(seen[y], d.seen[y], sizeof(d.seen[y]));
    }
  }
  mobs.clear();
  for (int i = 0; i < d.mob_n && i < 48; i++) {
    Mob m;
    m.x = d.mobs[i].x; m.y = d.mobs[i].y;
    m.hp = d.mobs[i].hp; m.maxhp = d.mobs[i].maxhp;
    m.atk = d.mobs[i].atk; m.def = d.mobs[i].def;
    m.type = d.mobs[i].type; m.alive = d.mobs[i].alive != 0;
    m.sleep_t = d.mobs[i].sleep_t; m.terror_t = d.mobs[i].terror_t;
    m.vx = (float)m.x; m.vy = (float)m.y;
    mobs.push_back(m);
  }
  items.clear();
  for (int i = 0; i < d.item_n && i < 48; i++)
    items.push_back({d.items[i].x, d.items[i].y, d.items[i].id, d.items[i].level});
  inv_n = std::min((int)d.inv_n, BAG_MAX);
  for (int i = 0; i < inv_n; i++)
    inv[i] = {d.inv[i].id, d.inv[i].count, d.inv[i].level, d.inv[i].charges};
  features.clear();
  for (int i = 0; i < d.feature_n && i < 64; i++) {
    Feature f;
    f.x = d.features[i].x; f.y = d.features[i].y;
    f.type = d.features[i].type; f.state = d.features[i].state;
    if (!have_map && f.type == F_TRAP_FOUND) f.type = F_TRAP_DISARMED;
    f.aux = d.features[i].aux;
    f.loot_id = d.features[i].loot_id;
    f.loot_level = d.features[i].loot_level;
    f.loot_gold = d.features[i].loot_gold;
    features.push_back(f);
  }
  memcpy(boss_dead, d.boss_dead, sizeof(boss_dead));
  tokens = d.tokens; token_quest = d.token_quest;
  smith_used = d.smith_used; imp_state = d.imp_state;
  ghost_floor = d.ghost_floor; ghost_state = d.ghost_state;
  ghost_boss_dead = d.ghost_boss_dead;
  wand_floor = d.wand_floor; wand_state = d.wand_state; have_dust = d.have_dust;
  smith_floor = d.smith_floor; smith_state = d.smith_state; ore_count = d.ore_count;
  imp_floor = d.imp_floor; imp_kills = d.imp_kills; imp_target = d.imp_target;
  reward1 = d.reward1; reward1_level = d.reward1_level;
  reward2 = d.reward2; reward2_level = d.reward2_level;
  bag_potion = d.bag_potion != 0;
  bag_scroll = d.bag_scroll != 0;
  bag_wand = d.bag_wand != 0;
  memcpy(shop_warned, d.shop_warned, sizeof(shop_warned));
  memcpy(shop_gone, d.shop_gone, sizeof(shop_gone));
  dust_timer = d.dust_timer;
  memcpy(talents, d.talents, sizeof(talents));
  talent_tier = d.talent_tier;
  buyback_n = std::min((int)d.buyback_n, 3);
  for (int i = 0; i < buyback_n; i++) {
    buyback_items[i] = {d.buyback_items[i].id, d.buyback_items[i].count,
                        d.buyback_items[i].level, d.buyback_items[i].charges};
    buyback_prices[i] = d.buyback_prices[i];
  }
  shop_n = std::min((int)d.shop_n, 20);
  shop_floor = d.shop_floor ? d.shop_floor : -1;
  for (int i = 0; i < shop_n; i++) {
    shop_items[i] = {d.shop_items[i].id, d.shop_items[i].count,
                     d.shop_items[i].level, d.shop_items[i].charges};
    shop_prices[i] = d.shop_prices[i];
    shop_sold[i] = d.shop_sold[i] != 0;
  }
  memset(vis, 0, sizeof(vis));
  search_fx.clear();
  search_ms = 0;
  pvx = (float)px; pvy = (float)py; panim_ms = 0;
  hero_combat = {};
  combat_fx.clear();
  combat_delay = 0;
  clear_path();
  cursor_x = px; cursor_y = py;
  targeting = false;
  paused = backpack = shop_open = false;
  dialog = false;
  dead = false;
  won = false;
  msg[0] = 0;
  build_run_plan();
  fov_radius = (plan_feeling[floor - 1] == 6) ? 5 : 8;
  spawn_rooms.clear();
  spawn_deck.clear();
  if (have_spawn) {
    for (int i = 0; i < sp.room_count; ++i) {
      auto r = sp.rooms[i];
      if (r.w >= 3 && r.h >= 3 && r.x > 0 && r.y > 0 &&
          r.x + r.w < map_width && r.y + r.h < map_height) spawn_rooms.push_back(r);
    }
    if (std::all_of(sp.deck, sp.deck + sp.deck_count, [](uint8_t t) { return t < M_COUNT; }))
      spawn_deck.assign(sp.deck, sp.deck + sp.deck_count);
  }
  // V9 files without room metadata cannot distinguish standard and secret rooms.
  mob_cap = (floor == 1 || floor % 5 == 0 || spawn_rooms.empty()) ? 0 : 3 + floor % 5;
  respawn_t = have_spawn ? sp.remaining : 0;
  check_talents();
  recompute_fov();
  return true;
}

bool Game::slot_meta(int slot, SlotMeta& out) {
  char path[64];
  slot_path(slot, path, sizeof(path));
  FILE* f = fopen(path, "rb");
  if (!f) return false;
  uint32_t magic = 0;
  if (fread(&magic, sizeof(magic), 1, f) != 1) { fclose(f); return false; }
  if (magic == SAVE_MAGIC) {
    fseek(f, 0, SEEK_SET);
    GameDisk d{};
    bool ok = fread(&d, sizeof(d), 1, f) == 1;
    fclose(f);
    if (!ok || (d.version != SAVE_VERSION && d.version != LEGACY_SAVE_VERSION)) return false;
    out = d.meta;
    out.used = true;
    return true;
  }
  fseek(f, 0, SEEK_SET);
  bool ok = fread(&out, sizeof(SlotMeta), 1, f) == 1;
  fclose(f);
  return ok && out.used;
}
