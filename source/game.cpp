#include "game.h"

#include <algorithm>

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

struct MobStats {
  int hp, atk, def, xp;
};
static const MobStats MOB_STATS[6] = {
    {5, 1, 0, 3},    // rat
    {9, 2, 1, 6},    // gnoll
    {13, 2, 3, 9},   // sewer crab
    {11, 3, 1, 8},   // albino rat
    {34, 5, 3, 25},  // gnoll brute (boss)
    {999, 0, 0, 0},  // npc
};
static const char* MOB_NAMES[6] = {"巨鼠", "豺狼人", "下水道螃蟹",
                                   "白色巨鼠", "豺狼人首领", "幽魂"};

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
int Game::armor_bonus() const {
  if (armor == IT_NONE) return 0;
  return item_def(armor).power + armor_level;
}

bool Game::walkable(int x, int y) const {
  if (x < 0 || y < 0 || x >= MW || y >= MH) return false;
  uint8_t t = tiles[y][x];
  return t == T_FLOOR || t == T_DOOR_OPEN || t == T_STAIRS || t == T_WATER ||
         t == T_GRASS;
}
bool Game::blocks_sight(int x, int y) const {
  if (x < 0 || y < 0 || x >= MW || y >= MH) return true;
  uint8_t t = tiles[y][x];
  return t == T_WALL || t == T_SECRET || t == T_DOOR;
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
  const int R = 7;
  for (int y = 0; y < MH; y++) {
    for (int x = 0; x < MW; x++) {
      int dx = x - px, dy = y - py;
      if (dx * dx + dy * dy > R * R + 1) continue;
      if (los(px, py, x, y)) {
        vis[y][x] = true;
        seen[y][x] = true;
      }
    }
  }
  vis[py][px] = seen[py][px] = true;
}

