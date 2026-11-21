#include "game.h"

#include <algorithm>
#include "app.h"

const char* ITEM_NAMES[6] = {"治疗药水", "探图卷轴", "钥匙", "金币", "干粮", "炸弹"};

static inline uint32_t rnd(Game& g) {
  g.rng ^= g.rng << 13;
  g.rng ^= g.rng >> 17;
  g.rng ^= g.rng << 5;
  return g.rng;
}
static inline int ri(Game& g, int a, int b) { return a + (int)(rnd(g) % (uint32_t)(b - a + 1)); }

static int absi(int v) { return v < 0 ? -v : v; }

int Game::hero_atk() const {
  static const int atk[6] = {3, 2, 4, 3, 3, 2};
  return atk[hero % 6];
}
int Game::hero_def() const {
  static const int def[6] = {2, 1, 1, 1, 1, 2};
  return def[hero % 6];
}

bool Game::walkable(int x, int y) const {
  if (x < 0 || y < 0 || x >= MW || y >= MH) return false;
  uint8_t t = tiles[y][x];
  return t == T_FLOOR || t == T_DOOR || t == T_STAIRS || t == T_WATER || t == T_GRASS;
}
bool Game::blocks_sight(int x, int y) const {
  if (x < 0 || y < 0 || x >= MW || y >= MH) return true;
  return tiles[y][x] == T_WALL || tiles[y][x] == T_SECRET;
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
  dlog("gen_level f%d", floor);
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
  if (rooms.size() < 2) rooms.push_back({1, 1, 7, 7});  // safety net
  dlog("gen rooms=%d", (int)rooms.size());

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

  // Doors at natural pinch points.
  for (int y = 1; y < MH - 1; y++)
    for (int x = 1; x < MW - 1; x++)
      if (tiles[y][x] == T_FLOOR &&
          ((blocks_sight(x - 1, y) && blocks_sight(x + 1, y)) ||
           (blocks_sight(x, y - 1) && blocks_sight(x, y + 1))) &&
          (rnd(*this) % 100) < 18)
        tiles[y][x] = T_DOOR;

  // A few secret doors against corridor walls.
  for (int i = 0; i < 2; i++) {
    int x = ri(*this, 1, MW - 2), y = ri(*this, 1, MH - 2);
    if (tiles[y][x] == T_WALL) tiles[y][x] = T_SECRET;
  }

  dlog("gen carve done");
  const Room& start = rooms.front();
  px = start.x + start.w / 2;
  py = start.y + start.h / 2;
  const Room& last = rooms.back();
  tiles[last.y + last.h / 2][last.x + last.w / 2] = T_STAIRS;

  // Some grass/water decoration for variety.
  for (int i = 0; i < 18; i++) {
    int x = ri(*this, 1, MW - 2), y = ri(*this, 1, MH - 2);
    if (tiles[y][x] == T_FLOOR && !(x == px && y == py))
      tiles[y][x] = (rnd(*this) & 1) ? T_GRASS : T_WATER;
  }

  // Monsters.
  dlog("gen terrain done");
  int nmobs = 4 + floor;
  for (int i = 0; i < nmobs; i++) {
    const Room& r = rooms[ri(*this, 1, (int)rooms.size() - 1)];
    int x = r.x + ri(*this, 1, r.w - 2), y = r.y + ri(*this, 1, r.h - 2);
    if (tiles[y][x] != T_FLOOR || (x == px && y == py)) continue;
    Mob m;
    m.x = x; m.y = y;
    m.type = (rnd(*this) % 100) < 60 ? 0 : 1;
    if (m.type == 0) { m.maxhp = 4 + floor; m.atk = 1 + floor / 3; }
    else { m.maxhp = 7 + floor * 2; m.atk = 2 + floor / 2; }
    m.hp = m.maxhp;
    mobs.push_back(m);
  }

  // Friendly NPC on the first floor: introduces the quest.
  if (floor == 1 && rooms.size() > 1) {
    const Room& r = rooms[1];
    Mob npc;
    npc.type = 2;
    npc.x = r.x + r.w / 2;
    npc.y = r.y + r.h / 2;
    npc.hp = npc.maxhp = 999;
    mobs.push_back(npc);
  }

  dlog("gen mobs=%d", (int)mobs.size());
  // Loot.
  int nloot = 4 + floor / 2;
  for (int i = 0; i < nloot; i++) {
    const Room& r = rooms[ri(*this, 0, (int)rooms.size() - 1)];
    int x = r.x + ri(*this, 1, r.w - 2), y = r.y + ri(*this, 1, r.h - 2);
    if (tiles[y][x] != T_FLOOR || (x == px && y == py)) continue;
    FloorItem it;
    it.x = x; it.y = y;
    int roll = ri(*this, 0, 99);
    if (roll < 38) it.type = I_GOLD;
    else if (roll < 60) it.type = I_POTION;
    else if (roll < 76) it.type = I_SCROLL;
    else if (roll < 88) it.type = I_KEY;
    else if (roll < 96) it.type = I_FOOD;
    else it.type = I_BOMB;
    items.push_back(it);
  }

  dlog("gen loot=%d", (int)items.size());
  cursor_x = px;
  cursor_y = py;
  camx = px * 16 - TOP_W / 2;
  camy = py * 16 - TOP_H / 2;
  recompute_fov();
  dlog("gen_level done");
}

