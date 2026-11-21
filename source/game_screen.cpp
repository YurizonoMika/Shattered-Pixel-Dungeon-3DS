#include "app.h"

#include <algorithm>

int g_bag_sel = 0;
int g_bag_tab = 0;
bool g_bag_action = false;

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

static void draw_sprite(const Img& sheet, int fw, int fh, int frame, float x,
                        float y, u32 tint = 0) {
  int cols = sheet.w / fw;
  int cx = (frame % cols) * fw;
  int cy = (frame / cols) * fh;
  draw_cell(sheet, cx, cy, fw, fh, x, y, 1, 1, tint);
}

static void draw_hero(int hero, float vx, float vy, int anim_ms, bool moving,
                      int search_ms, int camx, int camy) {
  static const uint8_t idle[8] = {0, 0, 0, 1, 0, 0, 1, 1};
  int frame;
  if (search_ms > 0) {
    // SPD's operate animation: frames 16,17,16,17 at 8fps.
    frame = 16 + ((500 - search_ms) / 125) % 2;
  } else {
    frame = moving ? 2 + (anim_ms / 100) % 6 : idle[(anim_ms / 1000) % 8];
  }
  draw_sprite(A_hero[hero % 6], 12, 15, frame,
              vx * 16 - camx + 2, vy * 16 - camy + 1);
}

static void draw_mob(const Mob& m, int camx, int camy) {
  const MobDef& d = mob_def(m.type);
  const Img& sheet = A_mobs[d.asset];
  bool moving = fabsf(m.vx - m.x) > 0.01f || fabsf(m.vy - m.y) > 0.01f;
  int frame = 0;
  if (!d.boss) {
    frame = moving ? 2 + (m.anim_ms / 100) % 4 : (m.anim_ms / 450) % 2;
  } else {
    frame = (m.anim_ms / 400) % 2;
  }
  float ox = (16 - d.fw) * 0.5f;
  float oy = 16.0f - d.fh;
  float bob = 0;
  if (m.type == M_NPC_GHOST) bob = sinf(TIME_S * 2.0f) * 1.5f;
  draw_sprite(sheet, d.fw, d.fh, frame, m.vx * 16 - camx + ox,
              m.vy * 16 - camy + oy + bob, d.tint);
}

