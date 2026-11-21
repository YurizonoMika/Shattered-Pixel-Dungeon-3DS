#include "app.h"

#include <algorithm>

static void tile_cell(uint8_t t, int& cx, int& cy) {
  // Indices from SPD's DungeonTileSheet (tiles_sewers.png is a 16x16 grid).
  int idx = 0;
  switch (t) {
    case T_WALL: idx = 48; break;      // FLAT_WALL
    case T_FLOOR: idx = 0; break;      // FLOOR
    case T_DOOR: idx = 56; break;      // FLAT_DOOR
    case T_SECRET: idx = 48; break;    // looks like a wall
    case T_STAIRS: idx = 17; break;    // EXIT
    case T_WATER: idx = 32; break;     // WATER
    case T_GRASS: idx = 2; break;      // GRASS
    default: idx = 0; break;
  }
  cx = (idx % 16) * 16;
  cy = (idx / 16) * 16;
}

static void draw_map() {
  int camx = G.px * 16 - TOP_W / 2 + 8;
  int camy = G.py * 16 - TOP_H / 2 + 8;
  int cx = G.cursor_x * 16 + 8, cy = G.cursor_y * 16 + 8;
  if (cx < camx + 44) camx = cx - 44;
  if (cx > camx + TOP_W - 44) camx = cx - (TOP_W - 44);
  if (cy < camy + 44) camy = cy - 44;
  if (cy > camy + TOP_H - 44) camy = cy - (TOP_H - 44);
  camx = std::max(0, std::min(camx, MW * 16 - TOP_W));
  camy = std::max(0, std::min(camy, MH * 16 - TOP_H));
  G.camx = camx;
  G.camy = camy;

  draw_box(0, 0, TOP_W, TOP_H, C2D_Color32(6, 8, 12, 255), 0.2f);

  int tx0 = std::max(0, camx / 16 - 1), ty0 = std::max(0, camy / 16 - 1);
  int tx1 = std::min(MW - 1, camx / 16 + 26), ty1 = std::min(MH - 1, camy / 16 + 17);
  for (int y = ty0; y <= ty1; y++) {
    for (int x = tx0; x <= tx1; x++) {
      if (!G.seen[y][x]) continue;
      int scx, scy;
      tile_cell(G.tiles[y][x], scx, scy);
      u32 tint = G.vis[y][x] ? 0 : 0x707070FF;
      draw_cell(A_tiles, scx, scy, 16, 16, (float)(x * 16 - camx),
                (float)(y * 16 - camy), 1, 1, tint);
    }
  }

  // floor items
  for (const FloorItem& it : G.items) {
    if (!G.vis[it.y][it.x]) continue;
    draw_item_icon(it.id, (float)(it.x * 16 - camx), (float)(it.y * 16 - camy), 1.0f);
  }

  // mobs
  for (const Mob& m : G.mobs) {
    if (!m.alive || !G.vis[m.y][m.x]) continue;
    const Img* sheet = &A_rat;
    u32 tint = 0;
    switch (m.type) {
      case M_RAT: sheet = &A_rat; break;
      case M_ALBINO: sheet = &A_rat; tint = 0xFFD6E6FF; break;
      case M_GNOLL: sheet = &A_gnoll; break;
      case M_CRAB: sheet = &A_crab; break;
      case M_BOSS: sheet = &A_gnoll; tint = 0xFF9090FF; break;
      default: sheet = &A_gnoll; tint = 0x9FC0FFFF; break;
    }
    draw_cell(*sheet, 0, 0, 16, 16, (float)(m.x * 16 - camx),
              (float)(m.y * 16 - camy), 1, 1, tint);
  }

  // spell / bomb impact spark
  if (G.fx_t > 0 && G.fx_x >= 0) {
    draw_box((float)(G.fx_x * 16 - camx + 6), (float)(G.fx_y * 16 - camy + 6),
             4, 4, CLR_YELLOW, 0.7f);
  }

  // player
  draw_cell(A_hero[G.hero], 0, 0, 16, 16, (float)(G.px * 16 - camx),
            (float)(G.py * 16 - camy), 1, 1, 0);

  // cursor
  u32 cc = G.targeting ? CLR_YELLOW : CLR_GREEN;
  draw_cursor((float)(G.cursor_x * 16 - camx), (float)(G.cursor_y * 16 - camy),
              16, 16, cc);

  if (G.targeting) {
    bool wand = G.target_item >= 0 && G.target_item < G.inv_n &&
                item_def(G.inv[G.target_item].id).kind == K_WAND;
    text(wand ? "选择目标: A 施法   B 取消" : "选择目标: A 投掷   B 取消",
         10, 8, CLR_YELLOW, 1.0f);
  }

  // bottom hint bar
  draw_box(0, TOP_H - 22, TOP_W, 22, C2D_Color32(0, 0, 0, 160), 0.55f);
  text("B 返回主菜单", 8, TOP_H - 17, CLR_GREY, 1.0f);
  text("A 移动   Y 交互", TOP_W - 138, TOP_H - 17, CLR_GREY, 1.0f);
  if (S.show_fps) textf(TOP_W - 96, 4, CLR_GREY, 1.0f, "FPS %.0f", FPS);
}

