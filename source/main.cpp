#include "app.h"

#include <sys/stat.h>
#include <algorithm>
#include <stdarg.h>

Settings S;
SlotMeta SLOTS[3];
std::vector<ScoreEntry> SCORES;
Scene SCENE = Scene::TITLE;
Game G;
Input IN;
int ACTIVE_SLOT = -1;
float TIME_S = 0;
float FPS = 60;
Scene SETTINGS_BACK = Scene::TITLE;

const char* HERO_NAMES[6] = {"战士", "法师", "盗贼", "女猎手", "决斗家", "牧师"};
const char* HERO_TITLES[6] = {"钢铁意志", "奥术研究者", "暗影行者",
                              "荒野之眼", "剑舞者", "光明信徒"};
const char* HERO_DESC[6] = {
    "近战专家，生命值最高。\n擅长正面战斗，适合第一次进入地牢的玩家。",
    "使用法杖与法术，生命值较低。\n可以用炸弹和卷轴控制战场。",
    "身手敏捷，攻击力最高。\n依靠速度与走位避开危险。",
    "远程射手，命中后可以拉开距离。\n需要耐心与地形意识。",
    "连击型战士，攻击节奏快。\n适合喜欢贴身缠斗的玩家。",
    "光明的信徒，治疗能力最强。\n生存能力好，输出偏低。",
};

static C3D_RenderTarget* g_top = nullptr;
static C3D_RenderTarget* g_bot = nullptr;
static int g_menu_sel = 0;
static int g_set_tab = 0, g_set_row = 0;
static int g_save_sel = 0;
static int g_hero_sel = 0, g_hero_arm = -1;
static bool g_hero_info = false;
static int g_rank_scroll = 0;
static u64 g_last_ms = 0;

// ---------------------------------------------------------------------------
// Data files
// ---------------------------------------------------------------------------
#define DATA_DIR SPD_DATA_DIR

static void ensure_dir();

void dlog(const char* fmt, ...) {
  ensure_dir();
  FILE* f = fopen(DATA_DIR "/log.txt", "a");
  if (!f) return;
  va_list ap;
  va_start(ap, fmt);
  vfprintf(f, fmt, ap);
  va_end(ap);
  fputc(10, f);
  fclose(f);
}

static void ensure_dir() {
  static bool done = false;
  if (done) return;
  mkdir("sdmc:/3ds", 0777);
  mkdir(DATA_DIR, 0777);
  done = true;
}