void Game::new_run(int hero_class, uint32_t s) {
  hero = hero_class;
  seed = s ? s : 0x9E3779B9u;
  floor = 1;
  turns = 0;
  keys = gold = 0;
  level = 1;
  xp = 0;
  xpnext = 10;
  static const int hp_base[6] = {25, 20, 18, 20, 22, 20};
  maxhp = hp_base[hero % 6];
  hp = maxhp;
  inv_n = 0;
  add_item(I_POTION, 2);
  add_item(I_FOOD, 1);
  targeting = false;
  dead = false;
  dialog = false;
  paused = backpack = false;
  dlog("new_run gen");
  gen_level();
  dlog("new_run gen ok");
  message("你来到了地牢入口。");
  dlog("new_run done");
}

void Game::add_item(int type, int count) {
  if (type == I_GOLD) { gold += count * 7; return; }
  if (type == I_KEY) { keys += count; return; }
  for (int i = 0; i < inv_n; i++) {
    if (inv[i].type == type && type != I_BOMB) {
      inv[i].count += count;
      return;
    }
  }
  if (inv_n >= 8) {
    message("背包满了！");
    return;
  }
  inv[inv_n].type = type;
  inv[inv_n].count = count;
  inv_n++;
}

void Game::pick_item(int idx) {
  FloorItem it = items[idx];
  items.erase(items.begin() + idx);
  add_item(it.type, 1);
  char b[64];
  snprintf(b, sizeof(b), "拾取了 %s", ITEM_NAMES[it.type]);
  message(b);
}

void Game::move_player(int dx, int dy) {
  int nx = px + dx, ny = py + dy;
  if (!walkable(nx, ny)) return;
  int m = mob_at(nx, ny);
  if (m >= 0) {
    if (mobs[m].type == 2) {
      open_dialog(1, "幽魂", "旅行者，这座地牢里有无数亡魂。\n你愿意替我找回我的护符吗？",
                  "接受任务", "暂时离开");
      return;
    }
    attack(m, false);
    advance_turn();
    return;
  }
  px = nx;
  py = ny;
  recompute_fov();
  int it = item_at(px, py);
  if (it >= 0) pick_item(it);
  if (tiles[py][px] == T_STAIRS) {
    open_dialog(0, "向下的楼梯", "要进入更深的一层吗？", "下去 (A)", "留下 (B)");
  }
  advance_turn();
}