static void draw_dialog() {
  draw_box(0, 0, TOP_W, TOP_H, C2D_Color32(0, 0, 0, 150), 0.6f);
  draw_panel(8, 8, TOP_W - 16, TOP_H - 36, 2.0f);

  // left 3/4: portrait + name + text
  float lx = 20, ly = 20;
  draw_button(lx, ly, 56, 56, "", false, 1.6f);
  if (G.dlg_type >= 2)
    draw_cell(A_hero[G.hero], 0, 0, 16, 16, lx + 12, ly + 12, 2.0f, 2.0f,
              G.dlg_type == 2 ? 0xBBBBBBFF : 0xFFFFFF);
  else
    draw_cell(A_gnoll, 0, 0, 16, 16, lx + 12, ly + 12, 2.0f, 2.0f, 0x9FC0FFFF);
  text(G.dlg_name, lx + 66, ly + 2,
       G.dlg_type == 2 ? CLR_RED : CLR_YELLOW, 2.0f);
  text_wrap(G.dlg_text, lx, ly + 68, 268, CLR_WHITE, 1.0f, 2.0f);

  // right 1/4: ABXY actions
  float rx = TOP_W - 104;
  static const char* keys[4] = {"A", "B", "X", "Y"};
  static const u32 kcol[4] = {CLR_GREEN, CLR_RED, CLR_BLUE, CLR_YELLOW};
  for (int i = 0; i < G.dlg_optn; i++) {
    float by = 18 + i * 48;
    draw_button(rx, by, 88, 42, "", false, 1.6f);
    draw_box(rx + 6, by + 11, 20, 20, 0x33000000 | (kcol[i] & 0xFFFFFF), 0.56f);
    text_c(keys[i], rx + 6, 20, by + 13, CLR_BLACK, 1.0f);
    text_wrap(G.dlg_opt[i], rx + 30, by + 8, 54, CLR_WHITE, 1.0f, 0.5f);
  }
  text_c("A / B / X / Y 选择", 8, TOP_W - 16, TOP_H - 30, CLR_GREY, 1.0f);
}

void game_draw_top() {
  static bool once = false;
  if (!once) { once = true; dlog("first game_draw_top"); }
  draw_map();
  if (G.dialog) draw_dialog();
}

// ---------------------------------------------------------------------------
// Bottom screen
// ---------------------------------------------------------------------------
static const float SLOT_X = 32, SLOT_Y = 34, SLOT_SIZE = 32;