void Game::gen_level() {
  memset(tiles, T_WALL, sizeof(tiles));
  memset(seen, 0, sizeof(seen));
  memset(vis, 0, sizeof(vis));
  mobs.clear();
  items.clear();
  rng = seed ^ (uint32_t)(floor * 2654435761u);

  struct Room { int x, y, w, h; };
  std::vector<Room> rooms;
  for (int attempt = 0; attempt < 80 && rooms.size() < 9; attempt++) {
    int w = ri(*this, 5, 9), h = ri(*this, 5, 9);
    int x = ri(*this, 1, MW - w - 2), y = ri(*this, 1, MH - h - 2);
    bool ok = true;
    for (const Room& r : rooms) {
      if (x < r.x + r.w + 1 && x + w + 1 > r.x && y < r.y + r.h + 1 &&
          y + h + 1 > r.y) {
        ok = false;
        break;
      }
    }
    if (ok) rooms.push_back({x, y, w, h});
  }
  if (rooms.size() < 2) rooms.push_back({1, 1, 7, 7});

  for (const Room& r : rooms)
    for (int y = r.y; y < r.y + r.h; y++)
      for (int x = r.x; x < r.x + r.w; x++) tiles[y][x] = T_FLOOR;

  auto carve_h = [&](int x0, int x1, int y) {
    for (int x = std::min(x0, x1); x <= std::max(x0, x1); x++)
      if (tiles[y][x] == T_WALL) tiles[y][x] = T_FLOOR;
  };
  auto carve_v = [&](int y0, int y1, int x) {
    for (int y = std::min(y0, y1); y <= std::max(y0, y1); y++)
      if (tiles[y][x] == T_WALL) tiles[y][x] = T_FLOOR;
  };
  for (size_t i = 1; i < rooms.size(); i++) {
    const Room& a = rooms[i - 1];
    const Room& b = rooms[i];
    int ax = a.x + a.w / 2, ay = a.y + a.h / 2;
    int bx = b.x + b.w / 2, by = b.y + b.h / 2;
    if (rnd(*this) & 1) { carve_h(ax, bx, ay); carve_v(ay, by, bx); }
    else { carve_v(ay, by, ax); carve_h(ax, bx, by); }
  }

  for (int y = 1; y < MH - 1; y++)
    for (int x = 1; x < MW - 1; x++)
      if (tiles[y][x] == T_FLOOR &&
          ((blocks_sight(x - 1, y) && blocks_sight(x + 1, y)) ||
           (blocks_sight(x, y - 1) && blocks_sight(x, y + 1))) &&
          (rnd(*this) % 100) < 18)
        tiles[y][x] = T_DOOR;

  for (int i = 0; i < 2; i++) {
    int x = ri(*this, 1, MW - 2), y = ri(*this, 1, MH - 2);
    if (tiles[y][x] == T_WALL) tiles[y][x] = T_SECRET;
  }

  const Room& start = rooms.front();
  px = start.x + start.w / 2;
  py = start.y + start.h / 2;
  const Room& last = rooms.back();
  int sx = last.x + last.w / 2, sy = last.y + last.h / 2;
  tiles[sy][sx] = T_STAIRS;

  for (int i = 0; i < 18; i++) {
    int x = ri(*this, 1, MW - 2), y = ri(*this, 1, MH - 2);
    if (tiles[y][x] == T_FLOOR && !(x == px && y == py))
      tiles[y][x] = (rnd(*this) & 1) ? T_GRASS : T_WATER;
  }

  auto spawn = [&](int type, int x, int y, bool scale) {
    if (x < 0 || y < 0 || x >= MW || y >= MH) return;
    if (tiles[y][x] == T_WALL) return;
    if (mob_at(x, y) >= 0) return;
    Mob m;
    m.x = x; m.y = y; m.vx = (float)x; m.vy = (float)y;
    m.type = (uint8_t)type;
    const MobStats& s = MOB_STATS[type];
    int bonus = scale ? (floor - 1) : 0;
    m.maxhp = s.hp + bonus * 2;
    m.atk = s.atk + bonus / 2;
    m.def = s.def + bonus / 2;
    m.hp = m.maxhp;
    mobs.push_back(m);
  };

  int nmobs = 5 + floor * 2;
  for (int i = 0; i < nmobs; i++) {
    const Room& r = rooms[ri(*this, 1, (int)rooms.size() - 1)];
    int x = r.x + ri(*this, 1, r.w - 2);
    int y = r.y + ri(*this, 1, r.h - 2);
    int roll = ri(*this, 0, 99), type;
    if (floor == 1) type = roll < 65 ? M_RAT : M_GNOLL;
    else if (floor == 2) type = roll < 25 ? M_RAT : (roll < 75 ? M_GNOLL : M_CRAB);
    else type = roll < 40 ? M_GNOLL : (roll < 75 ? M_CRAB : M_ALBINO);
    spawn(type, x, y, true);
  }

  if (floor == 3 && !boss_dead) {
    spawn(M_BOSS, sx - 1, sy, false);
  }

  if (floor == 1 && rooms.size() > 1) {
    const Room& r = rooms[1];
    Mob npc;
    npc.type = M_NPC;
    npc.x = r.x + r.w / 2;
    npc.y = r.y + r.h / 2;
    npc.vx = (float)npc.x; npc.vy = (float)npc.y;
    npc.hp = npc.maxhp = 999;
    mobs.push_back(npc);
  }

  // Quest amulet on floor 2 once the ghost's quest is accepted.
  if (floor == 2 && quest == 1) {
    items.push_back({sx, sy - 1, IT_AMULET, 0});
  }

  int nloot = 6 + floor;
  for (int i = 0; i < nloot; i++) {
    const Room& r = rooms[ri(*this, 0, (int)rooms.size() - 1)];
    int x = r.x + ri(*this, 1, r.w - 2), y = r.y + ri(*this, 1, r.h - 2);
    if (tiles[y][x] != T_FLOOR || (x == px && y == py)) continue;
    if (item_at(x, y) >= 0 || mob_at(x, y) >= 0) continue;
    int roll = ri(*this, 0, 99);
    uint8_t id = IT_GOLD;
    int lvl = 0;
    if (roll < 22) id = IT_GOLD;
    else if (roll < 34) id = IT_POTION_HEAL;
    else if (roll < 44) id = IT_SCROLL_MAP;
    else if (roll < 54) id = IT_FOOD_RATION;
    else if (roll < 62) id = IT_BOMB;
    else if (roll < 68) id = IT_KEY;
    else if (roll < 78) id = IT_SHORTSWORD;
    else if (roll < 86) id = IT_LEATHER_ARMOR;
    else if (roll < 93) id = (floor >= 2 && (roll & 1)) ? IT_WAND_FIRE : IT_WAND_MAGIC;
    else if (roll < 96) id = IT_POTION_STR;
    else if (roll < 98) id = IT_SCROLL_UP;
    else id = IT_SCROLL_TELE;
    if (id == IT_SHORTSWORD || id == IT_LEATHER_ARMOR) lvl = ri(*this, 0, floor - 1);
    items.push_back({x, y, id, (uint8_t)lvl});
  }

  cursor_x = px;
  cursor_y = py;
  camx = px * 16 - TOP_W / 2;
  camy = py * 16 - TOP_H / 2;
  recompute_fov();
}

