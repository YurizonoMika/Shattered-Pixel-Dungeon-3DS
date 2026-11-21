#include "app.h"

#include <algorithm>

int g_bag_sel = 0;
int g_bag_tab = 0;
bool g_bag_action = false;

static bool wall_like(uint8_t t) {
  return t == T_WALL || t == T_SECRET;
}

static void tile_cell(uint8_t t, int& cx, int& cy) {
  // Indices from SPD's DungeonTileSheet (tiles_sewers.png is a 16x16 grid).
  int idx = 0;
  switch (t) {
    case T_WALL: idx = 80; break;      // RAISED_WALL (3D wall face)
    case T_FLOOR: idx = 0; break;      // FLOOR
    case T_DOOR: idx = 56; break;      // flat door (raised door drawn on top)
    case T_DOOR_LOCKED: idx = 56; break;
    case T_SECRET: idx = 80; break;    // looks like a wall
    case T_STAIRS: idx = 17; break;    // EXIT
    case T_WATER: idx = 32; break;     // WATER (stitched in draw_map)
    case T_GRASS: idx = 2; break;      // GRASS
    default: idx = 0; break;
  }
  cx = (idx % 16) * 16;
  cy = (idx / 16) * 16;
}

// Draw a 16x16 world tile at integer destination boundaries so scaled
// neighbours never leave seams or sample adjacent atlas cells.
static void draw_tile_world(const Img& im, int cx, int cy, float wx, float wy,
                            float z, int camx, int camy, u32 tint = 0) {
  int x0 = (int)roundf(wx * z - camx);
  int x1 = (int)roundf((wx + 16) * z - camx);
  int y0 = (int)roundf(wy * z - camy);
  int y1 = (int)roundf((wy + 16) * z - camy);
  draw_rect_src(im, cx, cy, 16, 16, x0, y0, x1 - x0, y1 - y0, tint);
}

static void draw_sprite(const Img& sheet, int fw, int fh, int frame, float x,
                        float y, u32 tint = 0, float sc = 1.0f) {
  int cols = sheet.w / fw;
  int cx = (frame % cols) * fw;
  int cy = (frame / cols) * fh;
  int x0 = (int)roundf(x), y0 = (int)roundf(y);
  int w = (int)roundf(fw * sc), h = (int)roundf(fh * sc);
  draw_rect_src(sheet, cx, cy, fw, fh, x0, y0, w, h, tint);
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
  float z = G.zoom;
  draw_sprite(A_hero[hero % 6], 12, 15, frame,
              vx * 16 * z - camx + 2 * z, vy * 16 * z - camy + 1 * z, 0, z);
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
  float z = G.zoom;
  float ox = (16 - d.fw) * 0.5f;
  float oy = 16.0f - d.fh;
  float bob = 0;
  if (m.type == M_NPC_GHOST) bob = sinf(TIME_S * 2.0f) * 1.5f;
  draw_sprite(sheet, d.fw, d.fh, frame, m.vx * 16 * z - camx + ox * z,
              m.vy * 16 * z - camy + (oy + bob) * z, d.tint, z);
}

static void draw_feature(const Feature& f, int camx, int camy) {
  if (!G.vis[f.y][f.x] && G.mind_t == 0) return;
  float z = G.zoom;
  float x = (float)(f.x * 16 * z - camx);
  float y = (float)(f.y * 16 * z - camy);
  float wx = f.x * 16.0f, wy = f.y * 16.0f;
  auto tile16 = [&](int idx, u32 tint = 0) {
    draw_tile_world(A_tiles[G.region()], (idx % 16) * 16, (idx / 16) * 16,
                    wx, wy, z, camx, camy, tint);
  };
  switch (f.type) {
    case F_WELL:
    case F_WELL_USED:
      tile16(f.type == F_WELL ? 18 : 19);
      break;
    case F_BOOKSHELF:
    case F_BOOKSHELF_USED:
      tile16(12 + 5 * 16, f.type == F_BOOKSHELF_USED ? 0x808080FF : 0);
      break;
    case F_ALCHEMY:
    case F_ALCHEMY_USED:
      tile16(8 + 7 * 16, f.type == F_ALCHEMY_USED ? 0x808080FF : 0);
      break;
    case F_STATUE:
    case F_STATUE_USED:
      tile16(8 * 16, f.type == F_STATUE_USED ? 0x808080FF : 0);
      break;
    case F_BARRICADE:
      tile16(9 + 7 * 16);
      break;
    case F_CHEST:
    case F_CHEST_LOCKED:
    case F_CHEST_MIMIC: {
      int spr = f.type == F_CHEST_LOCKED ? 37 : 36;
      draw_tile_world(A_items, (spr % 16) * 16, (spr / 16) * 16, wx, wy + 1,
                      z, camx, camy, 0);
      break;
    }
    case F_CHEST_OPEN:
      draw_tile_world(A_items, (36 % 16) * 16, (36 / 16) * 16, wx, wy + 1, z,
                      camx, camy, 0x808080FF);
      break;
    case F_TOMB:
    case F_TOMB_USED:
      draw_tile_world(A_items, (34 % 16) * 16, (34 / 16) * 16, wx, wy + 1, z,
                      camx, camy,
                      f.type == F_TOMB_USED ? 0x808080FF : 0);
      break;
    case F_PLANT:
      tile16(2, 0x80FFB0FF);
      break;
    case F_SIGN:
      draw_box(x + 6 * z, y + 4 * z, 4 * z, 9 * z,
               C2D_Color32(90, 60, 30, 255), 0.5f);
      draw_box(x + 2 * z, y + 3 * z, 12 * z, 6 * z,
               C2D_Color32(150, 105, 55, 255), 0.51f);
      draw_box(x + 4 * z, y + 5 * z, 8 * z, 1 * z,
               C2D_Color32(80, 50, 20, 255), 0.52f);
      break;
    case F_TRAP_FOUND:
      draw_icon(32, 32, 14, 14, x + 1 * z, y + 1 * z, z, CLR_RED);
      break;
    case F_TOKEN: {
      float bob = sinf(TIME_S * 3.0f + f.x) * 1.5f;
      draw_item_icon(IT_DWARF_TOKEN, x, y + bob * z, z);
      break;
    }
    default:
      break;
  }
}