static void draw_feature(const Feature& f, int camx, int camy) {
  if (!G.vis[f.y][f.x] && G.mind_t == 0) return;
  float x = (float)(f.x * 16 - camx);
  float y = (float)(f.y * 16 - camy);
  switch (f.type) {
    case F_WELL:
    case F_WELL_USED: {
      int idx = f.type == F_WELL ? 18 : 19;
      draw_cell(A_tiles[G.region()], (idx % 16) * 16, (idx / 16) * 16, 16, 16,
                x, y, 1, 1, 0);
      break;
    }
    case F_BOOKSHELF:
    case F_BOOKSHELF_USED: {
      u32 tint = f.type == F_BOOKSHELF_USED ? 0x808080FF : 0;
      draw_cell(A_tiles[G.region()], 2 * 16, 3 * 16, 16, 16, x, y, 1, 1, tint);
      break;
    }
    case F_ALCHEMY:
    case F_ALCHEMY_USED: {
      u32 tint = f.type == F_ALCHEMY_USED ? 0x808080FF : 0;
      draw_cell(A_tiles[G.region()], 0, 4 * 16, 16, 16, x, y, 1, 1, tint);
      break;
    }
    case F_STATUE:
    case F_STATUE_USED: {
      u32 tint = f.type == F_STATUE_USED ? 0x808080FF : 0;
      draw_cell(A_tiles[G.region()], 8 * 16, 4 * 16, 16, 16, x, y, 1, 1, tint);
      break;
    }
    case F_BARRICADE:
      draw_cell(A_tiles[G.region()], 1 * 16, 4 * 16, 16, 16, x, y, 1, 1, 0);
      break;
    case F_CHEST:
    case F_CHEST_LOCKED:
      draw_box(x + 2, y + 5, 12, 8, C2D_Color32(120, 78, 40, 255), 0.5f);
      draw_box(x + 2, y + 3, 12, 4, C2D_Color32(86, 54, 26, 255), 0.51f);
      draw_box(x + 7, y + 8, 2, 3, f.type == F_CHEST_LOCKED ? CLR_YELLOW
                                                           : C2D_Color32(60, 40, 20, 255),
               0.52f);
      break;
    case F_CHEST_OPEN:
      draw_box(x + 2, y + 8, 12, 5, C2D_Color32(80, 52, 28, 255), 0.5f);
      draw_box(x + 2, y + 4, 12, 3, C2D_Color32(60, 40, 20, 255), 0.51f);
      break;
    case F_TOMB:
    case F_TOMB_USED:
      draw_box(x + 3, y + 4, 10, 10, C2D_Color32(110, 110, 120, 255), 0.5f);
      draw_box(x + 7, y + 6, 2, 6, C2D_Color32(60, 60, 70, 255), 0.51f);
      draw_box(x + 5, y + 8, 6, 2, C2D_Color32(60, 60, 70, 255), 0.51f);
      break;
    case F_PLANT:
      draw_cell(A_tiles[G.region()], 2 * 16, 0, 16, 16, x, y, 1, 1, 0x80FFB0FF);
      break;
    case F_SIGN:
      draw_box(x + 6, y + 4, 4, 9, C2D_Color32(90, 60, 30, 255), 0.5f);
      draw_box(x + 2, y + 3, 12, 6, C2D_Color32(150, 105, 55, 255), 0.51f);
      draw_box(x + 4, y + 5, 8, 1, C2D_Color32(80, 50, 20, 255), 0.52f);
      break;
    case F_TRAP_FOUND:
      draw_icon(32, 32, 14, 14, x + 1, y + 1, 1.0f, CLR_RED);
      break;
    case F_TOKEN: {
      float bob = sinf(TIME_S * 3.0f + f.x) * 1.5f;
      draw_item_icon(IT_AMULET, x, y + bob, 1.0f);
      break;
    }
    default:
      break;
  }
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
      float tx = (float)(x * 16 - camx), ty = (float)(y * 16 - camy);
      if (G.tiles[y][x] == T_DOOR || G.tiles[y][x] == T_DOOR_OPEN) {
        // The flat door sprite is a vertical panel. Rotate it when the wall
        // runs left-to-right so doors match the corridor orientation.
        bool horizontal_wall =
            G.blocks_sight(x - 1, y) && G.blocks_sight(x + 1, y);
        draw_cell_rot(A_tiles[G.region()], scx, scy, 16, 16, tx, ty, 1.0f,
                      horizontal_wall ? (float)M_PI * 0.5f : 0.0f, tint);
      } else {
        draw_cell(A_tiles[G.region()], scx, scy, 16, 16, tx, ty, 1, 1, tint);
      }
    }
  }

  // Search effect: SPD's CheckedCell (blue square shrinking after a delay).
  for (const SearchFx& f : G.search_fx) {
    if (f.delay > 0 || f.alpha <= 0) continue;
    float s = 16.0f * f.alpha;
    int a = (int)(f.alpha * 255);
    draw_box(f.x * 16 - camx + 8 - s * 0.5f, f.y * 16 - camy + 8 - s * 0.5f,
             s, s, C2D_Color32(0x55, 0xAA, 0xFF, a), 0.52f);
  }

  // boss health bar
  for (const Mob& m : G.mobs) {
    if (!m.alive || !mob_def(m.type).boss || !G.vis[m.y][m.x]) continue;
    const MobDef& d = mob_def(m.type);
    draw_box(60, 4, 280, 26, C2D_Color32(0, 0, 0, 150), 0.56f);
    text_c(d.name, 60, 280, 5, CLR_YELLOW, 1.0f);
    draw_box(66, 20, 268, 7, C2D_Color32(60, 12, 12, 255), 0.57f);
    float f = m.maxhp > 0 ? (float)m.hp / m.maxhp : 0;
    draw_box(66, 20, 268 * f, 7, C2D_Color32(210, 46, 46, 255), 0.58f);
  }

  // floor items: subtle floating animation + shadow
  for (const FloorItem& it : G.items) {
    if (!G.vis[it.y][it.x] && G.mind_t == 0) continue;
    float ix = (float)(it.x * 16 - camx);
    float iy = (float)(it.y * 16 - camy);
    float bob = sinf(TIME_S * 3.2f + it.x * 0.9f + it.y * 1.7f) * 1.5f;
    draw_box(ix + 4, iy + 12, 8, 3, C2D_Color32(0, 0, 0, 90), 0.45f);
    draw_item_icon(it.id, ix, iy + bob, 1.0f);
  }

  // interactables / decorations
  for (const Feature& f : G.features) draw_feature(f, camx, camy);

  // mobs
  for (const Mob& m : G.mobs) {
    if (!m.alive || (!G.vis[m.y][m.x] && G.mind_t == 0)) continue;
    draw_mob(m, camx, camy);
  }

  // spell / bomb impact spark
  if (G.fx_t > 0 && G.fx_x >= 0) {
    draw_box((float)(G.fx_x * 16 - camx + 6), (float)(G.fx_y * 16 - camy + 6),
             4, 4, CLR_YELLOW, 0.7f);
  }

  // player (hero frames are 12x15 inside the sheet)
  draw_hero(G.hero, G.pvx, G.pvy, G.panim_ms,
            fabsf(G.pvx - G.px) > 0.01f || fabsf(G.pvy - G.py) > 0.01f,
            G.search_ms, camx, camy);
  if (G.search_ms > 0) {
    const char* txt = "搜索中";
    text(txt, G.pvx * 16 - camx + 8 - text_w(txt, 1.0f) * 0.5f,
         G.pvy * 16 - camy - 14, CLR_WHITE, 1.0f);
  }

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
  if (G.dlg_type == 2 || G.dlg_type == 3) {
    draw_sprite(A_hero[G.hero], 12, 15, 0, lx + 22, ly + 12,
                G.dlg_type == 2 ? 0xBBBBBBFF : 0);
  } else {
    uint8_t npc = G.dlg_type == 4 ? M_NPC_SMITH
                  : G.dlg_type == 5 ? M_NPC_IMP
                                    : M_NPC_GHOST;
    const MobDef& nd = mob_def(npc);
    draw_sprite(A_mobs[nd.asset], nd.fw, nd.fh, 0, lx + 20, ly + 12);
  }
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
static const float SLOT_X = 52, SLOT_Y = 34, SLOT_SIZE = 27;

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
  textf(196, 3, CLR_WHITE, 1.0f, "%s %d", REGIONS[G.region()].name, G.floor);
  draw_item_icon(IT_KEY, 196, 15, 0.8f);
  textf(214, 17, CLR_WHITE, 1.0f, "x%d", G.keys);
  textf(252, 17, CLR_YELLOW, 1.0f, "%d 金", G.gold);
}