static void draw_status() {
  draw_box(0, 0, BOT_W, 32, C2D_Color32(12, 14, 20, 255), 0.3f);
  // HP
  draw_box(6, 5, 108, 9, C2D_Color32(50, 12, 12, 255), 0.4f);
  float f = G.maxhp > 0 ? (float)G.hp / G.maxhp : 0;
  if (f < 0) f = 0;
  draw_box(6, 5, 108 * f, 9, C2D_Color32(210, 46, 46, 255), 0.41f);
  textf(6, 16, CLR_WHITE, 1.0f, "HP %d/%d", G.hp, G.maxhp);
  if (G.shield > 0) textf(78, 16, CLR_BLUE, 1.0f, "盾%d", G.shield);
  // level + xp
  textf(122, 3, CLR_YELLOW, 1.0f, "Lv %d", G.level);
  draw_box(122, 16, 70, 6, C2D_Color32(30, 34, 46, 255), 0.4f);
  float xf = G.xpnext > 0 ? (float)G.xp / G.xpnext : 0;
  draw_box(122, 16, 70 * xf, 6, C2D_Color32(80, 150, 240, 255), 0.41f);
  // depth (top right), keys + gold on the second line
  textf(206, 3, CLR_WHITE, 1.0f, "第 %d 层", G.floor);
  draw_item_icon(IT_KEY, 196, 15, 0.8f);
  textf(214, 17, CLR_WHITE, 1.0f, "x%d", G.keys);
  textf(252, 17, CLR_YELLOW, 1.0f, "%d 金", G.gold);
}

static void draw_quickslots() {
  for (int i = 0; i < 8; i++) {
    float x = SLOT_X + i * SLOT_SIZE;
    draw_cell(A_toolbar, 0, 0, 32, 32, x, SLOT_Y, 1, 1, 0, 0.4f);
    if (i < G.inv_n) {
      const ItemDef& d = item_def(G.inv[i].id);
      draw_item_icon(G.inv[i].id, x + 8, SLOT_Y + 6, 1.0f);
      int num = d.kind == K_WAND ? G.inv[i].charges : G.inv[i].count;
      if (d.kind == K_WAND || num > 1)
        textf(x + 22, SLOT_Y + 18, CLR_WHITE, 1.0f, "%d", num);
    }
    if (G.targeting && G.target_item == i)
      draw_cursor(x, SLOT_Y, SLOT_SIZE, SLOT_SIZE, CLR_YELLOW);
  }
}

static void draw_action_bar() {
  static const char* labels[4] = {"等待", "搜索", "背包", "菜单"};
  for (int i = 0; i < 4; i++) {
    float x = 3 + i * 78, y = 196, w = 74, h = 38;
    draw_button(x, y, w, h, "", false, 1.6f);
    float ix = x + 25, iy = y + 5;
    if (i == 0) draw_icon(112, 32, 11, 11, ix + 2, iy + 2, 1.6f);
    else if (i == 1) draw_icon(32, 48, 14, 14, ix, iy + 1, 1.5f);
    else if (i == 2) draw_icon(0, 48, 16, 16, ix, iy, 1.4f);
    else draw_icon(48, 16, 14, 15, ix, iy, 1.5f);
    text_c(labels[i], x, w, y + 23, CLR_WHITE, 1.0f);
  }
}

static void draw_messages() {
  if (G.msg_timer > 0 && G.msg[0]) {
    draw_box(6, 70, BOT_W - 12, 22, C2D_Color32(0, 0, 0, 160), 0.38f);
    text_wrap(G.msg, 12, 74, BOT_W - 24, CLR_WHITE, 1.0f, 0.5f);
  }
  text_wrap("摇杆/十字键移动光标，A 走向目标，Y 交互。", 10, 104, BOT_W - 20,
            CLR_GREY, 1.0f, 1.0f);
  text_wrap("快捷栏：点击道具使用；法杖/炸弹可进入瞄准模式。", 10, 134, BOT_W - 20,
            CLR_GREY, 1.0f, 1.0f);
  textf(10, 166, C2D_Color32(198, 198, 206, 255), 1.0f, "回合 %d", G.turns);
}