// SPD FogOfWar: 0 visible, 1 visited, 2 mapped (scroll), 3 invisible.
static int fog_level(int x, int y) {
  if (x < 0 || y < 0 || x >= MW || y >= MH) return 3;
  if (G.vis[y][x]) return 0;
  if (G.seen[y][x] & 1) return 1;
  if (G.seen[y][x] & 2) return 2;
  return 3;
}

static u32 fog_color(int lvl) {
  switch (lvl) {
    case 0: return 0;
    case 1: return C2D_Color32(0x00, 0x00, 0x00, 0x99);
    case 2: return C2D_Color32(0x19, 0x33, 0x66, 0x99);
    default: return C2D_Color32(0x00, 0x00, 0x00, 0xFF);
  }
}

static void draw_fog(int camx, int camy) {
  float z = G.zoom;
  float cell = 16 * z;
  int tx0 = std::max(0, (int)(camx / cell) - 1);
  int ty0 = std::max(0, (int)(camy / cell) - 1);
  int tx1 = std::min(MW - 1, tx0 + (int)(TOP_W / cell) + 3);
  int ty1 = std::min(MH - 1, ty0 + (int)(TOP_H / cell) + 3);
  for (int y = ty0; y <= ty1; y++) {
    for (int x = tx0; x <= tx1; x++) {
      int x0 = (int)roundf(x * cell - camx);
      int x1 = (int)roundf((x + 1) * cell - camx);
      int y0 = (int)roundf(y * cell - camy);
      int y1 = (int)roundf((y + 1) * cell - camy);
      float fw = (float)(x1 - x0), fh = (float)(y1 - y0);
      if (!(G.seen[y][x] & 3)) {
        draw_box((float)x0, (float)y0, fw, fh, C2D_Color32(0, 0, 0, 255),
                 0.45f);
        continue;
      }
      uint8_t tile = G.tiles[y][x];
      float px = (float)x0, py = (float)y0;
      if (wall_like(tile) && y + 1 < MH && wall_like(G.tiles[y + 1][x])) {
        // Internal wall: fog the two halves separately, like SPD.
        int l = fog_level(x, y);
        if (x > 0) {
          if (wall_like(G.tiles[y][x - 1])) {
            l = std::max(l, fog_level(x - 1, y));
            if (y + 1 < MH && wall_like(G.tiles[y + 1][x - 1])) l = 3;
            else l = std::max(l, fog_level(x - 1, y + 1));
          } else {
            l = std::max(l, fog_level(x - 1, y));
          }
        } else l = 3;
        int rr = fog_level(x, y);
        if (x + 1 < MW) {
          if (wall_like(G.tiles[y][x + 1])) {
            rr = std::max(rr, fog_level(x + 1, y));
            if (y + 1 < MH && wall_like(G.tiles[y + 1][x + 1])) rr = 3;
            else rr = std::max(rr, fog_level(x + 1, y + 1));
          } else {
            rr = std::max(rr, fog_level(x + 1, y));
          }
        } else rr = 3;
        u32 cl = fog_color(l), cr = fog_color(rr);
        float half = (float)((x0 + x1) / 2 - x0);
        if (cl) draw_box(px, py, half, fh, cl, 0.45f);
        if (cr) draw_box(px + half, py, fw - half, fh, cr, 0.45f);
      } else {
        int l = fog_level(x, y);
        if (wall_like(tile) && !(y + 1 < MH && wall_like(G.tiles[y + 1][x])))
          l = std::max(l, fog_level(x, y + 1));
        u32 c = fog_color(l);
        if (c) draw_box(px, py, fw, fh, c, 0.45f);
      }
    }
  }
}