void Game::attack(int idx, bool ranged, int bonus) {
  Mob& m = mobs[idx];
  if (!m.alive) return;
  if (m.type == 2) {
    open_dialog(1, "幽魂", "别对我动手，旅行者。", "道歉", "离开");
    return;
  }
  int dmg = hero_atk() + ri(*this, 0, 2) + bonus;
  if (ranged) dmg += 3;
  m.hp -= dmg;
  char b[96];
  snprintf(b, sizeof(b), "%s受到了 %d 点伤害", m.type == 0 ? "巨鼠" : "豺狼人", dmg);
  message(b);
  if (m.hp <= 0) {
    m.alive = false;
    xp += (m.type == 0 ? 3 : 7);
    snprintf(b, sizeof(b), "%s被击败了！", m.type == 0 ? "巨鼠" : "豺狼人");
    message(b);
    while (xp >= xpnext) {
      xp -= xpnext;
      level++;
      xpnext = level * 10;
      maxhp += 5;
      hp = std::min(maxhp, hp + 5);
      message("升级了！");
    }
  }
}

void Game::interact_at(int x, int y) {
  int m = mob_at(x, y);
  if (m >= 0) {
    if (mobs[m].type == 2) {
      open_dialog(1, "幽魂", "旅行者，这座地牢里有无数亡魂。\n你愿意替我找回我的护符吗？",
                  "接受任务", "暂时离开");
      return;
    }
    int d = absi(x - px) + absi(y - py);
    if (d > 1) { message("距离太远，先靠近一点。"); return; }
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
    open_dialog(0, "向下的楼梯", "要进入更深的一层吗？", "下去 (A)", "留下 (B)");
    return;
  }
  // Reveal an adjacent secret door.
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
    cast_or_throw(x, y);
    return;
  }
  if (x == px && y == py) {
    advance_turn();
    message("你等待了一回合。");
    return;
  }
  // BFS path to the target, then auto-walk (up to 8 tiles).
  // Use static storage: the 3DS main thread stack is small.
  static int16_t prev[MH][MW];
  memset(prev, -1, sizeof(prev));
  static int16_t qx[MW * MH], qy[MW * MH];
  int head = 0, tail = 0;
  qx[tail] = px; qy[tail] = py; tail++;
  prev[py][px] = py * MW + px;
  bool found = false;
  while (head < tail) {
    int cx = qx[head], cy = qy[head];
    head++;
    if (cx == x && cy == y) { found = true; break; }
    static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
    for (int k = 0; k < 4; k++) {
      int nx = cx + DX[k], ny = cy + DY[k];
      if (nx < 0 || ny < 0 || nx >= MW || ny >= MH) continue;
      if (!walkable(nx, ny)) continue;
      if (mob_at(nx, ny) >= 0) continue;
      if (prev[ny][nx] != -1) continue;
      prev[ny][nx] = cy * MW + cx;
      qx[tail] = nx; qy[tail] = ny; tail++;
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
  // Reconstruct path.
  static int16_t path[MH * MW][2];
  int n = 0;
  int cx = x, cy = y;
  while (!(cx == px && cy == py) && n < MH * MW) {
    path[n][0] = cx; path[n][1] = cy;
    n++;
    int p = prev[cy][cx];
    cx = p % MW; cy = p / MW;
  }
  std::reverse(path, path + n);
  int steps = std::min(n, 8);
  for (int i = 0; i < steps; i++) {
    if (!dialog && !dead) move_player(path[i][0] - px, path[i][1] - py);
    if (dialog || dead) break;
  }
}

void Game::mob_turn(Mob& m) {
  if (!m.alive || m.type == 2) return;
  int dx = px - m.x, dy = py - m.y;
  int dist = absi(dx) + absi(dy);
  bool sees = dist <= 8 && los(m.x, m.y, px, py);
  if (!sees) {
    if ((rnd(*this) % 100) < 15) {
      int d = ri(*this, 0, 3);
      static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
      int nx = m.x + DX[d], ny = m.y + DY[d];
      if (walkable(nx, ny) && mob_at(nx, ny) < 0 && !(nx == px && ny == py)) {
        m.x = nx; m.y = ny;
      }
    }
    return;
  }
  if (dist == 1) {
    int dmg = std::max(1, m.atk + ri(*this, 0, 1) - hero_def());
    hp -= dmg;
    char b[96];
    snprintf(b, sizeof(b), "%s攻击了你，造成 %d 点伤害", m.type == 0 ? "巨鼠" : "豺狼人", dmg);
    message(b);
    return;
  }
  int sx = (dx > 0) - (dx < 0), sy = (dy > 0) - (dy < 0);
  if (absi(dx) >= absi(dy)) {
    if (walkable(m.x + sx, m.y) && mob_at(m.x + sx, m.y) < 0 && !(m.x + sx == px && m.y == py)) { m.x += sx; return; }
    if (walkable(m.x, m.y + sy) && mob_at(m.x, m.y + sy) < 0 && !(m.x == px && m.y + sy == py)) m.y += sy;
  } else {
    if (walkable(m.x, m.y + sy) && mob_at(m.x, m.y + sy) < 0 && !(m.x == px && m.y + sy == py)) { m.y += sy; return; }
    if (walkable(m.x + sx, m.y) && mob_at(m.x + sx, m.y) < 0 && !(m.x + sx == px && m.y == py)) m.x += sx;
  }
}

void Game::advance_turn() {
  if (dead) return;
  turns++;
  if (turns % 12 == 0 && hp < maxhp) hp++;
  for (Mob& m : mobs) {
    if (dead) break;
    mob_turn(m);
  }
  recompute_fov();
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

void Game::use_item(int idx) {
  if (idx < 0 || idx >= inv_n) return;
  Item& it = inv[idx];
  switch (it.type) {
    case I_POTION:
      if (hp >= maxhp) { message("生命值已满。"); return; }
      hp = std::min(maxhp, hp + 12);
      it.count--;
      message("你喝下药水，恢复了生命。");
      break;
    case I_SCROLL:
      for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++) seen[y][x] = true;
      it.count--;
      message("卷轴揭示了本层地图。");
      break;
    case I_FOOD:
      hp = std::min(maxhp, hp + 4);
      it.count--;
      message("你吃了一点干粮。");
      break;
    case I_BOMB:
      targeting = !targeting;
      target_item = idx;
      message(targeting ? "选择投掷目标（B 取消）。" : "取消投掷。");
      return;
    default:
      message("这个东西不能直接使用。");
      return;
  }
  if (it.count <= 0) {
    for (int i = idx; i < inv_n - 1; i++) inv[i] = inv[i + 1];
    inv_n--;
  }
  advance_turn();
}

void Game::cast_or_throw(int x, int y) {
  if (target_item < 0 || target_item >= inv_n) { targeting = false; return; }
  Item& it = inv[target_item];
  if (it.type != I_BOMB) { targeting = false; return; }
  if (absi(x - px) + absi(y - py) > 6) { message("太远了，够不到。"); return; }
  message("炸弹爆炸了！");
  for (size_t i = 0; i < mobs.size(); i++) {
    Mob& m = mobs[i];
    if (!m.alive || m.type == 2) continue;
    if (absi(m.x - x) <= 1 && absi(m.y - y) <= 1) attack((int)i, true, 6);
  }
  it.count--;
  if (it.count <= 0) {
    for (int i = target_item; i < inv_n - 1; i++) inv[i] = inv[i + 1];
    inv_n--;
  }
  targeting = false;
  target_item = -1;
  advance_turn();
}

void Game::descend() {
  floor++;
  if (floor > 25) floor = 25;
  gen_level();
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
    if (i == 0) { message("幽魂：谢谢你，愿灯火指引你。"); add_item(I_POTION, 1); }
    else message("幽魂叹了口气。");
  } else if (dlg_type == 2) {
    dialog = false;
  }
}

SlotMeta Game::meta() const {
  SlotMeta m;
  m.used = true;
  m.hero = (uint8_t)hero;
  m.depth = (uint8_t)floor;
  m.level = (uint8_t)level;
  m.hp = (int16_t)hp;
  m.maxhp = (int16_t)maxhp;
  m.score = floor * 100 + gold + level * 20 + turns / 10;
  m.seed = seed;
  m.turns = (uint32_t)turns;
  m.gold = gold;
  m.stamp = (uint32_t)osGetTime() / 1000;
  return m;
}