static void draw_quickslots() {
  // SPD toolbar.t3x layouts: start (86,0,20,24), middle (88,0,18,24),
  // end (106,0,19,24). Repeated 32x32 cells look broken, so build the strip.
  for (int i = 0; i < 8; i++) {
    float x = SLOT_X + i * SLOT_SIZE;
    int sx = 88, sw = 18;
    if (i == 0) { sx = 86; sw = 20; }
    else if (i == 7) { sx = 106; sw = 19; }
    draw_rect_src(A_toolbar, sx, 0, sw, 24, x, SLOT_Y, SLOT_SIZE, 36, 0, 0.4f);
    if (i < G.inv_n) {
      const ItemDef& d = item_def(G.inv[i].id);
      draw_item_icon(G.inv[i].id, x + 6, SLOT_Y + 8, 1.0f);
      int num = d.kind == K_WAND ? G.inv[i].charges : G.inv[i].count;
      if (d.kind == K_WAND || num > 1)
        textf(x + 17, SLOT_Y + 21, CLR_WHITE, 1.0f, "%d", num);
    }
    if (G.targeting && G.target_item == i)
      draw_cursor(x, SLOT_Y, SLOT_SIZE, 36, CLR_YELLOW);
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
    draw_box(6, 76, BOT_W - 12, 22, C2D_Color32(0, 0, 0, 160), 0.38f);
    text_wrap(G.msg, 12, 80, BOT_W - 24, CLR_WHITE, 1.0f, 0.5f);
  }
  text_wrap("摇杆/十字键移动光标，A 走向目标，Y 交互。", 10, 108, BOT_W - 20,
            CLR_GREY, 1.0f, 1.0f);
  text_wrap("快捷栏：点击道具使用；法杖/炸弹可进入瞄准模式。", 10, 138, BOT_W - 20,
            CLR_GREY, 1.0f, 1.0f);
  textf(10, 168, C2D_Color32(198, 198, 206, 255), 1.0f, "回合 %d", G.turns);
}