static void draw_map() {
  float z = G.zoom;
  float cell = 16 * z;
  int camx = (int)roundf((G.px * 16 + 8) * z - TOP_W / 2);
  int camy = (int)roundf((G.py * 16 + 8) * z - TOP_H / 2);
  int cx = (int)((G.cursor_x * 16 + 8) * z), cy = (int)((G.cursor_y * 16 + 8) * z);
  if (cx < camx + 44) camx = cx - 44;
  if (cx > camx + TOP_W - 44) camx = cx - (TOP_W - 44);
  if (cy < camy + 44) camy = cy - 44;
  if (cy > camy + TOP_H - 44) camy = cy - (TOP_H - 44);
  camx = std::max(0, std::min(camx, (int)(MW * cell) - TOP_W));
  camy = std::max(0, std::min(camy, (int)(MH * cell) - TOP_H));
  G.camx = camx;
  G.camy = camy;

  draw_box(0, 0, TOP_W, TOP_H, C2D_Color32(6, 8, 12, 255), 0.2f);

  int tx0 = std::max(0, (int)(camx / cell) - 1);
  int ty0 = std::max(0, (int)(camy / cell) - 1);
  int tx1 = std::min(MW - 1, tx0 + (int)(TOP_W / cell) + 3);
  int ty1 = std::min(MH - 1, ty0 + (int)(TOP_H / cell) + 3);
  for (int y = ty0; y <= ty1; y++) {
    for (int x = tx0; x <= tx1; x++) {
      if (!(G.seen[y][x] & 3)) continue;
      int scx, scy;
      uint8_t tt = G.tiles[y][x];
      tile_cell(tt, scx, scy);
      u32 tint = 0;
      float tx = (float)(x * cell - camx), ty = (float)(y * cell - camy);
      bool is_door = (tt == T_DOOR || tt == T_DOOR_OPEN ||
                      tt == T_DOOR_LOCKED);
      if (is_door) {
        // Doors are drawn below with the correct orientation.
        continue;
      } else if (wall_like(tt)) {
        // SPD: raised front face only when there is no wall below.
        bool below_wall = (y + 1 < MH && wall_like(G.tiles[y + 1][x]));
        if (!below_wall) {
          uint8_t below_tile = (y + 1 < MH) ? G.tiles[y + 1][x] : T_WALL;
          bool below_door = (below_tile == T_DOOR ||
                             below_tile == T_DOOR_OPEN ||
                             below_tile == T_DOOR_LOCKED);
          int idx = below_door ? 88 : 80;  // RAISED_WALL_DOOR / RAISED_WALL
          if (x + 1 >= MW || !wall_like(G.tiles[y][x + 1])) idx += 1;
          if (x <= 0 || !wall_like(G.tiles[y][x - 1])) idx += 2;
          draw_tile_world(A_tiles[G.region()], (idx % 16) * 16,
                          (idx / 16) * 16, x * 16.0f, y * 16.0f, z, camx, camy,
                          tint);
        }
      } else if (tt == T_WATER) {
        // +1 top land, +2 right, +4 bottom, +8 left (SPD stitchWaterTile).
        int idx = 32;
        if (y <= 0 || G.tiles[y - 1][x] != T_WATER) idx += 1;
        if (x + 1 >= MW || G.tiles[y][x + 1] != T_WATER) idx += 2;
        if (y + 1 >= MH || G.tiles[y + 1][x] != T_WATER) idx += 4;
        if (x <= 0 || G.tiles[y][x - 1] != T_WATER) idx += 8;
        draw_tile_world(A_tiles[G.region()], (idx % 16) * 16,
                        (idx / 16) * 16, x * 16.0f, y * 16.0f, z, camx, camy,
                        tint);
      } else {
        int idx = 0;
        if (tt == T_GRASS) idx = ((x * 7 + y * 13) % 19 == 0) ? 8 : 2;
        else if (tt == T_FLOOR && (x * 31 + y * 17) % 23 == 0) idx = 6;
        draw_tile_world(A_tiles[G.region()], (idx % 16) * 16,
                        (idx / 16) * 16, x * 16.0f, y * 16.0f, z, camx, camy,
                        tint);
      }
    }
  }

  // Doors: vertical panels in horizontal walls, horizontal panels in vertical walls.
  for (int y = ty0; y <= ty1; y++) {
    for (int x = tx0; x <= tx1; x++) {
      if (!(G.seen[y][x] & 3)) continue;
      uint8_t tt = G.tiles[y][x];
      if (tt != T_DOOR && tt != T_DOOR_OPEN && tt != T_DOOR_LOCKED)
        continue;
      uint32_t tint = 0;
      float tx = (float)(x * cell - camx), ty = (float)(y * cell - camy);
      bool above_wall = (y > 0 && wall_like(G.tiles[y - 1][x]));
      if (above_wall) {
        // The engine's wall layer is drawn without SPD's 8px 2.5D offset, so
        // place the sideways door panel directly in the door opening.
        draw_tile_world(A_tiles[G.region()], (116 % 16) * 16,
                        (116 / 16) * 16, x * 16.0f, y * 16.0f, z, camx, camy,
                        tint);
        if (tt == T_DOOR || tt == T_DOOR_LOCKED) {
          int di = tt == T_DOOR_LOCKED ? 228 : 227;
          draw_tile_world(A_tiles[G.region()], (di % 16) * 16,
                          (di / 16) * 16, x * 16.0f, y * 16.0f, z, camx, camy,
                          tint);
        }
      } else {
        int idx = tt == T_DOOR_OPEN ? 113 : (tt == T_DOOR_LOCKED ? 114 : 112);
        draw_tile_world(A_tiles[G.region()], (idx % 16) * 16,
                        (idx / 16) * 16, x * 16.0f, y * 16.0f, z, camx, camy,
                        tint);
      }
    }
  }

  // Search effect: SPD's CheckedCell (blue square shrinking after a delay).
  for (const SearchFx& f : G.search_fx) {
    if (f.delay > 0 || f.alpha <= 0) continue;
    float s = 16.0f * z * f.alpha;
    int a = (int)(f.alpha * 255);
    draw_box(f.x * 16 * z - camx + 8 * z - s * 0.5f,
             f.y * 16 * z - camy + 8 * z - s * 0.5f,
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
    float ix = roundf((float)(it.x * 16 * z - camx));
    float iy = roundf((float)(it.y * 16 * z - camy));
    draw_item_icon(it.id, ix, iy, z);
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
    draw_box((float)(G.fx_x * 16 * z - camx + 6 * z),
             (float)(G.fx_y * 16 * z - camy + 6 * z), 4 * z, 4 * z,
             CLR_YELLOW, 0.7f);
  }

  // player (hero frames are 12x15 inside the sheet)
  draw_hero(G.hero, G.pvx, G.pvy, G.panim_ms,
            fabsf(G.pvx - G.px) > 0.01f || fabsf(G.pvy - G.py) > 0.01f,
            G.search_ms, camx, camy);
  if (G.search_ms > 0) {
    const char* txt = "搜索中";
    text(txt, G.pvx * 16 * z - camx + 8 * z - text_w(txt, 1.0f) * 0.5f,
         G.pvy * 16 * z - camy - 14 * z, CLR_WHITE, 1.0f);
  }

  // SPD upper wall layers: internal wall tops (144..159) and overhangs (192..207).
  for (int y = ty0; y <= ty1; y++) {
    for (int x = tx0; x <= tx1; x++) {
      if (!(G.seen[y][x] & 3)) continue;
      uint8_t tt = G.tiles[y][x];
      uint8_t below = (y + 1 < MH) ? G.tiles[y + 1][x] : T_WALL;
      uint32_t tint = 0;
      float tx = (float)(x * cell - camx), ty = (float)(y * cell - camy);
      if (wall_like(tt) && wall_like(below)) {
        // Wall top surface, stitched with neighbours (SPD stitchInternalWallTile).
        int right = (x + 1 < MW && wall_like(G.tiles[y][x + 1])) ? 1 : 0;
        int rb = (x + 1 < MW && y + 1 < MH && wall_like(G.tiles[y + 1][x + 1])) ? 1 : 0;
        int lb = (x > 0 && y + 1 < MH && wall_like(G.tiles[y + 1][x - 1])) ? 1 : 0;
        int left = (x > 0 && wall_like(G.tiles[y][x - 1])) ? 1 : 0;
        int idx = 144 + (right ? 0 : 1) + (rb ? 0 : 2) + (lb ? 0 : 4) +
                  (left ? 0 : 8);
        draw_tile_world(A_tiles[G.region()], (idx % 16) * 16,
                        (idx / 16) * 16, x * 16.0f, y * 16.0f, z, camx, camy,
                        tint);
      } else if (!wall_like(tt) && wall_like(below)) {
        // Shadow/edge over the tile directly above a wall (SPD overhang layer).
        int rb = (x + 1 < MW && y + 1 < MH && wall_like(G.tiles[y + 1][x + 1])) ? 1 : 0;
        int lb = (x > 0 && y + 1 < MH && wall_like(G.tiles[y + 1][x - 1])) ? 1 : 0;
        int idx = 192 + (rb ? 0 : 1) + (lb ? 0 : 2);
        // Full wall-overhang material: brick lip + contact shadow, drawn on
        // the floor cell above a wall (this is what makes the bottom wall
        // visible from inside the room).
        draw_tile_world(A_tiles[G.region()], (idx % 16) * 16,
                        (idx / 16) * 16, x * 16.0f, y * 16.0f, z, camx, camy,
                        tint);
      } else if (!wall_like(tt) && (below == T_DOOR || below == T_DOOR_OPEN ||
                                    below == T_DOOR_LOCKED)) {
        int idx = below == T_DOOR_OPEN ? 225 : 224;
        draw_tile_world(A_tiles[G.region()], (idx % 16) * 16,
                        (idx / 16) * 16, x * 16.0f, y * 16.0f, z, camx, camy,
                        tint);
      }
    }
  }


  draw_fog(camx, camy);

  // cursor
  u32 cc = G.targeting ? CLR_YELLOW : CLR_GREEN;
  draw_cursor(roundf((float)(G.cursor_x * 16 * z - camx)),
              roundf((float)(G.cursor_y * 16 * z - camy)), roundf(16 * z),
              roundf(16 * z), cc);

  if (G.targeting) {
    bool wand = G.target_item >= 0 && G.target_item < G.inv_n &&
                item_def(G.inv[G.target_item].id).kind == K_WAND;
    text(wand ? "选择目标: A 施法   B 取消" : "选择目标: A 投掷   B 取消",
         10, 8, CLR_YELLOW, 1.0f);
  }

  // bottom hint bar
  draw_box(0, TOP_H - 22, TOP_W, 22, C2D_Color32(0, 0, 0, 160), 0.55f);
  text("B 菜单", 8, TOP_H - 17, CLR_GREY, 1.0f);
  text("A 移动   Y 交互", TOP_W - 138, TOP_H - 17, CLR_GREY, 1.0f);
  if (S.show_fps) textf(TOP_W - 96, 4, CLR_GREY, 1.0f, "FPS %.0f", FPS);
}

static void draw_dialog() {
  draw_box(0, 0, TOP_W, TOP_H, C2D_Color32(0, 0, 0, 150), 0.6f);
  draw_panel(8, 8, TOP_W - 16, TOP_H - 36, 2.0f);

  // left 3/4: portrait + name + text
  float lx = 20, ly = 20;
  draw_button(lx, ly, 56, 56, "", false, 2.0f);
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
    float by = 20 + i * 44;
    draw_button(rx, by, 88, 40, "", false, 2.0f);
    draw_box(rx + 6, by + 10, 20, 20, 0x33000000 | (kcol[i] & 0xFFFFFF), 0.56f);
    text_c(keys[i], rx + 6, 20, by + 12, CLR_BLACK, 1.0f);
    text_wrap(G.dlg_opt[i], rx + 30, by + 7, 54, CLR_WHITE, 1.0f, 0.5f);
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
  draw_item_icon(IT_KEY, 196, 15, 1.0f);
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
    draw_button(x, y, w, h, "", false, 2.0f);
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

static const float BAG_X = 17, BAG_Y = 92, BAG_W = 34, BAG_H = 21;
static const float BAG_GX = 36, BAG_GY = 22;

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
  draw_nine(A_chrome, 38, 6, 6, 6, 2, 2, 2, 2, x, y, 49, 22, 1.0f);
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
  draw_item_icon(G.inv[idx].id, 30, 54, 2.0f);
  text(d.name, 72, 54, CLR_YELLOW, 2.0f);
  char extra[40] = "";
  if (d.kind == K_WEAPON || d.kind == K_ARMOR)
    snprintf(extra, sizeof(extra), "强化 +%d", G.inv[idx].level);
  else if (d.kind == K_WAND)
    snprintf(extra, sizeof(extra), "充能 %d", G.inv[idx].charges);
  else if (G.inv[idx].count > 1)
    snprintf(extra, sizeof(extra), "数量 %d", G.inv[idx].count);
  if (extra[0]) text(extra, 72, 82, CLR_GREY, 1.0f);
  text_wrap(d.desc, 30, 104, 260, CLR_WHITE, 1.0f, 2.0f);

  const char* primary = "使用 (A)";
  if (d.kind == K_WEAPON || d.kind == K_ARMOR || d.kind == K_RING)
    primary = "装备 (A)";
  else if (d.kind == K_WAND || d.kind == K_BOMB) primary = "瞄准 (A)";
  else if (d.kind == K_QUEST) primary = "任务物品";

  draw_button(30, 140, 120, 30, primary, false, 2.0f);
  draw_button(170, 140, 120, 30, "丢弃 (X)", false, 2.0f);
  draw_button(100, 172, 120, 26, "关闭 (B)", false, 2.0f);
}

static void draw_backpack() {
  draw_box(0, 0, BOT_W, BOT_H, C2D_Color32(0, 0, 0, 210), 0.66f);
  draw_panel(2, 2, BOT_W - 4, BOT_H - 4, 2.0f);
  text("背包", 14, 14, CLR_YELLOW, 2.0f);
  draw_item_icon(IT_GOLD, 210, 16, 1.0f);
  textf(228, 18, CLR_YELLOW, 1.0f, "%d", G.gold);
  textf(252, 18, CLR_GREY, 1.0f, "%d/%d", G.inv_n, G.bag_capacity());

  textf(14, 44, CLR_GREY, 1.0f, "武器 %s", item_name(G.weapon));
  if (G.weapon_level) textf(112, 44, CLR_YELLOW, 1.0f, "+%d", G.weapon_level);
  textf(150, 44, CLR_GREY, 1.0f, "护甲 %s", item_name(G.armor));
  if (G.armor_level) textf(244, 44, CLR_YELLOW, 1.0f, "+%d", G.armor_level);

  textf(14, 58, CLR_GREY, 1.0f, "戒指一 %s", item_name(G.ring1));
  if (G.ring1_level) textf(100, 58, CLR_YELLOW, 1.0f, "+%d", G.ring1_level);
  textf(156, 58, CLR_GREY, 1.0f, "戒指二 %s", item_name(G.ring2));
  if (G.ring2_level) textf(244, 58, CLR_YELLOW, 1.0f, "+%d", G.ring2_level);

  for (int i = 0; i < 6; i++) draw_bag_tab(i, 10 + i * 50, 70);

  int vis[BAG_CAP];
  int n = bag_visible(vis);
  for (int i = 0; i < BAG_MAX; i++) {
    float x = BAG_X + (i % 8) * BAG_GX;
    float y = BAG_Y + (i / 8) * BAG_GY;
    draw_nine(A_chrome, 38, 6, 6, 6, 2, 2, 2, 2, x, y, BAG_W, BAG_H, 1.0f);
    if (i < n) {
      int idx = vis[i];
      const ItemDef& d = item_def(G.inv[idx].id);
      draw_item_icon(G.inv[idx].id, x + 9, y + 3, 1.0f);
      int num = d.kind == K_WAND ? G.inv[idx].charges : G.inv[idx].count;
      if (d.kind == K_WAND || num > 1)
        textf(x + BAG_W - 9, y + BAG_H - 12, CLR_WHITE, 1.0f, "%d", num);
      if (d.kind == K_WAND || d.kind == K_WEAPON || d.kind == K_ARMOR)
        draw_box(x + 2, y + 2, 3, 3, CLR_YELLOW, 0.55f);
    }
    if (i == g_bag_sel) draw_cursor(x, y, BAG_W, BAG_H, CLR_YELLOW, 0.6f, false);
  }
  text_c("L/R 分栏   A 详情/使用   X 丢弃   B 关闭", 0, BOT_W, 208, CLR_GREY, 1.0f);
  if (g_bag_action) draw_bag_action();
}

static int shop_list_size() {
  if (G.shop_mode == 0) return G.shop_n;
  if (G.shop_mode == 1) return G.inv_n;
  return G.buyback_n;
}

static Item shop_list_item(int i) {
  if (G.shop_mode == 1) return G.inv[i];
  if (G.shop_mode == 2) return G.buyback_items[i];
  return G.shop_items[i];
}

static int shop_list_price(int i) {
  if (G.shop_mode == 1) {
    Item it = G.inv[i];
    return G.shop_sell_price(it.id, it.count, it.level);
  }
  if (G.shop_mode == 2) return G.buyback_prices[i];
  return G.shop_prices[i];
}

static void draw_shop() {
  draw_box(0, 0, BOT_W, BOT_H, C2D_Color32(0, 0, 0, 220), 0.66f);
  draw_panel(4, 4, BOT_W - 8, BOT_H - 8, 2.0f);
  text("商店", 14, 16, CLR_YELLOW, 2.0f);
  draw_item_icon(IT_GOLD, 218, 18, 1.0f);
  textf(238, 20, CLR_YELLOW, 1.0f, "%d", G.gold);

  static const char* modes[3] = {"购买", "出售", "回购"};
  for (int i = 0; i < 3; i++) {
    float x = 10 + i * 102;
    bool sel = G.shop_mode == i;
    draw_button(x, 46, 98, 22, modes[i], false, 2.0f);
    if (sel) {
      draw_box(x + 2, 48, 94, 18, C2D_Color32(255, 226, 120, 60), 0.52f);
      draw_box(x + 2, 66, 94, 2, CLR_YELLOW, 0.55f);
    }
  }
  int size = shop_list_size();
  int pages = (size + 5) / 6;
  if (pages < 1) pages = 1;
  if (G.shop_page >= pages) G.shop_page = pages - 1;
  textf(264, 20, CLR_GREY, 1.0f, "%d/%d", G.shop_page + 1, pages);

  for (int row = 0; row < 6; row++) {
    int idx = G.shop_page * 6 + row;
    float y = 72 + row * 22;
    bool selected = idx == G.shop_page * 6 + G.shop_sel;
    draw_button(10, y, BOT_W - 20, 21, "", selected, 2.0f);
    if (idx >= size) continue;
    Item it = shop_list_item(idx);
    draw_item_icon(it.id, 16, y + 3, 1.0f);
    text(item_name(it.id), 38, y + 5, CLR_WHITE, 1.0f);
    if (it.count > 1) {
      char cnt[16];
      snprintf(cnt, sizeof(cnt), "x%d", it.count);
      text(cnt, 132, y + 5, CLR_GREY, 1.0f);
    }
    char price[24];
    if (G.shop_mode == 0 && G.shop_sold[idx]) snprintf(price, sizeof(price), "已售");
    else snprintf(price, sizeof(price), "%d 金", shop_list_price(idx));
    text(price, BOT_W - 22 - text_w(price, 1.0f), y + 5,
         (G.shop_mode == 0 && G.shop_sold[idx]) ? CLR_GREY : CLR_YELLOW, 1.0f);
  }
  if (size == 0) text_c("没有物品", 0, BOT_W, 120, CLR_GREY, 1.0f);
  text_c("X 切换  L/R 翻页  A 交易  B 关闭", 0, BOT_W, 208, CLR_GREY, 1.0f);
}

static int count_item(uint8_t id) {
  int n = 0;
  for (int i = 0; i < G.inv_n; i++)
    if (G.inv[i].id == id) n += G.inv[i].count;
  return n;
}

static void draw_alchemy() {
  draw_box(0, 0, BOT_W, BOT_H, C2D_Color32(0, 0, 0, 200), 0.66f);
  draw_panel(8, 40, BOT_W - 16, 176, 2.0f);
  text("炼金釜", 20, 46, CLR_YELLOW, 2.0f);
  static const uint8_t NEED[3] = {IT_SEED_SUNGRASS, IT_SEED_FIREBLOOM,
                                  IT_SEED_BLINDWEED};
  static const uint8_t OUT[3] = {IT_POTION_HEAL, IT_POTION_FLAME,
                                 IT_POTION_INVIS};
  for (int i = 0; i < 3; i++) {
    float y = 78 + i * 36;
    draw_button(18, y, BOT_W - 36, 32, "", G.alchemy_sel == i, 2.0f);
    draw_item_icon(OUT[i], 24, y + 8, 1.0f);
    text(item_name(OUT[i]), 46, y + 5, CLR_WHITE, 1.0f);
    char b[40];
    snprintf(b, sizeof(b), "%s %d/3", item_name(NEED[i]), count_item(NEED[i]));
    text(b, 46, y + 18, CLR_GREY, 1.0f);
  }
  text_c("A 炼金  B 关闭", 0, BOT_W, 188, CLR_GREY, 1.0f);
}

static int transmute_list(int out[]) {
  int n = 0;
  for (int i = 0; i < G.inv_n; i++)
    if (G.can_transmute(i)) out[n++] = i;
  return n;
}

static void draw_transmute() {
  draw_box(0, 0, BOT_W, BOT_H, C2D_Color32(0, 0, 0, 190), 0.66f);
  draw_panel(8, 38, BOT_W - 16, 178, 2.0f);
  text("嬗变：选择物品", 20, 50, CLR_YELLOW, 2.0f);
  int list[BAG_MAX];
  int n = transmute_list(list);
  int pages = (n + 4) / 5;
  if (pages < 1) pages = 1;
  if (G.transmute_page >= pages) G.transmute_page = pages - 1;
  for (int row = 0; row < 5; row++) {
    int pos = G.transmute_page * 5 + row;
    if (pos >= n) break;
    int idx = list[pos];
    float y = 82 + row * 20;
    draw_button(16, y, BOT_W - 32, 19, "", pos == G.transmute_sel, 1.0f);
    draw_item_icon(G.inv[idx].id, 22, y + 1, 1.0f);
    text(item_name(G.inv[idx].id), 42, y + 3, CLR_WHITE, 1.0f);
  }
  textf(BOT_W - 50, 54, CLR_GREY, 1.0f, "%d/%d", G.transmute_page + 1, pages);
  text_c("L/R 翻页  A 嬗变  B 取消", 0, BOT_W, 190, CLR_GREY, 1.0f);
}

static void draw_pause() {
  draw_box(0, 0, BOT_W, BOT_H, C2D_Color32(0, 0, 0, 170), 0.6f);
  draw_panel(44, 40, 232, 176, 2.0f);
  text_c("暂停", 44, 232, 46, CLR_YELLOW, 2.0f);
  draw_button(72, 78, 176, 30, "继续游戏", G.paused == 1, 2.0f);
  draw_button(72, 112, 176, 30, "设置", G.paused == 2, 2.0f);
  draw_button(72, 146, 176, 30, "返回主菜单", G.paused == 3, 2.0f);
  textf(72, 182, CLR_GREY, 1.0f, "天赋 %d/4", G.talent_tier);
  text_c("上下键 选择  A 确认  B 继续", 0, BOT_W, 222, CLR_GREY, 1.0f);
}

void game_draw_bottom() {
  static bool once = false;
  if (!once) { once = true; dlog("first game_draw_bottom"); }
  if (G.icon_test) {
    draw_box(0, 0, BOT_W, BOT_H, C2D_Color32(10, 12, 18, 255), 0.25f);
    for (int i = 1; i < IT_COUNT; i++)
      draw_item_icon((uint8_t)i, 4 + ((i - 1) % 18) * 17,
                     4 + ((i - 1) / 18) * 17, 1.0f);
    return;
  }
  draw_box(0, 0, BOT_W, BOT_H, C2D_Color32(10, 12, 18, 255), 0.25f);
  draw_status();
  draw_quickslots();
  draw_messages();
  if (!G.backpack && !G.paused) draw_action_bar();
  if (G.backpack) draw_backpack();
  if (G.paused) draw_pause();
  if (G.shop_open) draw_shop();
  if (G.alchemy_open) draw_alchemy();
  if (G.transmute_open) draw_transmute();
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
  if (dt < 1) dt = 1;
  if (dt > 100) dt = 100;
  // Shoulder zoom only while the map is the active UI.
  if (!G.backpack && !G.paused && !G.shop_open && !G.alchemy_open &&
      !G.transmute_open && !G.dialog) {
    float sdt = dt / 1000.0f;
    if (IN.holding(KEY_L)) G.zoom_target -= sdt * 1.25f;
    if (IN.holding(KEY_R)) G.zoom_target += sdt * 1.25f;
    G.zoom_target = std::max(0.75f, std::min(2.0f, G.zoom_target));
    G.zoom += (G.zoom_target - G.zoom) * std::min(1.0f, sdt * 14.0f);
  }
  last_tick = now_tick;
  G.tick(dt);
  if (G.msg_timer > 0) G.msg_timer--;
  if (G.demo_lock) return;

  if (G.shop_open) {
    int size = shop_list_size();
    int pages = (size + 5) / 6;
    if (pages < 1) pages = 1;
    if (IN.pressed(KEY_B)) { G.shop_open = false; return; }
    if (IN.pressed(KEY_X)) {
      G.shop_mode = (G.shop_mode + 1) % 3;
      G.shop_sel = 0; G.shop_page = 0;
      return;
    }
    if (IN.pressed(KEY_L) && G.shop_page > 0) { G.shop_page--; G.shop_sel = 0; }
    if (IN.pressed(KEY_R) && G.shop_page + 1 < pages) { G.shop_page++; G.shop_sel = 0; }
    if (IN.dy) {
      int maxsel = std::min(5, size - G.shop_page * 6 - 1);
      if (maxsel < 0) maxsel = 0;
      G.shop_sel = std::max(0, std::min(maxsel, G.shop_sel + IN.dy));
    }
    int idx = G.shop_page * 6 + G.shop_sel;
    if (IN.pressed(KEY_A)) {
      if (G.shop_mode == 0) { if (idx < G.shop_n) G.shop_buy(idx); }
      else if (G.shop_mode == 1) { if (idx < G.inv_n) G.shop_sell(idx); }
      else if (idx < G.buyback_n) G.shop_buyback(idx);
      return;
    }
    if (IN.tap) {
      for (int m = 0; m < 3; m++)
        if (in_rect(IN.tap_x, IN.tap_y, 10 + m * 102, 46, 98, 22)) {
          G.shop_mode = m; G.shop_sel = 0; G.shop_page = 0;
          return;
        }
      for (int row = 0; row < 6; row++) {
        float y = 72 + row * 22;
        if (in_rect(IN.tap_x, IN.tap_y, 10, y, BOT_W - 20, 22)) {
          int id2 = G.shop_page * 6 + row;
          if (id2 >= size) return;
          G.shop_sel = row;
          if (G.shop_mode == 0) G.shop_buy(id2);
          else if (G.shop_mode == 1) G.shop_sell(id2);
          else G.shop_buyback(id2);
          return;
        }
      }
      return;
    }
    return;
  }

  if (G.transmute_open) {
    int list[BAG_MAX];
    int n = transmute_list(list);
    if (IN.pressed(KEY_B)) { G.transmute_open = false; return; }
    int pages = (n + 4) / 5;
    if (pages < 1) pages = 1;
    if (IN.pressed(KEY_L) && G.transmute_page > 0) { G.transmute_page--; G.transmute_sel = G.transmute_page * 5; }
    if (IN.pressed(KEY_R) && G.transmute_page + 1 < pages) { G.transmute_page++; G.transmute_sel = G.transmute_page * 5; }
    if (IN.dy && n > 0) {
      G.transmute_sel = std::max(0, std::min(n - 1, G.transmute_sel + IN.dy));
      G.transmute_page = G.transmute_sel / 5;
    }
    if (IN.pressed(KEY_A) && n > 0)
      G.transmute_choose(list[G.transmute_sel]);
    if (IN.tap) {
      int list2[BAG_MAX];
      int n2 = transmute_list(list2);
      for (int row = 0; row < 5; row++) {
        int pos = G.transmute_page * 5 + row;
        if (pos >= n2) break;
        float y = 82 + row * 20;
        if (in_rect(IN.tap_x, IN.tap_y, 16, y, BOT_W - 32, 19)) {
          G.transmute_sel = pos;
          G.transmute_choose(list2[pos]);
          return;
        }
      }
      G.transmute_open = false;
    }
    return;
  }

  if (G.alchemy_open) {
    if (IN.pressed(KEY_B)) { G.alchemy_open = false; return; }
    if (IN.dy) G.alchemy_sel = (G.alchemy_sel + IN.dy + 3) % 3;
    if (IN.pressed(KEY_A)) { G.alchemy_brew(G.alchemy_sel); return; }
    if (IN.tap) {
      for (int i = 0; i < 3; i++)
        if (in_rect(IN.tap_x, IN.tap_y, 18, 78 + i * 36, BOT_W - 36, 32)) {
          G.alchemy_sel = i;
          G.alchemy_brew(i);
          return;
        }
      G.alchemy_open = false;
    }
    return;
  }

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
        else if (in_rect(IN.tap_x, IN.tap_y, 100, 172, 120, 26)) g_bag_action = false;
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
    if (IN.dy) g_bag_sel = std::max(0, std::min(BAG_MAX - 1, g_bag_sel + IN.dy * 8));
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
      for (int i = 0; i < BAG_MAX; i++) {
        float x = BAG_X + (i % 8) * BAG_GX;
        float y = BAG_Y + (i / 8) * BAG_GY;
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
    else { G.clear_path(); G.paused = 1; }
    return;
  }

  // Cursor movement.
  if (IN.dx) G.cursor_x = std::max(0, std::min(MW - 1, G.cursor_x + IN.dx));
  if (IN.dy) G.cursor_y = std::max(0, std::min(MH - 1, G.cursor_y + IN.dy));

  if (IN.pressed(KEY_A)) G.act_at(G.cursor_x, G.cursor_y, false);
  if (IN.pressed(KEY_Y)) G.act_at(G.cursor_x, G.cursor_y, true);
  if (IN.pressed(KEY_X)) { G.clear_path(); G.backpack = true; g_bag_sel = 0; g_bag_tab = 0; g_bag_action = false; }
  if (IN.pressed(KEY_ZL)) { G.wait_turns(1); G.message("你等待了一回合。"); }
  if (IN.pressed(KEY_ZR)) do_search();

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