static void draw_backpack() {
  draw_box(0, 0, BOT_W, BOT_H, C2D_Color32(0, 0, 0, 170), 0.6f);
  draw_panel(8, 38, BOT_W - 16, 184, 2.0f);
  text("背包", 20, 40, CLR_YELLOW, 2.0f);
  textf(BOT_W - 130, 46, CLR_GREY, 1.0f, "%d 金  %d 钥匙", G.gold, G.keys);
  // equipped gear
  textf(20, 68, CLR_GREY, 1.0f, "武器: %s", item_name(G.weapon));
  if (G.weapon_level) textf(168, 68, CLR_YELLOW, 1.0f, "+%d", G.weapon_level);
  textf(20, 82, CLR_GREY, 1.0f, "护甲: %s", item_name(G.armor));
  if (G.armor_level) textf(168, 82, CLR_YELLOW, 1.0f, "+%d", G.armor_level);

  for (int i = 0; i < 8; i++) {
    float x = 18 + (i % 2) * 148, y = 98 + (i / 2) * 28;
    draw_button(x, y, 140, 26, "", false, 1.4f);
    if (i < G.inv_n) {
      const ItemDef& d = item_def(G.inv[i].id);
      draw_item_icon(G.inv[i].id, x + 6, y + 5, 1.0f);
      text(item_name(G.inv[i].id), x + 28, y + 6, CLR_WHITE, 1.0f);
      char right[16];
      if (d.kind == K_WAND) snprintf(right, sizeof(right), "%d", G.inv[i].charges);
      else if (d.kind == K_WEAPON || d.kind == K_ARMOR)
        snprintf(right, sizeof(right), "%s%d", G.inv[i].level ? "+" : "", G.inv[i].level);
      else snprintf(right, sizeof(right), "x%d", G.inv[i].count);
      text(right, x + 140 - text_w(right, 1.0f) - 6, y + 6, CLR_GREY, 1.0f);
    } else {
      text("—", x + 10, y + 6, C2D_Color32(140, 140, 150, 255), 1.0f);
    }
  }
  text_c("点击装备/使用   B 关闭", 0, BOT_W, 210, CLR_GREY, 1.0f);
}

static void draw_pause() {
  draw_box(0, 0, BOT_W, BOT_H, C2D_Color32(0, 0, 0, 170), 0.6f);
  draw_panel(60, 52, 200, 150, 2.0f);
  text_c("暂停", 60, 200, 58, CLR_YELLOW, 2.0f);
  draw_button(76, 84, 168, 30, "继续游戏", G.paused == 1, 1.8f);
  draw_button(76, 118, 168, 30, "设置", G.paused == 2, 1.8f);
  draw_button(76, 152, 168, 30, "返回主菜单", G.paused == 3, 1.8f);
  text_c("上下键 选择  A 确认  B 继续", 0, BOT_W, 210, CLR_GREY, 1.0f);
}