static const float BAG_X = 66, BAG_Y = 84, BAG_W = 36, BAG_H = 24;
static const float BAG_GX = 38, BAG_GY = 25;

static const char* BAG_TABS[6] = {"全部", "药水", "卷轴", "法杖", "装备", "其他"};

static int bag_visible(int out[BAG_CAP]) {
  int n = 0;
  for (int i = 0; i < G.inv_n && i < BAG_CAP; i++) {
    if (g_bag_tab == 0 || item_def(G.inv[i].id).cat == g_bag_tab)
      out[n++] = i;
  }
  return n;
}

static int bag_sel_inv() {
  int out[BAG_CAP];
  int n = bag_visible(out);
  return (g_bag_sel >= 0 && g_bag_sel < n) ? out[g_bag_sel] : -1;
}

static void bag_clamp_sel() {
  int out[BAG_CAP];
  int n = bag_visible(out);
  if (g_bag_sel >= n) g_bag_sel = n > 0 ? n - 1 : 0;
}

static void draw_bag_tab(int i, float x, float y) {
  bool sel = g_bag_tab == i;
  draw_nine(A_chrome, 38, 6, 6, 6, 2, 2, 2, 2, x, y, 49, 22, 1.4f);
  if (sel) {
    draw_box(x + 2, y + 2, 45, 18, C2D_Color32(255, 226, 120, 56), 0.52f);
    draw_box(x + 2, y + 20, 45, 2, CLR_YELLOW, 0.55f);
  }
  if (i == 0) draw_icon(0, 48, 16, 16, x + 3, y + 3, 1.0f);
  else if (i == 1) draw_item_icon(IT_POTION_HEAL, x + 3, y + 3, 1.0f);
  else if (i == 2) draw_item_icon(IT_SCROLL_MAP, x + 3, y + 3, 1.0f);
  else if (i == 3) draw_item_icon(IT_WAND_MAGIC, x + 3, y + 3, 1.0f);
  else if (i == 4) draw_item_icon(IT_LEATHER_ARMOR, x + 3, y + 3, 1.0f);
  else draw_item_icon(IT_BOMB, x + 3, y + 3, 1.0f);
  text(BAG_TABS[i], x + 21, y + 5, sel ? CLR_YELLOW : CLR_WHITE, 1.0f);
}