void settings_load(Settings& s) {
  ensure_dir();
  FILE* f = fopen(DATA_DIR "/settings.bin", "rb");
  if (!f) return;
  fread(&s, sizeof(s), 1, f);
  fclose(f);
}
void settings_save(const Settings& s) {
  ensure_dir();
  FILE* f = fopen(DATA_DIR "/settings.bin", "wb");
  if (!f) return;
  fwrite(&s, sizeof(s), 1, f);
  fclose(f);
}
void slots_load(SlotMeta slots[3]) {
  ensure_dir();
  for (int i = 0; i < 3; i++) {
    SlotMeta m{};
    if (Game::slot_meta(i, m)) slots[i] = m;
    else slots[i] = SlotMeta{};
  }
}
void slot_save(int i, const SlotMeta& m) {
  ensure_dir();
  char path[64];
  snprintf(path, sizeof(path), DATA_DIR "/slot%d.bin", i);
  FILE* f = fopen(path, "wb");
  if (!f) return;
  fwrite(&m, sizeof(SlotMeta), 1, f);
  fclose(f);
}
void scores_load(std::vector<ScoreEntry>& out) {
  ensure_dir();
  out.clear();
  FILE* f = fopen(DATA_DIR "/scores.bin", "rb");
  if (!f) return;
  uint32_t n = 0;
  if (fread(&n, sizeof(n), 1, f) == 1 && n < 64) {
    for (uint32_t i = 0; i < n; i++) {
      ScoreEntry e{};
      if (fread(&e, sizeof(e), 1, f) != 1) break;
      out.push_back(e);
    }
  }
  fclose(f);
}
void scores_add(const ScoreEntry& e) {
  SCORES.push_back(e);
  std::stable_sort(SCORES.begin(), SCORES.end(),
                   [](const ScoreEntry& a, const ScoreEntry& b) {
                     return a.score > b.score;
                   });
  if (SCORES.size() > 32) SCORES.resize(32);
  ensure_dir();
  FILE* f = fopen(DATA_DIR "/scores.bin", "wb");
  if (!f) return;
  uint32_t n = (uint32_t)SCORES.size();
  fwrite(&n, sizeof(n), 1, f);
  fwrite(SCORES.data(), sizeof(ScoreEntry), n, f);
  fclose(f);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
static int s_last_dx = 0, s_last_dy = 0, s_rep = 0;

void input_update(Input& in) {
  in.down = hidKeysDown();
  in.held = hidKeysHeld();
  in.up = hidKeysUp();
  in.dx = in.dy = 0;

  int dx = ((in.held & KEY_RIGHT) ? 1 : 0) - ((in.held & KEY_LEFT) ? 1 : 0);
  int dy = ((in.held & KEY_DOWN) ? 1 : 0) - ((in.held & KEY_UP) ? 1 : 0);
  circlePosition cp;
  hidCircleRead(&cp);
  if (!dx) {
    if (cp.dx > 40) dx = 1;
    else if (cp.dx < -40) dx = -1;
  }
  if (!dy) {
    if (cp.dy > 40) dy = 1;
    else if (cp.dy < -40) dy = -1;
  }
  in.pad_dx = dx;
  in.pad_dy = dy;
  if (dx != s_last_dx || dy != s_last_dy) {
    in.dx = dx;
    in.dy = dy;
    s_rep = 0;
  } else if (dx || dy) {
    if (++s_rep >= 6) {
      s_rep = 0;
      in.dx = dx;
      in.dy = dy;
    }
  }
  s_last_dx = dx;
  s_last_dy = dy;

  touchPosition tp;
  hidTouchRead(&tp);
  in.touch = (in.held & KEY_TOUCH) != 0;
  static bool touching = false, moved = false;
  static float sx = 0, sy = 0;
  static float last_x = 0, last_y = 0;
  in.tap = false;
  if (in.touch) {
    // hidTouchRead can report (0,0) on the release frame, so keep the last
    // real position and use it for the tap.
    last_x = tp.px;
    last_y = tp.py;
    if (!touching) {
      touching = true;
      moved = false;
      sx = tp.px;
      sy = tp.py;
    } else if (fabsf(tp.px - sx) > 8 || fabsf(tp.py - sy) > 8) {
      moved = true;
    }
  } else if (touching) {
    touching = false;
    if (!moved) {
      in.tap = true;
      in.tap_x = last_x;
      in.tap_y = last_y;
    }
  }
  in.tx = last_x;
  in.ty = last_y;
}

// ---------------------------------------------------------------------------
// Shared drawing helpers
// ---------------------------------------------------------------------------
static bool in_rect(float x, float y, float rx, float ry, float rw, float rh) {
  return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

void draw_panel(float x, float y, float w, float h, float scale) {
  draw_nine(A_chrome, 0, 0, 20, 20, 6, 6, 6, 6, x, y, w, h, scale);
}

void draw_button(float x, float y, float w, float h, const char* label,
                 bool focused, float scale) {
  draw_nine(A_chrome, 38, 6, 6, 6, 2, 2, 2, 2, x, y, w, h, scale);
  if (focused) {
    draw_box(x + 2, y + 2, w - 4, h - 4, C2D_Color32(255, 226, 120, 46), 0.52f);
    draw_cursor(x, y, w, h, CLR_YELLOW, 0.6f, false);
  }
  float ts = 1.0f;
  text_c(label, x, w, y + (h - FONT_LINE_H * ts) * 0.5f,
         focused ? CLR_YELLOW : CLR_WHITE, ts);
}

void draw_icon(int sx, int sy, int sw, int sh, float x, float y, float scale,
               u32 tint) {
  draw_rect_src(A_icons, sx, sy, sw, sh, x, y, sw * scale, sh * scale, tint);
}

void draw_item_icon(uint8_t id, float x, float y, float scale) {
  int cx = 0, cy = 0;
  item_sprite_cell(id, cx, cy);
  draw_cell(A_items, cx, cy, 16, 16, x, y, scale, scale);
}

void draw_hero_icon(int hero, float x, float y, float scale) {
  draw_cell(A_heroicons, (hero % 8) * 16, (hero / 8) * 16, 16, 16, x, y, scale,
            scale);
}

void draw_hero_portrait(int hero, float x, float y, float scale) {
  draw_rect_src(A_avatars, (hero % 6) * 24, 0, 24, 32, x, y, 24 * scale,
                32 * scale);
}

void draw_top_hint(const char* s) {
  draw_box(0, TOP_H - 22, TOP_W, 22, C2D_Color32(0, 0, 0, 150), 0.55f);
  text_c(s, 0, TOP_W, TOP_H - 18, CLR_GREY, 1.0f);
}

void draw_bg(bool bottom) {
  float xo = bottom ? -40.0f : 0.0f;
  float yo = bottom ? -(float)TOP_H : 0.0f;
  // tiled stone floor so both screens read as one continuous scene
  for (int y = 0; y < TOP_H; y += 16)
    for (int x = 0; x < (bottom ? BOT_W : TOP_W); x += 16)
      draw_cell(A_tiles[0], 0, 0, 16, 16, (float)x, (float)y, 1, 1,
                0x2A2A30FF, 0.08f);
  for (int k = -1; k <= 1; k++)
    draw_img(A_t_back, xo - fmodf(TIME_S * 8, 512) + k * 512, yo, 1, 1, 0, 0.1f);
  for (int k = -1; k <= 2; k++)
    draw_img(A_t_archs, xo - fmodf(TIME_S * 15, 1024) + k * 1024, yo + 96, 1, 1,
             0, 0.12f);
  for (int k = -1; k <= 2; k++)
    draw_img(A_t_front, xo - fmodf(TIME_S * 26, 1024) + k * 1024, yo + 20, 1, 1,
             0, 0.14f);
  draw_box(0, 0, bottom ? BOT_W : TOP_W, TOP_H, C2D_Color32(6, 8, 14, 110), 0.2f);
}

static void draw_logo(float scale, float y) {
  // BannerSprites.TITLE_LAND = uvRect(0,100,240,157) -> a 240x57 logo.
  float w = 240 * scale, h = 57 * scale;
  draw_rect_src(A_banners, 0, 100, 240, 57, (TOP_W - w) / 2, y, w, h);
}

static void draw_slot(int i, float x, float y, float w, float h, bool focused) {
  draw_panel(x, y, w, h, 1.0f);
  if (i == 3) {
    draw_icon(96, 32, 11, 11, x + w / 2 - 16, y + 16, 3.0f, CLR_YELLOW);
    text_c("新游戏", x, w, y + 60, focused ? CLR_YELLOW : CLR_WHITE, 1.0f);
  } else if (!SLOTS[i].used) {
    text_c("空存档位", x, w, y + 34, CLR_GREY, 1.0f);
    text_c("按 A 开始新游戏", x, w, y + 56, C2D_Color32(196, 196, 204, 255), 1.0f);
  } else {
    const SlotMeta& m = SLOTS[i];
    draw_hero_portrait(m.hero, x + 8, y + 8, 1.0f);
    text(HERO_NAMES[m.hero % 6], x + 44, y + 8, CLR_WHITE, 1.0f);
    textf(x + 44, y + 24, CLR_GREY, 1.0f, "第 %d 层 · Lv%d", m.depth, m.level);
    // HP bar
    float bw = w - 20;
    draw_box(x + 10, y + 46, bw, 8, C2D_Color32(40, 10, 10, 255));
    float frac = m.maxhp > 0 ? (float)m.hp / m.maxhp : 0;
    if (frac < 0) frac = 0;
    draw_box(x + 10, y + 46, bw * frac, 8, C2D_Color32(200, 40, 40, 255));
    textf(x + 10, y + 60, CLR_GREY, 1.0f, "HP %d/%d", m.hp, m.maxhp);
    textf(x + w - 76, y + h - 20, CLR_YELLOW, 1.0f, "分数 %d", m.score);
  }
  if (focused) draw_cursor(x + 1, y + 1, w - 2, h - 2, CLR_YELLOW);
}

// ---------------------------------------------------------------------------
// Settings model
// ---------------------------------------------------------------------------
static int tab_rows(int tab) {
  static const int n[3] = {3, 4, 3};
  return n[tab % 3];
}
static const char* tab_name(int t) {
  static const char* n[3] = {"游戏", "音频", "画面"};
  return n[t % 3];
}
static const char* row_label(int tab, int row) {
  static const char* L[3][4] = {
      {"屏幕震动", "快速点击", "光标吸附", ""},
      {"音乐", "音乐音量", "音效", "音效音量"},
      {"显示 FPS", "摇杆速度", "触摸控制", ""},
  };
  return L[tab % 3][row % 4];
}
static void row_value(int tab, int row, char* out, size_t n) {
  tab %= 3;
  if (tab == 0) {
    bool v = row == 0 ? S.shake : (row == 1 ? S.quick_tap : S.cursor_snap);
    snprintf(out, n, "%s", v ? "开" : "关");
  } else if (tab == 1) {
    if (row == 0) snprintf(out, n, "%s", S.music ? "开" : "关");
    else if (row == 1) snprintf(out, n, "%d / 10", S.music_vol);
    else if (row == 2) snprintf(out, n, "%s", S.sfx ? "开" : "关");
    else snprintf(out, n, "%d / 10", S.sfx_vol);
  } else {
    if (row == 0) snprintf(out, n, "%s", S.show_fps ? "开" : "关");
    else if (row == 1) snprintf(out, n, "%d / 3", S.stick_speed);
    else snprintf(out, n, "%s", S.touch_control ? "开" : "关");
  }
}
static void row_activate(int tab, int row) {
  tab %= 3;
  if (tab == 0) {
    if (row == 0) S.shake = !S.shake;
    else if (row == 1) S.quick_tap = !S.quick_tap;
    else S.cursor_snap = !S.cursor_snap;
  } else if (tab == 1) {
    if (row == 0) S.music = !S.music;
    else if (row == 1) S.music_vol = (S.music_vol + 1) % 11;
    else if (row == 2) S.sfx = !S.sfx;
    else S.sfx_vol = (S.sfx_vol + 1) % 11;
  } else {
    if (row == 0) S.show_fps = !S.show_fps;
    else if (row == 1) S.stick_speed = (S.stick_speed % 3) + 1;
    else S.touch_control = !S.touch_control;
  }
  settings_save(S);
}

// ---------------------------------------------------------------------------
// Scene transitions
// ---------------------------------------------------------------------------
void go_scene(Scene s) {
  SCENE = s;
  if (s == Scene::TITLE) {
    g_menu_sel = 0;
  } else if (s == Scene::SETTINGS) {
    g_set_tab = 0;
    g_set_row = 0;
  } else if (s == Scene::SAVES) {
    g_save_sel = 0;
  } else if (s == Scene::HEROES) {
    g_hero_sel = 0;
    g_hero_arm = -1;
    g_hero_info = false;
  }
}

void start_new_game(int hero) {
  dlog("start_new_game %d", hero);
  G.new_run(hero, (uint32_t)(osGetTime() & 0xFFFFFFFF));
  ACTIVE_SLOT = -1;
  for (int i = 0; i < 3; i++) {
    if (!SLOTS[i].used) { ACTIVE_SLOT = i; break; }
  }
  if (ACTIVE_SLOT < 0) ACTIVE_SLOT = 0;
  G.save(ACTIVE_SLOT);
  SLOTS[ACTIVE_SLOT] = G.meta();
  dlog("start_new_game done floor=%d", G.floor);
  SCENE = Scene::GAME;
}

void load_slot(int i) {
  if (!SLOTS[i].used) return;
  if (!G.load(i)) {
    G.new_run(SLOTS[i].hero % 6, SLOTS[i].seed);
  }
  ACTIVE_SLOT = i;
  SCENE = Scene::GAME;
}

void leave_game_to_saves(bool save) {
  if (save && ACTIVE_SLOT >= 0) {
    G.save(ACTIVE_SLOT);
    SLOTS[ACTIVE_SLOT] = G.meta();
  }
  SCENE = Scene::SAVES;
  g_save_sel = 0;
}

void record_run_end(bool victory) {
  ScoreEntry e{};
  e.hero = (uint8_t)G.hero;
  e.depth = (uint8_t)G.floor;
  e.score = G.meta().score + (victory ? 500 : 0);
  e.stamp = (uint32_t)(osGetTime() / 1000);
  scores_add(e);
  if (ACTIVE_SLOT >= 0) {
    SlotMeta empty{};
    SLOTS[ACTIVE_SLOT] = empty;
    slot_save(ACTIVE_SLOT, empty);
  }
  ACTIVE_SLOT = -1;
}

// ---------------------------------------------------------------------------
// Title
// ---------------------------------------------------------------------------
static void title_update() {
  if (IN.dy) g_menu_sel = (g_menu_sel + IN.dy + 3) % 3;
  for (int i = 0; i < 3; i++) {
    float y = 24 + i * 50;
    if (IN.tap && in_rect(IN.tap_x, IN.tap_y, 48, y, 224, 42)) g_menu_sel = i;
  }
  bool activate = IN.pressed(KEY_A);
  if (IN.tap && in_rect(IN.tap_x, IN.tap_y, 48, 24 + g_menu_sel * 50, 224, 42))
    activate = true;
  if (activate) {
    if (g_menu_sel == 0) go_scene(Scene::SAVES);
    else if (g_menu_sel == 1) go_scene(Scene::RANKINGS);
    else { SETTINGS_BACK = Scene::TITLE; go_scene(Scene::SETTINGS); }
  }
}

static void title_draw_top() {
  draw_bg(false);
  draw_logo(1.0f, 44);
  text_c("Shattered Pixel Dungeon - 3DS", 0, TOP_W, 118,
         C2D_Color32(255, 210, 120, 255), 2.0f);
  textf(TOP_W - 86, TOP_H - 32, CLR_GREY, 1.0f, "v%s", SPD3DS_VERSION);
  draw_top_hint("十字键 / 摇杆 选择    A 确认");
}

static void title_draw_bottom() {
  draw_bg(true);
  for (int i = 0; i < 3; i++)
    draw_button(48, 24 + i * 50, 224, 42, (i == 0 ? "进入地牢" : (i == 1 ? "排行榜" : "设置")), g_menu_sel == i, 2.0f);
  text_c("上下键 选择   A 确认", 0, BOT_W, 198, CLR_GREY, 1.0f);
}

// ---------------------------------------------------------------------------
// Rankings
// ---------------------------------------------------------------------------
static void rankings_update() {
  if (IN.pressed(KEY_B)) go_scene(Scene::TITLE);
  if (IN.dy) g_rank_scroll = std::max(0, g_rank_scroll + IN.dy);
}
static void rankings_draw_top() {
  draw_bg(false);
  draw_logo(1.0f, 10);
  text_c("本地排行榜", 0, TOP_W, 168, CLR_YELLOW, 2.0f);
  text_c("按 B 返回主菜单", 0, TOP_W, TOP_H - 20, CLR_GREY, 1.0f);
}
static void rankings_draw_bottom() {
  draw_bg(true);
  draw_panel(8, 8, BOT_W - 16, BOT_H - 40, 1.0f);
  if (SCORES.empty()) {
    text_c("还没有冒险记录", 0, BOT_W, 100, CLR_GREY, 1.0f);
    return;
  }
  int shown = 0;
  for (size_t i = (size_t)g_rank_scroll; i < SCORES.size() && shown < 6; i++, shown++) {
    const ScoreEntry& e = SCORES[i];
    float y = 18 + shown * 32;
    textf(18, y, CLR_YELLOW, 1.0f, "#%d", (int)i + 1);
    draw_hero_portrait(e.hero % 6, 50, y - 6, 1.0f);
    text(HERO_NAMES[e.hero % 6], 80, y, CLR_WHITE, 1.0f);
    textf(160, y, CLR_GREY, 1.0f, "第 %d 层", e.depth);
    textf(240, y, CLR_WHITE, 1.0f, "%d", e.score);
  }
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------
static void settings_update() {
  if (IN.pressed(KEY_B)) { go_scene(SETTINGS_BACK); return; }
  if (IN.dx) {
    g_set_tab = (g_set_tab + IN.dx + 3) % 3;
    g_set_row = 0;
  }
  if (IN.dy) {
    int n = tab_rows(g_set_tab);
    g_set_row = (g_set_row + IN.dy + n) % n;
  }
  if (IN.pressed(KEY_A)) row_activate(g_set_tab, g_set_row);
  if (IN.tap) {
    for (int t = 0; t < 3; t++)
      if (in_rect(IN.tap_x, IN.tap_y, 4 + t * 106, 2, 102, 26)) {
        g_set_tab = t;
        g_set_row = 0;
        return;
      }
    for (int r = 0; r < tab_rows(g_set_tab); r++)
      if (in_rect(IN.tap_x, IN.tap_y, 10, 34 + r * 30, 300, 26)) {
        g_set_row = r;
        row_activate(g_set_tab, r);
        return;
      }
    if (in_rect(IN.tap_x, IN.tap_y, 100, 196, 120, 28)) go_scene(SETTINGS_BACK);
  }
}

static void settings_draw_top() {
  draw_bg(false);
  draw_logo(1.0f, 10);
  text_c("设置", 0, TOP_W, 148, CLR_YELLOW, 2.0f);
  draw_top_hint("B 返回主菜单");
}

static void settings_draw_bottom() {
  draw_bg(true);
  // tabs
  for (int t = 0; t < 3; t++)
    draw_button(4 + t * 106, 2, 102, 26, tab_name(t), g_set_tab == t, 2.0f);
  // rows
  int n = tab_rows(g_set_tab);
  for (int r = 0; r < n; r++) {
    float y = 34 + r * 30;
    draw_button(10, y, 300, 26, "", g_set_row == r, 2.0f);
    text(row_label(g_set_tab, r), 20, y + 7, g_set_row == r ? CLR_YELLOW : CLR_WHITE, 1.0f);
    char v[32];
    row_value(g_set_tab, r, v, sizeof(v));
    text(v, 300 - text_w(v, 1.0f) - 16, y + 7, CLR_GREY, 1.0f);
  }
  draw_button(100, 196, 120, 28, "返回", false, 2.0f);
  text_c("左右键 切换标签   上下键 选择   A 修改", 0, BOT_W, 178, CLR_GREY, 1.0f);
}

// ---------------------------------------------------------------------------
// Saves
// ---------------------------------------------------------------------------
static void saves_update() {
  if (IN.pressed(KEY_B)) { go_scene(Scene::TITLE); return; }
  if (IN.dx) g_save_sel = (g_save_sel % 2 == 0) ? g_save_sel + 1 : g_save_sel - 1;
  if (IN.dy) g_save_sel = (g_save_sel + 2) % 4;
  if (IN.pressed(KEY_A)) {
    if (g_save_sel == 3) { go_scene(Scene::HEROES); }
    else if (SLOTS[g_save_sel].used) load_slot(g_save_sel);
    else go_scene(Scene::HEROES);
  }
  if (IN.tap) {
    for (int i = 0; i < 4; i++) {
      float x = 5 + (i % 2) * 160, y = 4 + (i / 2) * 104;
      if (in_rect(IN.tap_x, IN.tap_y, x, y, 150, 98)) {
        g_save_sel = i;
        if (i == 3 || !SLOTS[i].used) go_scene(Scene::HEROES);
        else load_slot(i);
        return;
      }
    }
  }
}

static void saves_draw_top() {
  draw_bg(false);
  draw_icon(0, 48, 16, 16, TOP_W / 2 - 40, 24, 5.0f);
  text_c("进行中的游戏", 0, TOP_W, 112, CLR_YELLOW, 2.0f);
  text_c("选择一个存档继续，或开始新的冒险", 0, TOP_W, 142, CLR_GREY, 1.0f);
  draw_top_hint("B 返回主菜单");
}

static void saves_draw_bottom() {
  draw_bg(true);
  for (int i = 0; i < 4; i++) {
    float x = 5 + (i % 2) * 160, y = 4 + (i / 2) * 104;
    draw_slot(i, x, y, 150, 98, g_save_sel == i);
  }
  text_c("A 载入 / 新建", 0, BOT_W, 214, CLR_GREY, 1.0f);
}

// ---------------------------------------------------------------------------
// Hero select
// ---------------------------------------------------------------------------
static void heroes_update() {
  if (IN.pressed(KEY_B)) { go_scene(Scene::SAVES); return; }
  int col = g_hero_sel % 3, row = g_hero_sel / 3;
  if (IN.dx) { col = (col + IN.dx + 3) % 3; g_hero_arm = -1; }
  if (IN.dy) { row = (row + IN.dy + 2) % 2; g_hero_arm = -1; }
  g_hero_sel = row * 3 + col;

  if (IN.pressed(KEY_X)) g_hero_info = !g_hero_info;
  if (IN.pressed(KEY_A)) {
    if (g_hero_arm == g_hero_sel) start_new_game(g_hero_sel);
    else g_hero_arm = g_hero_sel;
  }
  if (IN.tap) {
    if (in_rect(IN.tap_x, IN.tap_y, 282, 2, 32, 32)) {
      g_hero_info = !g_hero_info;
      return;
    }
    for (int i = 0; i < 6; i++) {
      int c = i % 3, r = i / 3;
      float x = 58 + c * 72, y = 48 + r * 68;
      if (in_rect(IN.tap_x, IN.tap_y, x, y, 66, 66)) {
        if (g_hero_sel == i && g_hero_arm == i) { start_new_game(i); return; }
        g_hero_sel = i;
        g_hero_arm = i;
        return;
      }
    }
  }
}

static void heroes_draw_top() {
  draw_bg(false);
  draw_img(A_art[g_hero_sel], 0, 8, 1, 1, 0, 0.3f);
  draw_box(0, 0, TOP_W, 34, C2D_Color32(0, 0, 0, 150), 0.4f);
  if (!g_hero_info) {
    text_c("选择一位英雄", 0, TOP_W, 6, CLR_YELLOW, 2.0f);
    draw_top_hint("B 返回");
  } else {
    draw_box(0, 0, TOP_W, TOP_H, C2D_Color32(0, 0, 0, 140), 0.55f);
    draw_panel(16, 24, 368, 176, 2.0f);
    draw_hero_portrait(g_hero_sel, 28, 36, 2.0f);
    text(HERO_NAMES[g_hero_sel], 74, 36, CLR_YELLOW, 2.0f);
    text(HERO_TITLES[g_hero_sel], 74, 66, CLR_GREY, 1.0f);
    text_wrap(HERO_DESC[g_hero_sel], 28, 88, 340, CLR_WHITE, 1.0f, 2.0f);
    text_c("再点一次 i 关闭", 16, 368, 168, CLR_GREY, 1.0f);
  }
}

static void heroes_draw_bottom() {
  draw_bg(true);
  // header row
  draw_panel(6, 2, BOT_W - 12, 44, 1.0f);
  text(HERO_NAMES[g_hero_sel], 14, 6, CLR_YELLOW, 1.0f);
  text(HERO_TITLES[g_hero_sel], 14, 22, C2D_Color32(225, 225, 225, 255), 1.0f);
  draw_button(278, 8, 36, 32, "", false, 2.0f);
  draw_icon(16, 32, 14, 14, 282, 10, 2.0f, g_hero_info ? CLR_YELLOW : CLR_WHITE);

  for (int i = 0; i < 6; i++) {
    int c = i % 3, r = i / 3;
    float x = 58 + c * 72, y = 48 + r * 68;
    draw_button(x, y, 66, 66, "", false, 2.0f);
    draw_hero_portrait(i, x + 9, y + 1, 2.0f);
    if (g_hero_sel == i)
      draw_cursor(x, y, 66, 66, g_hero_arm == i ? CLR_GREEN : CLR_YELLOW);
  }
  text_c(g_hero_arm == g_hero_sel ? "再按一次 A 开始冒险" : "A 选择英雄   X 查看信息",
         0, BOT_W, 186, CLR_GREY, 1.0f);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------
static void update() {
  switch (SCENE) {
    case Scene::TITLE: title_update(); break;
    case Scene::RANKINGS: rankings_update(); break;
    case Scene::SETTINGS: settings_update(); break;
    case Scene::SAVES: saves_update(); break;
    case Scene::HEROES: heroes_update(); break;
    case Scene::GAME: game_update(); break;
  }
}

static void draw_top() {
  switch (SCENE) {
    case Scene::TITLE: title_draw_top(); break;
    case Scene::RANKINGS: rankings_draw_top(); break;
    case Scene::SETTINGS: settings_draw_top(); break;
    case Scene::SAVES: saves_draw_top(); break;
    case Scene::HEROES: heroes_draw_top(); break;
    case Scene::GAME: game_draw_top(); break;
  }
}
static void draw_bottom() {
  switch (SCENE) {
    case Scene::TITLE: title_draw_bottom(); break;
    case Scene::RANKINGS: rankings_draw_bottom(); break;
    case Scene::SETTINGS: settings_draw_bottom(); break;
    case Scene::SAVES: saves_draw_bottom(); break;
    case Scene::HEROES: heroes_draw_bottom(); break;
    case Scene::GAME: game_draw_bottom(); break;
  }
}

int main() {
  gfxInitDefault();
  C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
  C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
  C2D_Prepare();
  // 2D UI: always pass the depth test, keep submission order.
  C3D_DepthTest(true, GPU_ALWAYS, GPU_WRITE_ALL);
  romfsInit();
  fsInit();

  g_top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
  g_bot = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

  assets_load();
  settings_load(S);
  slots_load(SLOTS);
  scores_load(SCORES);

  // Debug scene jump: write an integer to sdmc:/3ds/spd/scene.txt before launch.
  {
    FILE* f = fopen(DATA_DIR "/scene.txt", "rb");
    if (f) {
      int want = 0;
      if (fscanf(f, "%d", &want) == 1) {
        remove(DATA_DIR "/scene.txt");
        dlog("scene.txt want=%d", want);
        if (want == 1) go_scene(Scene::RANKINGS);
        else if (want == 2) go_scene(Scene::SETTINGS);
        else if (want == 3) go_scene(Scene::SAVES);
        else if (want == 4) { SCENE = Scene::HEROES; g_hero_sel = 0; }
        else if (want == 5) { dlog("jump to game"); start_new_game(0); }
        else if (want == 14) { SCENE = Scene::HEROES; g_hero_sel = 0; g_hero_info = true; }
        else if (want == 6) {
          if (SLOTS[0].used) load_slot(0); else start_new_game(0);
        }
        else if (want == 7) {
          start_new_game(0);
          G.hp -= 7; G.gold += 99; G.keys += 3; G.quest = 1;
          G.add_item(IT_WAND_FIRE, 1, 0);
          G.save(2);
        }
        else if (want == 8) {
          load_slot(2);
        }
        else if (want == 18) {
          start_new_game(0);
          G.floor = 3; G.gen_level();
          for (Mob& m : G.mobs)
            if (mob_def(m.type).boss) { G.px = m.x - 1; G.py = m.y; break; }
          G.cursor_x = G.px; G.cursor_y = G.py;
          G.pvx = (float)G.px; G.pvy = (float)G.py;
          G.recompute_fov();
        }
        else if (want >= 60 && want <= 64) {
          start_new_game(0);
          if (want == 60 || want == 61) G.floor = G.ghost_floor;
          else if (want == 62) G.floor = G.wand_floor;
          else if (want == 63) G.floor = G.smith_floor;
          else G.floor = G.imp_floor;
          G.gen_level();
          for (int i = 0; i < (int)G.mobs.size(); i++) {
            uint8_t t2 = G.mobs[i].type;
            if (t2 >= M_NPC_GHOST) {
              G.px = G.mobs[i].x - 1; G.py = G.mobs[i].y;
              G.talk_npc(i);
              G.dialog_choose(0);
              if (want == 61) {
                G.ghost_boss_dead = 1;
                G.talk_npc(i);
              }
              break;
            }
          }
          G.pvx = (float)G.px; G.pvy = (float)G.py;
          G.recompute_fov();
          G.demo_lock = true;
        }
        else if (want == 89 || want == 90) {
          G.new_run(0, 0x12345678);
          SCENE = Scene::GAME;
          G.mobs.clear(); G.items.clear(); G.features.clear();
          const int x = G.px, y = G.py;
          Mob m;
          m.type = M_GNOLL;
          m.x = x + (want == 89 ? 1 : 3); m.y = y;
          m.vx = m.x; m.vy = m.y;
          m.hp = m.maxhp = 100;
          m.atk = 4;
          m.ai = Mob::Hunting;
          G.mobs.push_back(m);
          if (want == 89) {
            G.act_at(m.x, m.y, false);
          } else {
            G.add_item(IT_WAND_FIRE, 1);
            for (int i = 0; i < G.inv_n; ++i) if (G.inv[i].id == IT_WAND_FIRE) {
              G.cast_wand(i, m.x, m.y);
              break;
            }
          }
          dlog("[combat] scene=%d hero attack=%d mob attack=%d mob hp=%d effects=%d",
               want, G.hero_combat.age >= 0, G.mobs[0].combat.age >= 0,
               G.mobs[0].hp, (int)G.combat_fx.size());
          G.tick(100);
          G.zoom = G.zoom_target = 2.f;
          G.freeze_anim = G.demo_lock = true;
        }
        else if (want == 87) {
          // Do not create or overwrite a regular save slot for the audit.
          G.new_run(0, 1);
          SCENE = Scene::GAME;
          int bad = 0;
          for (uint32_t seed = 1; seed <= 32; ++seed) {
            G.seed = 0x9e3779b9u * seed;
            for (int floor = 1; floor <= 25; ++floor) {
              G.floor = floor;
              G.gen_level();
              bool reached[MH][MW]{};
              std::vector<int> pending{G.py * MW + G.px};
              reached[G.py][G.px] = true;
              bool exit = false;
              for (size_t i = 0; i < pending.size(); ++i) {
                const int p = pending[i], x = p % MW, y = p / MW;
                exit |= G.tiles[y][x] == T_STAIRS;
                for (int dy = -1; dy <= 1; ++dy)
                  for (int dx = -1; dx <= 1; ++dx) {
                    const int nx = x + dx, ny = y + dy;
                    if (!G.walkable(nx, ny) || reached[ny][nx]) continue;
                    reached[ny][nx] = true;
                    pending.push_back(ny * MW + nx);
                  }
              }
              int locks = 0, available = 0;
              for (int y = 0; y < MH; ++y)
                for (int x = 0; x < MW; ++x) locks += G.tiles[y][x] == T_DOOR_LOCKED;
              for (const Feature& f : G.features) locks += f.type == F_CHEST_LOCKED;
              for (const FloorItem& it : G.items)
                if (it.id == IT_KEY && reached[it.y][it.x]) ++available;
              if (!exit || available < locks) {
                ++bad;
                dlog("[generation] seed=%lu floor=%d exit=%d keys=%d/%d",
                     (unsigned long)G.seed, floor, exit, available, locks);
              }
            }
          }
          dlog("[generation] 800 maps, %d failures", bad);
          G.new_run(0, 0x12345678);
          G.demo_lock = true;
        }
        else if (want == 88) {
          G.new_run(0, 0x12345678);
          SCENE = Scene::GAME;
          G.mobs.clear(); G.items.clear(); G.features.clear();
          memset(G.tiles, T_WALL, sizeof(G.tiles));
          for (int x = 2; x <= 20; ++x) G.tiles[10][x] = T_FLOOR;
          for (int y = 10; y <= 15; ++y) G.tiles[y][20] = T_FLOOR;
          G.tiles[10][9] = T_DOOR;
          memset(G.seen, 1, sizeof(G.seen));
          G.pvx = G.px = 2; G.pvy = G.py = 10;
          G.act_at(20, 15, false);
          for (int i = 0; i < 120; ++i) G.tick(100);
          const bool walked = G.px == 20 && G.py == 15 && G.tiles[10][9] == T_DOOR;
          G.pvx = G.px = 20; G.pvy = G.py = 15;
          Mob chaser;
          chaser.x = 15; chaser.y = 10;
          chaser.ai = Mob::Hunting; chaser.target = 15 * MW + 20;
          G.mobs.push_back(chaser);
          // With the hero invisible this must use the last known position.
          G.invis_t = 10;
          G.mob_turn(G.mobs[0]);
          bool pursued = G.mobs[0].x == 16 && G.mobs[0].y == 10;
          G.cursor_x = G.px; G.cursor_y = G.py;
          dlog("[navigation] long route + door=%d, last known target=%d", walked, pursued);
          G.demo_lock = true;
        }
        else if (want == 85 || want == 86) {
          // Same geometry for all three states, at native and fractional zoom.
          start_new_game(0);
          G.mobs.clear(); G.items.clear(); G.features.clear();
          const int cx = MW / 2, cy = MH / 2;
          for (int y = 0; y < MH; y++) {
            for (int x = 0; x < MW; x++) {
              G.tiles[y][x] = (x == 0 || y == 0 || x == MW - 1 || y == MH - 1)
                                 ? T_WALL : T_FLOOR;
              G.seen[y][x] = G.vis[y][x] = true;
            }
          }
          const uint8_t states[] = {T_DOOR, T_DOOR_OPEN, T_DOOR_LOCKED};
          for (int i = 0; i < 3; i++) {
            const int x = cx + (i - 1) * 4, y = cy - 2;
            for (int dy = -2; dy <= 2; dy++) G.tiles[y + dy][x] = T_WALL;
            G.tiles[y][x] = states[i];
            G.tiles[cy + 3][x - 1] = G.tiles[cy + 3][x + 1] = T_WALL;
            G.tiles[cy + 3][x] = states[i];
          }
          G.cursor_x = G.px = cx + 1; G.cursor_y = G.py = cy + 1;
          G.pvx = G.px; G.pvy = G.py;
          G.zoom = G.zoom_target = want == 85 ? 1.0f : 1.33f;
          G.demo_lock = true;
        }
        else if (want == 84) {
          start_new_game(0);
          G.mobs.clear();
          for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) G.seen[y][x] = G.vis[y][x] = true;
          G.demo_lock = true;
        }
        else if (want == 83) {
          start_new_game(0);
          G.zoom = G.zoom_target = 1.33f;
          G.mobs.clear();
          for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) G.seen[y][x] = G.vis[y][x] = true;
          for (const Feature& f : G.features)
            dlog("[f83] type%d x%d y%d", f.type, f.x, f.y);
          G.demo_lock = true;
        }
        else if (want >= 80 && want <= 82) {
          start_new_game(0);
          G.zoom = G.zoom_target = (want == 80) ? 1.6f : (want == 81 ? 0.75f : 1.33f);
          G.demo_lock = true;
        }
        else if (want == 79) {
          start_new_game(0);
          int dx = -1, dy = -1;
          for (int y = 1; y < MH - 1 && dx < 0; y++)
            for (int x = 1; x < MW - 1; x++)
              if (G.tiles[y][x] == T_DOOR) { dx = x; dy = y; break; }
          if (dx >= 0) {
            G.tiles[dy][dx] = T_DOOR_OPEN;
            G.px = dx; G.py = dy; G.pvx = (float)dx; G.pvy = (float)dy;
            int ex = dx, ey = dy;
            if (G.walkable(dx + 1, dy)) ex = dx + 1;
            else if (G.walkable(dx - 1, dy)) ex = dx - 1;
            else if (G.walkable(dx, dy + 1)) ey = dy + 1;
            else if (G.walkable(dx, dy - 1)) ey = dy - 1;
            G.move_player(ex - dx, ey - dy);
            dlog("[door] opened at %d,%d then moved to %d,%d -> tile %d",
                 dx, dy, ex, ey, (int)G.tiles[dy][dx]);
          }
          G.demo_lock = true;
        }
        else if (want == 78) {
          start_new_game(0);
          for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++)
              G.seen[y][x] = (x < MW / 2) ? 1 : 2;
          G.recompute_fov();
          G.demo_lock = true;
        }
        else if (want == 77) {
          start_new_game(0);
          int found = -1;
          for (int y = 1; y < MH - 1 && found < 0; y++)
            for (int x = 1; x < MW - 1; x++)
              if (G.tiles[y][x] == T_DOOR_LOCKED) { found = y * MW + x; break; }
          if (found >= 0) {
            int tx = found % MW, ty = found / MW;
            // stand next to it, preferring the room side
            static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; k++) {
              int nx = tx + DX[k], ny = ty + DY[k];
              if (G.walkable(nx, ny)) {
                G.px = nx; G.py = ny; G.pvx = (float)nx; G.pvy = (float)ny;
                break;
              }
            }
            G.keys = 5;
          }
          for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) G.seen[y][x] = G.vis[y][x] = true;
          G.zoom = G.zoom_target = 1.6f;
          G.demo_lock = true;
        }
        else if (want == 76) {
          start_new_game(0);
          G.floor = 14;
          G.gen_level();
          G.mobs.clear();
          G.items.clear();
          static const uint8_t types[] = {
              M_SNAKE, M_SWARM, M_SLIME, M_DM100, M_NECROMANCER,
              M_DM200, M_GHOUL, M_ELEMENTAL, M_SCORPIO, M_MIMIC};
          for (int i = 0; i < 10; i++) {
            int x = G.px - 4 + (i % 5) * 2;
            int y = G.py - 2 + (i / 5) * 3;
            if (!G.walkable(x, y)) continue;
            Mob m;
            m.x = x; m.y = y; m.vx = (float)x; m.vy = (float)y;
            m.type = types[i];
            m.maxhp = m.hp = 100;
            m.sleep_t = 999;
            G.mobs.push_back(m);
          }
          for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) G.seen[y][x] = G.vis[y][x] = true;
          G.demo_lock = true;
        }
        else if (want == 75) {
          start_new_game(0);
          G.floor = 14;
          G.gen_level();
          G.mobs.clear();
          static const uint8_t types[] = {
              M_SNAKE, M_SWARM, M_SLIME, M_DM100, M_NECROMANCER,
              M_DM200, M_GHOUL, M_ELEMENTAL, M_SCORPIO, M_MIMIC};
          int n = (int)(sizeof(types) / sizeof(types[0]));
          for (int i = 0; i < n; i++) {
            int x = G.px - 4 + (i % 5) * 2;
            int y = G.py - 3 + (i / 5) * 3;
            if (!G.walkable(x, y)) continue;
            Mob m;
            m.x = x; m.y = y; m.vx = (float)x; m.vy = (float)y;
            m.type = types[i];
            m.maxhp = m.hp = 100;
            m.sleep_t = 999;
            G.mobs.push_back(m);
          }
          for (int y = 0; y < MH; y++)
            for (int x = 0; x < MW; x++) G.seen[y][x] = G.vis[y][x] = true;
          G.demo_lock = true;
        }
        else if (want == 74) {
          start_new_game(0);
          for (int f = 1; f <= 25; f++) {
            G.floor = f;
            G.gen_level();
            G.debug_dump();
          }
          G.floor = 1;
          G.gen_level();
          G.demo_lock = true;
        }
        else if (want == 73) {
          start_new_game(0);
          G.icon_test = true;
          G.demo_lock = true;
        }
        else if (want == 72) {
          start_new_game(0);
          G.add_item(IT_SCROLL_TRANSMUTE, 1, 0);
          G.add_item(IT_RING_MIGHT, 1, 1);
          G.add_item(IT_POTION_HEAL, 2, 0);
          G.add_item(IT_WAND_FIRE, 1, 0);
          G.add_item(IT_KNIFE, 3, 0);
          for (int i = 0; i < G.inv_n; i++)
            if (G.inv[i].id == IT_SCROLL_TRANSMUTE) { G.use_item(i); break; }
          G.demo_lock = true;
        }
        else if (want == 70) {
          start_new_game(0);
          G.add_item(IT_SEED_SUNGRASS, 3, 0);
          G.add_item(IT_SEED_FIREBLOOM, 3, 0);
          G.add_item(IT_SEED_BLINDWEED, 3, 0);
          for (size_t i = 0; i < G.features.size(); i++)
            if (G.features[i].type == F_ALCHEMY) {
              G.px = G.features[i].x; G.py = G.features[i].y + 1;
              G.pvx = (float)G.px; G.pvy = (float)G.py;
              G.alchemy_open = true; G.demo_lock = true;
              break;
            }
          G.recompute_fov();
        }
        else if (want == 71) {
          start_new_game(0);
          G.level = 12;
          G.check_talents();
          G.paused = 1;
          G.demo_lock = true;
        }
        else if (want == 65) {
          start_new_game(0);
          for (int f = 1; f <= 25; f++) { G.floor = f; G.gen_level(); }
          dlog("all 25 floors generated");
        }
        else if (want == 54) {
          start_new_game(0); G.floor = 6; G.gen_level();
          G.open_shop(); G.demo_lock = true;
        }
        else if (want == 59) {
          start_new_game(0); G.imp_state = 2; G.floor = 20; G.gen_level();
          G.open_shop(); G.demo_lock = true;
        }
        else if (want >= 55 && want <= 58) {
          start_new_game(0);
          static const uint8_t fl[4] = {0, 0, 0, 0};
          (void)fl;
          if (want == 55) G.floor = G.ghost_floor;
          else if (want == 56) G.floor = G.wand_floor;
          else if (want == 57) G.floor = G.smith_floor;
          else G.floor = G.imp_floor;
          G.gen_level();
          for (int i = 0; i < (int)G.mobs.size(); i++) {
            uint8_t t2 = G.mobs[i].type;
            if (t2 == M_NPC_GHOST || t2 == M_NPC_WANDMAKER ||
                t2 == M_NPC_SMITH || t2 == M_NPC_IMP) {
              G.px = G.mobs[i].x - 1; G.py = G.mobs[i].y;
              G.talk_npc(i);
              break;
            }
          }
          G.pvx = (float)G.px; G.pvy = (float)G.py;
          G.recompute_fov();
        }
        else if (want == 40 || want == 41) {
          start_new_game(0);
          G.add_item(IT_RING_MIGHT, 1, 1);
          G.add_item(IT_RING_EVASION, 1, 0);
          G.add_item(IT_KNIFE, 3, 0);
          G.add_item(IT_SEED_SUNGRASS, 2, 0);
          G.add_item(IT_POTION_HASTE, 1, 0);
          G.add_item(IT_WAND_PRISMATIC, 1, 0);
          G.add_item(IT_SCROLL_CHALLENGE, 1, 0);
          G.backpack = true;
          G.demo_lock = true;
          g_bag_sel = (want == 41) ? 2 : 0;
          g_bag_tab = 0;
          g_bag_action = (want == 41);
        }
        else if (want >= 25 && want <= 29) {
          int f = (want - 25) * 5 + 5;
          start_new_game(0);
          G.floor = f;
          G.gen_level();
          for (Mob& m : G.mobs)
            if (mob_def(m.type).boss) { G.px = m.x - 1; G.py = m.y; break; }
          G.cursor_x = G.px; G.cursor_y = G.py;
          G.pvx = (float)G.px; G.pvy = (float)G.py;
          G.recompute_fov();
        }
        else if (want >= 30 && want <= 33) {
          static const int fl[4] = {6, 11, 16, 21};
          start_new_game(0);
          G.floor = fl[want - 30];
          G.gen_level();
          G.pvx = (float)G.px; G.pvy = (float)G.py;
          G.recompute_fov();
        }
        else if (want == 34) {
          start_new_game(0); G.floor = 12; G.gen_level();
          for (int i = 0; i < (int)G.mobs.size(); i++)
            if (G.mobs[i].type == M_NPC_SMITH) {
              G.px = G.mobs[i].x - 1; G.py = G.mobs[i].y;
              G.talk_npc(i);
              break;
            }
          G.pvx = (float)G.px; G.pvy = (float)G.py; G.recompute_fov();
        }
        else if (want == 35) {
          start_new_game(0); G.floor = 17; G.gen_level();
          for (int i = 0; i < (int)G.mobs.size(); i++)
            if (G.mobs[i].type == M_NPC_IMP) {
              G.px = G.mobs[i].x - 1; G.py = G.mobs[i].y;
              G.talk_npc(i);
              break;
            }
          G.pvx = (float)G.px; G.pvy = (float)G.py; G.recompute_fov();
        }
        else if (want == 36) {
          start_new_game(0); G.floor = 6; G.gen_level();
          G.open_shop(); G.demo_lock = true;
        }
        else if (want == 24) {
          start_new_game(0);
          G.add_item(IT_SHORTSWORD, 1, 1);
          G.add_item(IT_LEATHER_ARMOR, 1, 0);
          G.add_item(IT_WAND_MAGIC, 1, 0);
          G.add_item(IT_WAND_FIRE, 1, 0);
          G.add_item(IT_SCROLL_UP, 2);
          G.add_item(IT_POTION_HEAL, 3);
          G.add_item(IT_BOMB, 2);
          G.keys += 2;
          G.gold += 40;
          G.backpack = true;
          G.demo_lock = true;
          g_bag_sel = 0;
          g_bag_tab = 1;
          g_bag_action = false;
        }
        else if (want == 23) {
          start_new_game(0);
          G.add_item(IT_WAND_FIRE, 1, 0);
          G.backpack = true;
          G.demo_lock = true;
          g_bag_sel = 0;
          g_bag_tab = 0;
          g_bag_action = true;
        }
        else if (want == 22) {
          start_new_game(0);
          G.freeze_anim = true;
          G.search();
          G.search_ms = 400;
          G.demo_lock = true;
          for (SearchFx& f : G.search_fx) f.delay = 0;
        }
        else if (want == 21) {
          start_new_game(0);
          G.freeze_anim = true;
          G.pvx = (float)G.px + 0.55f;
          G.panim_ms = 250;
          for (Mob& m : G.mobs) {
            if (m.type < M_NPC_GHOST) { m.vx = (float)m.x - 0.5f; m.anim_ms = 150; break; }
          }
        }
        else if (want == 19) {
          start_new_game(0);
          G.quest = 1; G.floor = 2; G.gen_level();
          for (FloorItem& it : G.items)
            if (it.id == IT_AMULET) { G.px = it.x; G.py = it.y + 1; break; }
          G.cursor_x = G.px; G.cursor_y = G.py;
          G.pvx = (float)G.px; G.pvy = (float)G.py;
          G.recompute_fov();
        }
        else if (want >= 15 && want <= 17) {
          start_new_game(0);
          G.add_item(IT_SHORTSWORD, 1, 1);
          G.add_item(IT_LEATHER_ARMOR, 1, 0);
          G.add_item(IT_WAND_MAGIC, 1, 0);
          G.add_item(IT_WAND_FIRE, 1, 0);
          G.add_item(IT_SCROLL_UP, 2);
          G.add_item(IT_POTION_HEAL, 3);
          G.add_item(IT_BOMB, 2);
          G.keys += 2;
          G.gold += 40;
          G.quest = 1;
          if (want == 16) { G.backpack = true; G.demo_lock = true; }
          if (want == 17) {
            for (int i = 0; i < G.inv_n; i++)
              if (item_def(G.inv[i].id).kind == K_WAND) {
                G.targeting = true;
                G.target_item = i;
                break;
              }
          }
        }
        else if (want >= 10 && want <= 13) {
          start_new_game(0);
          if (want == 10)
            G.open_dialog(1, "幽魂",
                          "旅行者，这座地牢里有无数亡魂。\n你愿意替我找回我的护符吗？",
                          "接受任务", "暂时离开");
          else if (want == 11) G.backpack = true;
          else if (want == 12) G.paused = 1;
          else if (want == 13) {
            G.add_item(IT_BOMB, 1);
            G.targeting = true;
            G.target_item = G.inv_n - 1;
          }
        }
      }
      fclose(f);
    }
  }

  g_last_ms = osGetTime();
  int frames = 0;
  u64 fps_t = g_last_ms;
  int boot_frames = 0;
  dlog("boot scene=%d", (int)SCENE);

  while (aptMainLoop()) {
    hidScanInput();
    input_update(IN);
    if (boot_frames < 30) {
      boot_frames++;
      IN.down = IN.held = 0;
      IN.dx = IN.dy = 0;
      IN.tap = false;
      if (boot_frames == 30) dlog("input unlocked, scene=%d", (int)SCENE);
    }
    TIME_S = osGetTime() / 1000.0f;
    update();

    C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
    C2D_SetTintMode(C2D_TintMult);
    C2D_TargetClear(g_top, CLR_BLACK);
    C2D_SceneBegin(g_top);
    draw_top();
    C2D_TargetClear(g_bot, CLR_BLACK);
    C2D_SceneBegin(g_bot);
    draw_bottom();
    C3D_FrameEnd(0);

    frames++;
    u64 now = osGetTime();
    if (now - fps_t >= 1000) {
      FPS = frames * 1000.0f / (now - fps_t);
      frames = 0;
      fps_t = now;
    }
  }

  fsExit();
  romfsExit();
  C2D_Fini();
  C3D_Fini();
  gfxExit();
  return 0;
}