void game_draw_bottom() {
  static bool once = false;
  if (!once) { once = true; dlog("first game_draw_bottom"); }
  draw_box(0, 0, BOT_W, BOT_H, C2D_Color32(10, 12, 18, 255), 0.25f);
  draw_status();
  draw_quickslots();
  draw_messages();
  if (!G.backpack && !G.paused) draw_action_bar();
  if (G.backpack) draw_backpack();
  if (G.paused) draw_pause();
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
static bool in_rect(float x, float y, float rx, float ry, float rw, float rh) {
  return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

static void do_search() {
  bool found = false;
  for (int dy = -3; dy <= 3; dy++)
    for (int dx = -3; dx <= 3; dx++) {
      int x = G.px + dx, y = G.py + dy;
      if (x < 0 || y < 0 || x >= MW || y >= MH) continue;
      if (G.tiles[y][x] == T_SECRET) {
        G.tiles[y][x] = T_DOOR;
        G.seen[y][x] = true;
        found = true;
      }
    }
  G.message(found ? "你发现了一扇暗门！" : "你仔细搜索了周围，什么也没找到。");
  G.advance_turn();
}

static int s_wait_frames = 0;
static int s_wait_hp0 = 0;

static void activate_action(int i) {
  switch (i) {
    case 0: G.wait_turns(1); G.message("你等待了一回合。"); break;
    case 1: do_search(); break;
    case 2: G.backpack = !G.backpack; break;
    case 3: G.paused = 1; break;
  }
}

void game_update() {
  if (G.msg_timer > 0) G.msg_timer--;

  if (G.dialog) {
    if (IN.pressed(KEY_A)) G.dialog_choose(0);
    else if (IN.pressed(KEY_B)) G.dialog_choose(1);
    else if (IN.pressed(KEY_X)) G.dialog_choose(2);
    else if (IN.pressed(KEY_Y)) G.dialog_choose(3);
    if (!G.dialog && (G.dlg_type == 2 || G.dlg_type == 3)) {
      record_run_end(G.won);
      go_scene(Scene::SAVES);
    }
    return;
  }

  if (G.paused) {
    if (IN.pressed(KEY_B)) { G.paused = 0; return; }
    if (IN.dy) G.paused = ((G.paused - 1 + IN.dy + 3) % 3) + 1;
    int sel = G.paused;
    if (IN.pressed(KEY_A)) {
      if (sel == 1) G.paused = 0;
      else if (sel == 2) { SETTINGS_BACK = Scene::GAME; go_scene(Scene::SETTINGS); }
      else leave_game_to_saves(true);
      return;
    }
    if (IN.tap) {
      if (in_rect(IN.tap_x, IN.tap_y, 76, 84, 168, 30)) G.paused = 0;
      else if (in_rect(IN.tap_x, IN.tap_y, 76, 118, 168, 30)) {
        SETTINGS_BACK = Scene::GAME;
        go_scene(Scene::SETTINGS);
      } else if (in_rect(IN.tap_x, IN.tap_y, 76, 152, 168, 30)) {
        leave_game_to_saves(true);
      }
    }
    return;
  }

  if (G.backpack) {
    if (IN.pressed(KEY_B)) { G.backpack = false; return; }
    if (IN.tap) {
      for (int i = 0; i < 8; i++) {
        float x = 18 + (i % 2) * 148, y = 98 + (i / 2) * 28;
        if (in_rect(IN.tap_x, IN.tap_y, x, y, 140, 26) && i < G.inv_n) {
          G.use_item(i);
          return;
        }
      }
    }
    return;
  }

  if (IN.pressed(KEY_B)) {
    if (G.targeting) { G.targeting = false; G.message("取消投掷。"); }
    else leave_game_to_saves(true);
    return;
  }

  // Cursor movement.
  if (IN.dx) G.cursor_x = std::max(0, std::min(MW - 1, G.cursor_x + IN.dx));
  if (IN.dy) G.cursor_y = std::max(0, std::min(MH - 1, G.cursor_y + IN.dy));

  if (IN.pressed(KEY_A)) G.act_at(G.cursor_x, G.cursor_y, false);
  if (IN.pressed(KEY_Y)) G.act_at(G.cursor_x, G.cursor_y, true);
  if (IN.pressed(KEY_X)) G.backpack = true;
  if (IN.pressed(KEY_L)) { G.wait_turns(1); G.message("你等待了一回合。"); }
  if (IN.pressed(KEY_R)) do_search();

  // Touch: quickslots and action buttons.
  if (IN.tap) {
    if (IN.tap_y >= SLOT_Y && IN.tap_y < SLOT_Y + SLOT_SIZE &&
        IN.tap_x >= SLOT_X && IN.tap_x < SLOT_X + SLOT_SIZE * 8) {
      int i = (int)((IN.tap_x - SLOT_X) / SLOT_SIZE);
      if (i < G.inv_n && i >= 0) G.use_item(i);
      return;
    }
    if (IN.tap_y >= 196) {
      int i = (int)((IN.tap_x - 3) / 78);
      if (i >= 0 && i < 4) activate_action(i);
      return;
    }
  }

  // Long-press the wait button: keep waiting until HP changes.
  bool on_wait = IN.touch && IN.ty >= 196 && IN.tx >= 3 && IN.tx < 77;
  if (on_wait) {
    if (s_wait_frames == 0) { s_wait_hp0 = G.hp; G.wait_turns(1); }
    s_wait_frames++;
    if (s_wait_frames > 20 && (s_wait_frames % 7) == 0 && G.hp == s_wait_hp0)
      G.wait_turns(1);
    if (G.hp != s_wait_hp0) s_wait_frames = 0;
  } else {
    s_wait_frames = 0;
  }
}