static void draw_bag_action() {
  int idx = bag_sel_inv();
  if (idx < 0) return;
  const ItemDef& d = item_def(G.inv[idx].id);
  draw_box(0, 0, BOT_W, BOT_H, C2D_Color32(0, 0, 0, 170), 0.7f);
  draw_panel(16, 40, 288, 176, 2.0f);
  draw_item_icon(G.inv[idx].id, 30, 52, 2.0f);
  text(d.name, 72, 50, CLR_YELLOW, 2.0f);
  char extra[40] = "";
  if (d.kind == K_WEAPON || d.kind == K_ARMOR)
    snprintf(extra, sizeof(extra), "强化 +%d", G.inv[idx].level);
  else if (d.kind == K_WAND)
    snprintf(extra, sizeof(extra), "充能 %d", G.inv[idx].charges);
  else if (G.inv[idx].count > 1)
    snprintf(extra, sizeof(extra), "数量 %d", G.inv[idx].count);
  if (extra[0]) text(extra, 72, 78, CLR_GREY, 1.0f);
  text_wrap(d.desc, 30, 102, 260, CLR_WHITE, 1.0f, 2.0f);

  const char* primary = "使用 (A)";
  if (d.kind == K_WEAPON || d.kind == K_ARMOR || d.kind == K_RING)
    primary = "装备 (A)";
  else if (d.kind == K_WAND || d.kind == K_BOMB) primary = "瞄准 (A)";
  else if (d.kind == K_QUEST) primary = "任务物品";

  draw_button(30, 140, 120, 30, primary, false, 1.8f);
  draw_button(170, 140, 120, 30, "丢弃 (X)", false, 1.8f);
  draw_button(100, 176, 120, 26, "关闭 (B)", false, 1.8f);
}

static void draw_backpack() {
  draw_box(0, 0, BOT_W, BOT_H, C2D_Color32(0, 0, 0, 210), 0.66f);
  draw_panel(4, 4, BOT_W - 8, BOT_H - 8, 2.0f);
  text("背包", 14, 8, CLR_YELLOW, 2.0f);
  draw_item_icon(IT_GOLD, 210, 12, 1.0f);
  textf(230, 14, CLR_YELLOW, 1.0f, "%d", G.gold);
  textf(260, 14, CLR_GREY, 1.0f, "%d/%d", G.inv_n, BAG_CAP);

  textf(14, 32, CLR_GREY, 1.0f, "武器 %s", item_name(G.weapon));
  if (G.weapon_level) textf(112, 32, CLR_YELLOW, 1.0f, "+%d", G.weapon_level);
  textf(150, 32, CLR_GREY, 1.0f, "护甲 %s", item_name(G.armor));
  if (G.armor_level) textf(250, 32, CLR_YELLOW, 1.0f, "+%d", G.armor_level);

  textf(14, 44, CLR_GREY, 1.0f, "戒指 %s", item_name(G.ring1));
  if (G.ring1_level) textf(108, 44, CLR_YELLOW, 1.0f, "+%d", G.ring1_level);
  textf(164, 44, CLR_GREY, 1.0f, " / %s", item_name(G.ring2));
  if (G.ring2_level) textf(262, 44, CLR_YELLOW, 1.0f, "+%d", G.ring2_level);

  for (int i = 0; i < 6; i++) draw_bag_tab(i, 10 + i * 50, 58);

  int vis[BAG_CAP];
  int n = bag_visible(vis);
  for (int i = 0; i < BAG_CAP; i++) {
    float x = BAG_X + (i % 5) * BAG_GX;
    float y = BAG_Y + (i / 5) * BAG_GY;
    draw_nine(A_chrome, 38, 6, 6, 6, 2, 2, 2, 2, x, y, BAG_W, BAG_H, 1.4f);
    if (i < n) {
      int idx = vis[i];
      const ItemDef& d = item_def(G.inv[idx].id);
      draw_item_icon(G.inv[idx].id, x + 8, y + 2, 1.3f);
      int num = d.kind == K_WAND ? G.inv[idx].charges : G.inv[idx].count;
      if (d.kind == K_WAND || num > 1)
        textf(x + BAG_W - 10, y + BAG_H - 12, CLR_WHITE, 1.0f, "%d", num);
      if (d.kind == K_WAND || d.kind == K_WEAPON || d.kind == K_ARMOR)
        draw_box(x + 2, y + 2, 3, 3, CLR_YELLOW, 0.55f);
    }
    if (i == g_bag_sel) draw_cursor(x, y, BAG_W, BAG_H, CLR_YELLOW, 0.6f, false);
  }
  text_c("L/R 分栏   A 详情/使用   X 丢弃   B 关闭", 0, BOT_W, 212, CLR_GREY, 1.0f);
  if (g_bag_action) draw_bag_action();
}