void Game::new_run(int hero_class, uint32_t s) {
  hero = hero_class;
  seed = s ? s : 0x9E3779B9u;
  rng = seed;
  floor = 1;
  turns = 0;
  keys = gold = shield = 0;
  level = 1;
  xp = 0;
  xpnext = 10;
  quest = 0;
  boss_dead = 0;
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
  paused = backpack = false;
  clear_path();
  gen_level();
  pvx = (float)px; pvy = (float)py; panim_ms = 0;
  message("你来到了地牢入口。");
}

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
  if (id == IT_GOLD) { gold += count * 7; return; }
  if (id == IT_KEY) { keys += count; return; }
  const ItemDef& d = item_def(id);
  if (item_is_stackable(id)) {
    for (int i = 0; i < inv_n; i++) {
      if (inv[i].id == id) {
        inv[i].count = (uint8_t)std::min(99, inv[i].count + count);
        return;
      }
    }
  }
  if (inv_n >= 8) {
    message("背包满了！");
    return;
  }
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
      if (it.id == IT_POTION_HEAL) {
        if (hp >= maxhp) { message("生命值已满。"); return; }
        hp = std::min(maxhp, hp + d.power);
        message("你喝下治疗药水。");
      } else if (it.id == IT_POTION_STR) {
        maxhp += 2;
        hp = std::min(maxhp, hp + 2);
        message("力量涌入身体，最大生命 +2。");
      } else if (it.id == IT_POTION_SHIELD) {
        shield += d.power;
        message("一层护盾包围了你。");
      } else if (it.id == IT_SCROLL_MAP) {
        for (int y = 0; y < MH; y++)
          for (int x = 0; x < MW; x++) seen[y][x] = true;
        message("卷轴揭示了本层地图。");
      } else if (it.id == IT_SCROLL_UP) {
        if (weapon != IT_NONE) {
          weapon_level++;
          message("你的武器变得更强了。");
        } else {
          armor_level++;
          message("你的护甲变得更强了。");
        }
      } else if (it.id == IT_SCROLL_TELE) {
        int tries = 0;
        do {
          px = ri(*this, 1, MW - 2);
          py = ri(*this, 1, MH - 2);
        } while ((!walkable(px, py) || mob_at(px, py) >= 0) && ++tries < 200);
        recompute_fov();
        message("你被传送到本层的另一个角落。");
      }
      break;
    case K_FOOD:
      hp = std::min(maxhp, hp + d.power);
      message("你吃了一些食物。");
      break;
    case K_WAND:
      if (it.charges == 0) {
        message("法杖还没有充能。");
        return;
      }
      targeting = !targeting;
      target_item = idx;
      message(targeting ? "选择施法目标（B 取消）。" : "取消施法。");
      return;
    case K_BOMB:
      targeting = !targeting;
      target_item = idx;
      message(targeting ? "选择投掷目标（B 取消）。" : "取消投掷。");
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

