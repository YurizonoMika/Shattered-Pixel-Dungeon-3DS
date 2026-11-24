#pragma once

#define SPD3DS_VERSION "0.1.15alpha"

#include <3ds.h>
#include <citro2d.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "font_glyphs.h"

#define SPD_DATA_DIR "sdmc:/3ds/spd"

#define TOP_W 400
#define TOP_H 240
#define BOT_W 320
#define BOT_H 240

struct Img {
  C3D_Tex* tex = nullptr;
  const Tex3DS_SubTexture* base = nullptr;
  int w = 0, h = 0;
  bool ok() const { return tex != nullptr; }
};

// ---- palette (SPD-ish) ------------------------------------------------------
#define CLR_WHITE  C2D_Color32(0xFF, 0xFF, 0xFF, 0xFF)
#define CLR_BLACK  C2D_Color32(0x00, 0x00, 0x00, 0xFF)
#define CLR_YELLOW C2D_Color32(0xFF, 0xD2, 0x3C, 0xFF)
#define CLR_GREEN  C2D_Color32(0x3C, 0xE0, 0x6E, 0xFF)
#define CLR_RED    C2D_Color32(0xE0, 0x40, 0x40, 0xFF)
#define CLR_GREY   C2D_Color32(0xD2, 0xD2, 0xD6, 0xFF)
#define CLR_DARK   C2D_Color32(0x12, 0x14, 0x1A, 0xFF)
#define CLR_BLUE   C2D_Color32(0x54, 0x8C, 0xF0, 0xFF)
#define CLR_ORANGE C2D_Color32(0xFF, 0x9C, 0x30, 0xFF)

// ---- assets ----------------------------------------------------------------
extern Img A_banners;    // 512x256  title logo & banners
extern Img A_chrome;     // 128x64   nine-patch UI chrome
extern Img A_toolbar;    // 256x32   8x 32px quickslots
extern Img A_status;     // 128x128  status pane
extern Img A_icons;      // 256x128  UI icons (16x16 grid)
extern Img A_heroicons;  // 128x256  hero icons (16x16 grid)
extern Img A_avatars;    // 256x32   hero portraits (24x32 frames)
extern Img A_items;      // 256x512  item sprites (16x16 grid)
extern Img A_itemicons;  // 128x64   item category icons (16x16 grid)
enum MobAsset : uint8_t {
  MA_RAT, MA_GNOLL, MA_CRAB, MA_SKELETON, MA_THIEF, MA_GUARD, MA_BAT, MA_BRUTE,
  MA_SHAMAN, MA_SPINNER, MA_GOLEM, MA_MONK, MA_WARLOCK, MA_SUCCUBUS, MA_EYE,
  MA_RIPPER, MA_WRAITH, MA_SNAKE, MA_SWARM, MA_SLIME, MA_DM100, MA_NECRO,
  MA_DM200, MA_GHOUL, MA_ELEMENTAL, MA_SCORPIO, MA_MIMIC,
  MA_GOO, MA_TENGU, MA_DM300, MA_KING, MA_YOG,
  MA_GHOST, MA_SHOP, MA_SMITH, MA_IMP, MA_WANDMAKER, MA_COUNT
};
extern Img A_tiles[5];   // region tilesets (256x256, 16x16 grid)
extern Img A_water[5];   // standalone repeatable 32x32 water textures
extern Img A_mobs[MA_COUNT];
extern Img A_hero[6];    // 256x128  hero walk sheets (16x16 grid)
extern Img A_art[6];     // 400x225  hero splash art
extern Img A_t_archs, A_t_back, A_t_front;
extern Img A_font;

bool assets_load();

// ---- drawing ---------------------------------------------------------------
void draw_img(const Img& im, float x, float y, float sx = 1, float sy = 1,
              u32 tint = 0, float z = 0.5f);
// Draw a cell from a grid sheet.
void draw_cell(const Img& im, int cx, int cy, int cw, int ch, float x, float y,
               float sx = 1, float sy = 1, u32 tint = 0, float z = 0.5f);
// Rotated cell, angle in radians, centered on the destination cell.
void draw_cell_rot(const Img& im, int cx, int cy, int cw, int ch, float x,
                   float y, float scale, float angle, u32 tint = 0,
                   float z = 0.5f);
// Draw a sub-rect of a sheet in source pixels (no grid assumption).
void draw_rect_src(const Img& im, int sx0, int sy0, int sw, int sh, float x,
                   float y, float dx, float dy, u32 tint = 0, float z = 0.5f);
// Nine-patch with pixel insets (left, top, right, bottom) scaled by `scale`.
void draw_nine(const Img& im, int sx, int sy, int sw, int sh, int l, int t, int r,
               int b, float x, float y, float w, float h, float scale = 1.0f,
               u32 tint = 0);

void draw_box(float x, float y, float w, float h, u32 color, float z = 0.4f);
void draw_frame(float x, float y, float w, float h, float thickness, u32 color,
                float z = 0.45f);

// Corner-bracket selection cursor (SPD style).
void draw_cursor(float x, float y, float w, float h, u32 color,
                 float z = 0.6f, bool center_dot = true);

// ---- text ------------------------------------------------------------------
void text(const char* s, float x, float y, u32 color, float scale = 1.0f);
void textf(float x, float y, u32 color, float scale, const char* fmt, ...);
float text_w(const char* s, float scale = 1.0f);
// Draw centered at x within width w.
void text_c(const char* s, float x, float w, float y, u32 color,
            float scale = 1.0f);
// Draw wrapped inside width w, returns height used.
float text_wrap(const char* s, float x, float y, float w, u32 color,
                float scale = 1.0f, float line_gap = 1.0f);

// ---- input -----------------------------------------------------------------
struct Input {
  u32 down = 0, held = 0, up = 0;
  int dx = 0, dy = 0;  // direction events this frame (repeat included)
  int pad_dx = 0, pad_dy = 0;
  bool tap = false;
  float tap_x = 0, tap_y = 0;
  bool touch = false;
  float tx = 0, ty = 0;

  bool pressed(u32 k) const { return (down & k) != 0; }
  bool holding(u32 k) const { return (held & k) != 0; }
};

void input_update(Input& in);

// ---- app state -------------------------------------------------------------
enum class Scene { TITLE, RANKINGS, SETTINGS, SAVES, HEROES, GAME };

struct Settings {
  bool music = true;
  bool sfx = true;
  int music_vol = 7;
  int sfx_vol = 8;
  bool shake = true;
  bool show_fps = false;
  bool quick_tap = false;
  int stick_speed = 2;  // 1..3
  bool touch_control = true;
  bool cursor_snap = true;
};

struct SlotMeta {
  bool used = false;
  uint8_t hero = 0;
  uint8_t depth = 1;
  uint8_t level = 1;
  int16_t hp = 0, maxhp = 0;
  int32_t score = 0;
  uint32_t seed = 0;
  uint32_t turns = 0;
  int32_t gold = 0;
  uint32_t stamp = 0;
};

struct ScoreEntry {
  uint8_t hero;
  uint8_t depth;
  int32_t score;
  uint32_t stamp;
};

void settings_load(Settings& s);
void settings_save(const Settings& s);
void slots_load(SlotMeta slots[3]);
void scores_load(std::vector<ScoreEntry>& out);
void scores_add(const ScoreEntry& e);

extern const char* HERO_NAMES[6];
extern const char* HERO_TITLES[6];
extern const char* HERO_DESC[6];