static void draw_shop() {
  draw_box(0, 0, BOT_W, BOT_H, C2D_Color32(0, 0, 0, 220), 0.66f);
  draw_panel(4, 4, BOT_W - 8, BOT_H - 8, 2.0f);
  text("商店", 14, 8, CLR_YELLOW, 2.0f);
  draw_item_icon(IT_GOLD, 218, 14, 1.0f);
  textf(238, 16, CLR_YELLOW, 1.0f, "%d", G.gold);
  for (int i = 0; i < G.shop_n; i++) {
    float y = 34 + i * 26;
    draw_button(12, y, BOT_W - 24, 24, "", i == G.shop_sel, 1.6f);
    draw_item_icon(G.shop_items[i].id, 18, y + 4, 1.0f);
    text(item_name(G.shop_items[i].id), 40, y + 6, CLR_WHITE, 1.0f);
    if (G.shop_items[i].count > 1) {
      char cnt[16];
      snprintf(cnt, sizeof(cnt), "x%d", G.shop_items[i].count);
      text(cnt, 130, y + 6, CLR_GREY, 1.0f);
    }
    char price[24];
    if (G.shop_sold[i]) snprintf(price, sizeof(price), "已售");
    else snprintf(price, sizeof(price), "%d 金", G.shop_prices[i]);
    text(price, BOT_W - 24 - text_w(price, 1.0f), y + 6,
         G.shop_sold[i] ? CLR_GREY : CLR_YELLOW, 1.0f);
  }
  text_c("A 购买   B 关闭", 0, BOT_W, 220, CLR_GREY, 1.0f);
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
  if (G.shop_open) draw_shop();
}