void Game::cast_wand(int idx, int tx, int ty) {
  if (idx < 0 || idx >= inv_n) { targeting = false; return; }
  Item& it = inv[idx];
  const ItemDef& d = item_def(it.id);
  if (d.kind != K_WAND || it.charges == 0) { targeting = false; return; }
  if (absi(tx - px) + absi(ty - py) > 6 || !los(px, py, tx, ty)) {
    message("目标不可达。");
    return;
  }
  int dmg = d.power + it.level * 2 + ri(*this, 0, 2);
  int hits = 0;
  for (size_t i = 0; i < mobs.size(); i++) {
    Mob& m = mobs[i];
    if (!m.alive || m.type == M_NPC) continue;
    if (absi(m.x - tx) > 1 || absi(m.y - ty) > 1) continue;
    if (!los(px, py, m.x, m.y)) continue;
    if (it.id == IT_WAND_MAGIC && hits > 0) break;  // single target
    damage_mob((int)i, dmg);
    hits++;
  }
  it.charges--;
  fx_x = tx; fx_y = ty; fx_t = 8;
  if (hits == 0) message("法术击中了空处。");
  targeting = false;
  target_item = -1;
  advance_turn();
}

void Game::pick_item(int idx) {
  FloorItem fi = items[idx];
  items.erase(items.begin() + idx);
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

void Game::move_player(int dx, int dy) {
  int nx = px + dx, ny = py + dy;
  if (nx < 0 || ny < 0 || nx >= MW || ny >= MH) return;
  if (tiles[ny][nx] == T_DOOR) {
    tiles[ny][nx] = T_DOOR_OPEN;
    message("你推开了门。");
  }
  if (!walkable(nx, ny)) return;
  int m = mob_at(nx, ny);
  if (m >= 0) {
    if (mobs[m].type == M_NPC) {
      if (quest == 0)
        open_dialog(1, "幽魂", "旅行者，这座地牢里有无数亡魂。\n你愿意替我找回我的护符吗？",
                    "接受任务", "暂时离开");
      else if (quest == 1)
        open_dialog(1, "幽魂", "护符应该在第 2 层的某个角落。", "继续寻找");
      else if (quest == 2)
        open_dialog(1, "幽魂", "这就是我的护符……谢谢你。\n收下这些吧。", "收下奖励");
      else
        open_dialog(1, "幽魂", "愿灯火指引你，旅行者。", "离开");
      return;
    }
    attack(m, false);
    advance_turn();
    return;
  }
  pvx = (float)px; pvy = (float)py;
  px = nx;
  py = ny;
  recompute_fov();
  int it = item_at(px, py);
  if (it >= 0) pick_item(it);
  if (tiles[py][px] == T_STAIRS) {
    if (floor >= 3)
      open_dialog(0, "地牢出口", "你已经抵达试玩内容的终点。\n要带着战利品离开吗？",
                  "离开地牢 (A)", "继续探索 (B)");
    else
      open_dialog(0, "向下的楼梯", "要进入更深的一层吗？", "下去 (A)", "留下 (B)");
  }
  advance_turn();
}

void Game::damage_mob(int idx, int raw) {
  if (idx < 0 || idx >= (int)mobs.size()) return;
  Mob& m = mobs[idx];
  if (!m.alive) return;
  int dmg = raw - m.def;
  if (dmg < 1) dmg = 1;
  m.hp -= dmg;
  char b[96];
  snprintf(b, sizeof(b), "%s受到了 %d 点伤害", MOB_NAMES[m.type], dmg);
  message(b);
  if (m.hp > 0) return;
  m.alive = false;
  xp += MOB_STATS[m.type].xp;
  snprintf(b, sizeof(b), "%s被击败了！", MOB_NAMES[m.type]);
  message(b);
  if (m.type == M_BOSS) {
    boss_dead = 1;
    items.push_back({m.x, m.y, IT_SHORTSWORD, 2});
    add_item(IT_GOLD, 8);
    message("首领倒下了，掉落了一把强化短剑。");
  } else if ((rnd(*this) % 100) < 22) {
    items.push_back({m.x, m.y, (uint8_t)((rnd(*this) & 1) ? IT_POTION_HEAL : IT_GOLD), 0});
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
  if (m.type == M_NPC) {
    open_dialog(1, "幽魂", "别对我动手，旅行者。", "道歉", "离开");
    return;
  }
  int raw = hero_atk() + weapon_bonus() + ri(*this, 0, 2) + bonus;
  if (ranged) raw += 1;
  damage_mob(idx, raw);
}

void Game::interact_at(int x, int y) {
  int m = mob_at(x, y);
  if (m >= 0) {
    if (mobs[m].type == M_NPC) {
      if (quest == 0)
        open_dialog(1, "幽魂", "旅行者，这座地牢里有无数亡魂。\n你愿意替我找回我的护符吗？",
                    "接受任务", "暂时离开");
      else if (quest == 1)
        open_dialog(1, "幽魂", "护符应该在第 2 层的某个角落。", "继续寻找");
      else if (quest == 2)
        open_dialog(1, "幽魂", "这就是我的护符……谢谢你。\n收下这些吧。", "收下奖励");
      else
        open_dialog(1, "幽魂", "愿灯火指引你，旅行者。", "离开");
      return;
    }
    if (absi(x - px) + absi(y - py) > 1) { message("距离太远，先靠近一点。"); return; }
    attack(m, false);
    advance_turn();
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
    if (floor >= 3)
      open_dialog(0, "地牢出口", "你已经抵达试玩内容的终点。\n要带着战利品离开吗？",
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
        seen[ny][nx] = true;
        message("你发现了一扇暗门！");
        recompute_fov();
        advance_turn();
        return;
      }
    }
  message("这里没有可以交互的东西。");
}

void Game::act_at(int x, int y, bool interact) {
  if (dialog || dead) return;
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
  static int16_t prev[MH][MW];
  memset(prev, -1, sizeof(prev));
  static int16_t qx[MW * MH], qy[MW * MH];
  int head = 0, tail = 0;
  qx[tail] = (int16_t)px; qy[tail] = (int16_t)py; tail++;
  prev[py][px] = (int16_t)(py * MW + px);
  bool found = false;
  while (head < tail) {
    int cx = qx[head], cy = qy[head];
    head++;
    if (cx == x && cy == y) { found = true; break; }
    static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) {
      int nx = cx + DX[k], ny = cy + DY[k];
      if (nx < 0 || ny < 0 || nx >= MW || ny >= MH) continue;
      if (!walkable(nx, ny) && tiles[ny][nx] != T_DOOR) continue;
      if (mob_at(nx, ny) >= 0) continue;
      if (prev[ny][nx] != -1) continue;
      prev[ny][nx] = (int16_t)(cy * MW + cx);
      qx[tail] = (int16_t)nx; qy[tail] = (int16_t)ny; tail++;
    }
  }
  if (!found) {
    int m = mob_at(x, y);
    if (m >= 0 && absi(x - px) + absi(y - py) == 1) {
      attack(m, false);
      advance_turn();
    } else {
      message("无法到达那里。");
    }
    return;
  }
  static int16_t path[MH * MW][2];
  int n = 0;
  int cx = x, cy = y;
  while (!(cx == px && cy == py) && n < MH * MW) {
    path[n][0] = (int16_t)cx; path[n][1] = (int16_t)cy;
    n++;
    int p = prev[cy][cx];
    cx = p % MW; cy = p / MW;
  }
  std::reverse(path, path + n);
  int steps = std::min(n, 8);
  path_n = steps;
  path_i = 0;
  for (int i = 0; i < steps; i++) {
    path_x[i] = path[i][0];
    path_y[i] = path[i][1];
  }
}

void Game::clear_path() {
  path_n = path_i = 0;
}

void Game::tick(int dt_ms) {
  if (dt_ms < 1) dt_ms = 1;
  if (dt_ms > 100) dt_ms = 100;
  const float sp = dt_ms * 0.014f;  // ~14 tiles / second

  if (!freeze_anim) {
    if (pvx < px) pvx = std::min((float)px, pvx + sp);
    else if (pvx > px) pvx = std::max((float)px, pvx - sp);
    if (pvy < py) pvy = std::min((float)py, pvy + sp);
    else if (pvy > py) pvy = std::max((float)py, pvy - sp);
  }
  panim_ms += dt_ms;

  for (Mob& m : mobs) {
    if (!freeze_anim) {
      if (m.vx < m.x) m.vx = std::min((float)m.x, m.vx + sp);
      else if (m.vx > m.x) m.vx = std::max((float)m.x, m.vx - sp);
      if (m.vy < m.y) m.vy = std::min((float)m.y, m.vy + sp);
      else if (m.vy > m.y) m.vy = std::max((float)m.y, m.vy - sp);
    }
    m.anim_ms += dt_ms;
  }

  // Advance the queued walk once the player has visually reached the tile.
  if (!dialog && !dead && !paused && !backpack && path_i < path_n &&
      fabsf(pvx - px) < 0.02f && fabsf(pvy - py) < 0.02f) {
    int nx = path_x[path_i], ny = path_y[path_i];
    path_i++;
    int ox = px, oy = py;
    move_player(nx - px, ny - py);
    if ((px == ox && py == oy) || dialog || dead) clear_path();
  }
}

void Game::mob_turn(Mob& m) {
  if (!m.alive || m.type == M_NPC) return;
  int dx = px - m.x, dy = py - m.y;
  int dist = absi(dx) + absi(dy);
  bool sees = dist <= 8 && los(m.x, m.y, px, py);
  if (!sees) {
    if ((rnd(*this) % 100) < 15) {
      int d = ri(*this, 0, 3);
      static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
      int nx = m.x + DX[d], ny = m.y + DY[d];
      if (walkable(nx, ny) && mob_at(nx, ny) < 0 && !(nx == px && ny == py)) {
        m.vx = (float)m.x; m.vy = (float)m.y;
        m.x = nx; m.y = ny;
      }
    }
    return;
  }
  if (dist == 1) {
    int dmg = std::max(1, m.atk + ri(*this, 0, 1) - hero_def() - armor_bonus());
    if (shield > 0) {
      int absorbed = std::min(shield, dmg);
      shield -= absorbed;
      dmg -= absorbed;
    }
    if (dmg > 0) hp -= dmg;
    char b[96];
    snprintf(b, sizeof(b), "%s攻击了你，造成 %d 点伤害", MOB_NAMES[m.type], dmg);
    message(b);
    return;
  }
  int sx = (dx > 0) - (dx < 0), sy = (dy > 0) - (dy < 0);
  auto try_move = [&](int nx, int ny) {
    if (!walkable(nx, ny) || mob_at(nx, ny) >= 0 || (nx == px && ny == py)) return false;
    m.vx = (float)m.x; m.vy = (float)m.y;
    m.x = nx; m.y = ny;
    return true;
  };
  if (absi(dx) >= absi(dy)) {
    if (try_move(m.x + sx, m.y)) return;
    try_move(m.x, m.y + sy);
  } else {
    if (try_move(m.x, m.y + sy)) return;
    try_move(m.x + sx, m.y);
  }
}

void Game::recharge_wands() {
  if (turns % 15 != 0) return;
  for (int i = 0; i < inv_n; i++) {
    if (item_def(inv[i].id).kind == K_WAND) {
      uint8_t maxc = item_def(inv[i].id).max_charge;
      if (inv[i].charges < maxc) inv[i].charges++;
    }
  }
}

void Game::advance_turn() {
  if (dead) return;
  turns++;
  if (turns % 12 == 0 && hp < maxhp) hp++;
  recharge_wands();
  for (Mob& m : mobs) {
    if (dead) break;
    mob_turn(m);
  }
  recompute_fov();
  if (fx_t > 0) fx_t--;
  if (hp <= 0) {
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
  if (floor >= 3) {
    won = true;
    open_dialog(3, "试玩完成", "你抵达了当前内容的最深处。\n感谢试玩这个 3DS 移植原型！",
                "带着战利品离开 (A)");
    return;
  }
  floor++;
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
    if (i == 0) {
      dialog = false;
      descend();
    } else {
      dialog = false;
    }
  } else if (dlg_type == 1) {
    dialog = false;
    if (quest == 0 && i == 0) {
      quest = 1;
      message("幽魂：谢谢你，护符应该在第 2 层。");
    } else if (quest == 2 && i == 0) {
      quest = 3;
      add_item(IT_POTION_HEAL, 2);
      gold += 30;
      message("幽魂：收下这些药水和金币吧。");
    }
  } else {
    dialog = false;
  }
}

void Game::cast_or_throw(int x, int y) {
  if (target_item < 0 || target_item >= inv_n) { targeting = false; return; }
  Item& it = inv[target_item];
  if (item_def(it.id).kind != K_BOMB) { targeting = false; return; }
  if (absi(x - px) + absi(y - py) > 6) { message("太远了，够不到。"); return; }
  message("炸弹爆炸了！");
  for (size_t i = 0; i < mobs.size(); i++) {
    Mob& m = mobs[i];
    if (!m.alive || m.type == M_NPC) continue;
    if (absi(m.x - x) <= 1 && absi(m.y - y) <= 1) attack((int)i, true, 6);
  }
  it.count--;
  if (it.count <= 0) remove_item(target_item, 1);
  targeting = false;
  target_item = -1;
  fx_x = x; fx_y = y; fx_t = 8;
  advance_turn();
}

SlotMeta Game::meta() const {
  SlotMeta m;
  m.used = true;
  m.hero = (uint8_t)hero;
  m.depth = (uint8_t)floor;
  m.level = (uint8_t)level;
  m.hp = (int16_t)hp;
  m.maxhp = (int16_t)maxhp;
  m.score = floor * 100 + gold + level * 20 + turns / 10 + (boss_dead ? 200 : 0);
  m.seed = seed;
  m.turns = (uint32_t)turns;
  m.gold = gold;
  m.stamp = (uint32_t)osGetTime() / 1000;
  return m;
}

// ---------------------------------------------------------------------------
// Save / load
// ---------------------------------------------------------------------------
static const uint32_t SAVE_MAGIC = 0x33445053u;  // "SPD3"
static const uint16_t SAVE_VERSION = 3;

static void slot_path(int slot, char* out, size_t n) {
  snprintf(out, n, SPD_DATA_DIR "/slot%d.bin", slot);
}

bool Game::save(int slot) const {
  GameDisk d{};
  d.magic = SAVE_MAGIC;
  d.version = SAVE_VERSION;
  d.bytes = (uint16_t)sizeof(GameDisk);
  d.meta = meta();
  d.seed = seed;
  d.rng = rng;
  d.floor = (int16_t)floor;
  d.hero = (int16_t)hero;
  d.px = (int16_t)px; d.py = (int16_t)py;
  d.hp = (int16_t)hp; d.maxhp = (int16_t)maxhp;
  d.level = (int16_t)level; d.xp = (int16_t)xp; d.xpnext = (int16_t)xpnext;
  d.keys = (int16_t)keys; d.gold = (int16_t)gold;
  d.turns = (int16_t)std::min(turns, 32000);
  d.shield = (int16_t)shield;
  d.weapon = weapon; d.weapon_level = weapon_level;
  d.armor = armor; d.armor_level = armor_level;
  d.quest = quest; d.boss_dead = boss_dead;
  memcpy(d.tiles, tiles, sizeof(tiles));
  memcpy(d.seen, seen, sizeof(seen));
  d.mob_n = (uint16_t)std::min((int)mobs.size(), 48);
  for (int i = 0; i < d.mob_n; i++) {
    d.mobs[i] = {(int16_t)mobs[i].x, (int16_t)mobs[i].y, (int16_t)mobs[i].hp,
                 (int16_t)mobs[i].maxhp, (int16_t)mobs[i].atk, (int16_t)mobs[i].def,
                 mobs[i].type, (uint8_t)mobs[i].alive};
  }
  d.item_n = (uint16_t)std::min((int)items.size(), 48);
  for (int i = 0; i < d.item_n; i++) {
    d.items[i] = {(int16_t)items[i].x, (int16_t)items[i].y, items[i].id, items[i].level};
  }
  d.inv_n = (uint8_t)inv_n;
  for (int i = 0; i < inv_n; i++) {
    d.inv[i] = {inv[i].id, inv[i].count, inv[i].level, inv[i].charges};
  }
  char path[64];
  slot_path(slot, path, sizeof(path));
  FILE* f = fopen(path, "wb");
  if (!f) return false;
  bool ok = fwrite(&d, sizeof(d), 1, f) == 1;
  fclose(f);
  return ok;
}

bool Game::load(int slot) {
  char path[64];
  slot_path(slot, path, sizeof(path));
  FILE* f = fopen(path, "rb");
  if (!f) return false;
  GameDisk d{};
  bool ok = fread(&d, sizeof(d), 1, f) == 1;
  fclose(f);
  if (!ok || d.magic != SAVE_MAGIC || d.version != SAVE_VERSION) return false;
  if (d.floor < 1 || d.floor > 3) return false;
  seed = d.seed; rng = d.rng;
  floor = d.floor; hero = d.hero;
  px = d.px; py = d.py;
  hp = d.hp; maxhp = d.maxhp; level = d.level; xp = d.xp; xpnext = d.xpnext;
  keys = d.keys; gold = d.gold; turns = d.turns; shield = d.shield;
  weapon = d.weapon; weapon_level = d.weapon_level;
  armor = d.armor; armor_level = d.armor_level;
  quest = d.quest; boss_dead = d.boss_dead;
  memcpy(tiles, d.tiles, sizeof(tiles));
  memcpy(seen, d.seen, sizeof(seen));
  mobs.clear();
  for (int i = 0; i < d.mob_n && i < 48; i++) {
    Mob m;
    m.x = d.mobs[i].x; m.y = d.mobs[i].y;
    m.hp = d.mobs[i].hp; m.maxhp = d.mobs[i].maxhp;
    m.atk = d.mobs[i].atk; m.def = d.mobs[i].def;
    m.type = d.mobs[i].type; m.alive = d.mobs[i].alive != 0;
    m.vx = (float)m.x; m.vy = (float)m.y;
    mobs.push_back(m);
  }
  items.clear();
  for (int i = 0; i < d.item_n && i < 48; i++)
    items.push_back({d.items[i].x, d.items[i].y, d.items[i].id, d.items[i].level});
  inv_n = std::min((int)d.inv_n, 8);
  for (int i = 0; i < inv_n; i++)
    inv[i] = {d.inv[i].id, d.inv[i].count, d.inv[i].level, d.inv[i].charges};
  memset(vis, 0, sizeof(vis));
  pvx = (float)px; pvy = (float)py; panim_ms = 0;
  clear_path();
  cursor_x = px; cursor_y = py;
  targeting = false;
  paused = backpack = false;
  dialog = false;
  dead = false;
  won = false;
  msg[0] = 0;
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
    if (!ok || d.version != SAVE_VERSION) return false;
    out = d.meta;
    out.used = true;
    return true;
  }
  // Legacy v1 files stored SlotMeta directly.
  fseek(f, 0, SEEK_SET);
  bool ok = fread(&out, sizeof(SlotMeta), 1, f) == 1;
  fclose(f);
  return ok && out.used;
}
