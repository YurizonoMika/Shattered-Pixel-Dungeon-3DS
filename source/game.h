#pragma once

#include "common.h"

#define MW 41
#define MH 41

enum TileType : uint8_t {
  T_WALL = 0,
  T_FLOOR,
  T_DOOR,
  T_SECRET,
  T_STAIRS,
  T_WATER,
  T_GRASS,
};

enum ItemType : uint8_t {
  I_POTION = 0,
  I_SCROLL,
  I_KEY,
  I_GOLD,
  I_FOOD,
  I_BOMB,
};

struct Mob {
  int x = 0, y = 0;
  int type = 0;  // 0 rat, 1 gnoll, 2 npc
  int hp = 1, maxhp = 1, atk = 1;
  bool alive = true;
};

struct FloorItem {
  int x = 0, y = 0;
  int type = 0;
};

struct Item {
  int type = 0;
  int count = 1;
};

struct Game {
  uint32_t seed = 1, rng = 1;
  int floor = 1;
  int hero = 0;
  int px = 1, py = 1;
  int hp = 20, maxhp = 20, level = 1, xp = 0, xpnext = 10;
  int keys = 0, gold = 0;
  int turns = 0;
  uint8_t tiles[MH][MW]{};
  bool seen[MH][MW]{};
  bool vis[MH][MW]{};
  std::vector<Mob> mobs;
  std::vector<FloorItem> items;
  Item inv[8];
  int inv_n = 0;
  int cursor_x = 1, cursor_y = 1;
  bool targeting = false;
  int target_item = -1;
  int camx = 0, camy = 0;
  int wait_hold = 0;

  bool paused = false, backpack = false;
  bool dialog = false;
  int dlg_type = 0;  // 0 stairs, 1 npc, 2 death
  char dlg_name[32]{};
  char dlg_text[256]{};
  char dlg_opt[4][40]{};
  int dlg_optn = 0;
  bool dead = false;

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
  void attack(int mob, bool ranged, int dmg_bonus = 0);
  void advance_turn();
  void wait_turns(int n);
  void mob_turn(Mob& m);
  void message(const char* s);
  void use_item(int idx);
  void pick_item(int idx);
  void descend();
  void open_dialog(int type, const char* name, const char* text,
                   const char* o0 = nullptr, const char* o1 = nullptr,
                   const char* o2 = nullptr, const char* o3 = nullptr);
  void dialog_choose(int i);
  void add_item(int type, int count = 1);
  int hero_atk() const;
  int hero_def() const;
  void cast_or_throw(int cx, int cy);
  SlotMeta meta() const;
};

extern const char* ITEM_NAMES[6];