// ---------------------------------------------------------------------------
// Update
// ---------------------------------------------------------------------------
static bool in_rect(float x, float y, float rx, float ry, float rw, float rh) {
  return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

static void do_search() { G.search(); }

static void bag_primary_action() {
  int i = bag_sel_inv();
  if (i < 0) return;
  const ItemDef& d = item_def(G.inv[i].id);
  if (d.kind == K_QUEST) {
    G.message("这是任务物品，不能使用。");
  } else if (d.kind == K_WAND || d.kind == K_BOMB) {
    G.clear_path();
    G.use_item(i);
    G.backpack = false;
    g_bag_action = false;
  } else if (d.kind == K_WEAPON || d.kind == K_ARMOR) {
    G.equip_item(i);
    g_bag_action = false;
  } else if (d.kind == K_RING) {
    G.equip_ring(i);
    bag_clamp_sel();
    g_bag_action = false;
  } else {
    G.use_item(i);
    g_bag_action = false;
    bag_clamp_sel();
  }
}

static void bag_drop_action() {
  int i = bag_sel_inv();
  if (i < 0) return;
  if (G.drop_item(i)) {
    bag_clamp_sel();
    g_bag_action = false;
  }
}

static int s_wait_frames = 0;
static int s_wait_hp0 = 0;

static void activate_action(int i) {
  switch (i) {
    case 0: G.wait_turns(1); G.message("你等待了一回合。"); break;
    case 1: do_search(); break;
    case 2: G.clear_path(); G.backpack = !G.backpack; g_bag_sel = 0; g_bag_tab = 0; g_bag_action = false; break;
    case 3: G.clear_path(); G.paused = 1; break;
  }
}

void game_update() {
  static u64 last_tick = 0;
  u64 now_tick = osGetTime();
  int dt = last_tick ? (int)(now_tick - last_tick) : 16;
  last_tick = now_tick;
  G.tick(dt);
  if (G.msg_timer > 0) G.msg_timer--;
  if (G.demo_lock) return;

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
    if (g_bag_action) {
      int i = g_bag_sel;
      if (IN.pressed(KEY_B)) { g_bag_action = false; return; }
      if (IN.pressed(KEY_A)) { bag_primary_action(); return; }
      if (IN.pressed(KEY_X)) { bag_drop_action(); return; }
      if (IN.tap) {
        if (in_rect(IN.tap_x, IN.tap_y, 30, 140, 120, 30)) bag_primary_action();
        else if (in_rect(IN.tap_x, IN.tap_y, 170, 140, 120, 30)) bag_drop_action();
        else if (in_rect(IN.tap_x, IN.tap_y, 100, 176, 120, 26)) g_bag_action = false;
      }
      return;
    }
    if (IN.pressed(KEY_B)) { G.backpack = false; return; }
    if (IN.pressed(KEY_L) || IN.pressed(KEY_R)) {
      g_bag_tab = (g_bag_tab + (IN.pressed(KEY_R) ? 1 : 5)) % 6;
      g_bag_sel = 0;
      g_bag_action = false;
      return;
    }
    if (IN.dx) g_bag_sel = std::max(0, std::min(BAG_CAP - 1, g_bag_sel + IN.dx));
    if (IN.dy) g_bag_sel = std::max(0, std::min(BAG_CAP - 1, g_bag_sel + IN.dy * 5));
    if (IN.pressed(KEY_A) && bag_sel_inv() >= 0) g_bag_action = true;
    if (IN.tap) {
      if (IN.tap_y >= 58 && IN.tap_y < 80 && IN.tap_x >= 10 && IN.tap_x < 310) {
        int tab = (int)((IN.tap_x - 10) / 50);
        if (tab >= 0 && tab < 6) {
          g_bag_tab = tab;
          g_bag_sel = 0;
          g_bag_action = false;
        }
        return;
      }
      int vis[BAG_CAP];
      int n = bag_visible(vis);
      for (int i = 0; i < BAG_CAP; i++) {
        float x = BAG_X + (i % 5) * BAG_GX;
        float y = BAG_Y + (i / 5) * BAG_GY;
        if (in_rect(IN.tap_x, IN.tap_y, x, y, BAG_W, BAG_H)) {
          g_bag_sel = i;
          if (i < n) g_bag_action = true;
          return;
        }
      }
      if (in_rect(IN.tap_x, IN.tap_y, 4, 4, BOT_W - 8, BOT_H - 8)) return;
      G.backpack = false;
    }
    return;
  }

  if (IN.pressed(KEY_B)) {
    if (G.targeting) { G.targeting = false; G.message("取消瞄准。"); }
    else { G.clear_path(); leave_game_to_saves(true); }
    return;
  }

  // Cursor movement.
  if (IN.dx) G.cursor_x = std::max(0, std::min(MW - 1, G.cursor_x + IN.dx));
  if (IN.dy) G.cursor_y = std::max(0, std::min(MH - 1, G.cursor_y + IN.dy));

  if (IN.pressed(KEY_A)) G.act_at(G.cursor_x, G.cursor_y, false);
  if (IN.pressed(KEY_Y)) G.act_at(G.cursor_x, G.cursor_y, true);
  if (IN.pressed(KEY_X)) { G.clear_path(); G.backpack = true; g_bag_sel = 0; g_bag_tab = 0; g_bag_action = false; }
  if (IN.pressed(KEY_L)) { G.wait_turns(1); G.message("你等待了一回合。"); }
  if (IN.pressed(KEY_R)) do_search();

  // Touch: quickslots and action buttons.
  if (IN.tap) {
    if (IN.tap_y >= SLOT_Y && IN.tap_y < SLOT_Y + 36 &&
        IN.tap_x >= SLOT_X && IN.tap_x < SLOT_X + SLOT_SIZE * 8) {
      int i = (int)((IN.tap_x - SLOT_X) / SLOT_SIZE);
      if (i < G.inv_n && i >= 0) { G.clear_path(); G.use_item(i); }
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

