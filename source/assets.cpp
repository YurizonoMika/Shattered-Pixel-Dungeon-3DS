#include "common.h"

#include <stdarg.h>
#include <vector>

Img A_banners, A_chrome, A_toolbar, A_status, A_icons, A_heroicons, A_avatars,
    A_items, A_itemicons, A_tiles;
Img A_hero[6], A_art[6];
Img A_rat, A_gnoll;
Img A_t_archs, A_t_back, A_t_front;
Img A_font;

static std::vector<C2D_SpriteSheet> g_sheets;

static Img load_img(const char* path, int w, int h) {
  Img im;
  C2D_SpriteSheet sh = C2D_SpriteSheetLoad(path);
  if (!sh) {
    printf("[assets] missing %s\n", path);
    return im;
  }
  g_sheets.push_back(sh);
  C2D_Image ci = C2D_SpriteSheetGetImage(sh, 0);
  // Pixel art: never blur between texels.
  C3D_TexSetFilter(ci.tex, GPU_NEAREST, GPU_NEAREST);
  im.tex = ci.tex;
  im.base = ci.subtex;
  im.w = w;
  im.h = h;
  return im;
}

bool assets_load() {
  A_banners = load_img("romfs:/gfx/banners.t3x", 512, 256);
  A_chrome = load_img("romfs:/gfx/chrome.t3x", 128, 64);
  A_toolbar = load_img("romfs:/gfx/toolbar.t3x", 256, 32);
  A_status = load_img("romfs:/gfx/status_pane.t3x", 128, 128);
  A_icons = load_img("romfs:/gfx/icons.t3x", 256, 128);
  A_heroicons = load_img("romfs:/gfx/hero_icons.t3x", 128, 256);
  A_avatars = load_img("romfs:/gfx/avatars.t3x", 256, 32);
  A_items = load_img("romfs:/gfx/items.t3x", 256, 512);
  A_itemicons = load_img("romfs:/gfx/item_icons.t3x", 128, 64);
  A_tiles = load_img("romfs:/gfx/tiles.t3x", 256, 256);

  static const char* heroes[6] = {"warrior", "mage", "rogue", "huntress", "duelist", "cleric"};
  char path[128];
  for (int i = 0; i < 6; i++) {
    snprintf(path, sizeof(path), "romfs:/gfx/hero_%s.t3x", heroes[i]);
    A_hero[i] = load_img(path, 256, 128);
    snprintf(path, sizeof(path), "romfs:/gfx/art_%s.t3x", heroes[i]);
    A_art[i] = load_img(path, 400, 225);
  }
  A_rat = load_img("romfs:/gfx/mob_rat.t3x", 256, 64);
  A_gnoll = load_img("romfs:/gfx/mob_gnoll.t3x", 256, 64);
  A_t_archs = load_img("romfs:/gfx/title_archs.t3x", 1024, 256);
  A_t_back = load_img("romfs:/gfx/title_back.t3x", 512, 512);
  A_t_front = load_img("romfs:/gfx/title_front.t3x", 1024, 512);
  A_font = load_img("romfs:/gfx/font.t3x", FONT_ATLAS_W, FONT_ATLAS_H);
  return A_banners.ok() && A_chrome.ok() && A_tiles.ok() && A_font.ok();
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

static inline Tex3DS_SubTexture make_sub(const Img& im, int sx, int sy, int sw,
                                         int sh) {
  Tex3DS_SubTexture st;
  st.width = (u16)sw;
  st.height = (u16)sh;
  float span_u = im.base->right - im.base->left;
  float span_v = im.base->top - im.base->bottom;  // > 0
  float u0 = im.base->left + (sx / (float)im.w) * span_u;
  float u1 = im.base->left + ((sx + sw) / (float)im.w) * span_u;
  // v grows downward in image space; tex3ds stores v=1 at the top row.
  float v0 = im.base->top - (sy / (float)im.h) * span_v;
  float v1 = im.base->top - ((sy + sh) / (float)im.h) * span_v;
  st.left = u0;
  st.right = u1;
  st.top = v0;
  st.bottom = v1;
  return st;
}

static inline C2D_ImageTint make_tint(u32 color) {
  C2D_ImageTint t;
  C2D_SetImageTint(&t, C2D_TopLeft, color, 1.0f);
  C2D_SetImageTint(&t, C2D_TopRight, color, 1.0f);
  C2D_SetImageTint(&t, C2D_BotLeft, color, 1.0f);
  C2D_SetImageTint(&t, C2D_BotRight, color, 1.0f);
  return t;
}

void draw_rect_src(const Img& im, int sx0, int sy0, int sw, int sh, float x,
                   float y, float dx, float dy, u32 tint, float z) {
  if (!im.ok() || sw <= 0 || sh <= 0) return;
  Tex3DS_SubTexture st = make_sub(im, sx0, sy0, sw, sh);
  C2D_Image img{im.tex, &st};
  C2D_ImageTint t;
  const C2D_ImageTint* tp = nullptr;
  if (tint) {
    t = make_tint(tint);
    tp = &t;
  }
  C2D_DrawImageAt(img, x, y, z, tp, dx / (float)sw, dy / (float)sh);
}

void draw_cell(const Img& im, int cx, int cy, int cw, int ch, float x, float y,
               float sx, float sy, u32 tint, float z) {
  draw_rect_src(im, cx, cy, cw, ch, x, y, cw * sx, ch * sy, tint, z);
}

void draw_img(const Img& im, float x, float y, float sx, float sy, u32 tint,
              float z) {
  draw_rect_src(im, 0, 0, im.w, im.h, x, y, im.w * sx, im.h * sy, tint, z);
}

void draw_nine(const Img& im, int sx, int sy, int sw, int sh, int l, int t, int r,
               int b, float x, float y, float w, float h, float scale, u32 tint) {
  if (!im.ok()) return;
  const float L = l * scale, T = t * scale, R = r * scale, B = b * scale;
  const float cw = w - L - R, ch = h - T - B;
  const int mw = sw - l - r, mh = sh - t - b;
  // corners
  draw_rect_src(im, sx, sy, l, t, x, y, L, T, tint, 0.5f);
  draw_rect_src(im, sx + sw - r, sy, r, t, x + w - R, y, R, T, tint, 0.5f);
  draw_rect_src(im, sx, sy + sh - b, l, b, x, y + h - B, L, B, tint, 0.5f);
  draw_rect_src(im, sx + sw - r, sy + sh - b, r, b, x + w - R, y + h - B, R, B, tint, 0.5f);
  // edges
  draw_rect_src(im, sx + l, sy, mw, t, x + L, y, cw, T, tint, 0.5f);
  draw_rect_src(im, sx + l, sy + sh - b, mw, b, x + L, y + h - B, cw, B, tint, 0.5f);
  draw_rect_src(im, sx, sy + t, l, mh, x, y + T, L, ch, tint, 0.5f);
  draw_rect_src(im, sx + sw - r, sy + t, r, mh, x + w - R, y + T, R, ch, tint, 0.5f);
  // center
  draw_rect_src(im, sx + l, sy + t, mw, mh, x + L, y + T, cw, ch, tint, 0.5f);
}

void draw_box(float x, float y, float w, float h, u32 color, float z) {
  C2D_DrawRectSolid(x, y, z, w, h, color);
}
void draw_frame(float x, float y, float w, float h, float t, u32 color, float z) {
  C2D_DrawRectSolid(x, y, z, w, t, color);
  C2D_DrawRectSolid(x, y + h - t, z, w, t, color);
  C2D_DrawRectSolid(x, y, z, t, h, color);
  C2D_DrawRectSolid(x + w - t, y, z, t, h, color);
}

void draw_cursor(float x, float y, float w, float h, u32 color, float z,
                 bool center_dot) {
  const float c = 5, t = 2;
  // four corner brackets
  draw_box(x, y, c, t, color, z);
  draw_box(x, y, t, c, color, z);
  draw_box(x + w - c, y, c, t, color, z);
  draw_box(x + w - t, y, t, c, color, z);
  draw_box(x, y + h - t, c, t, color, z);
  draw_box(x, y + h - c, t, c, color, z);
  draw_box(x + w - c, y + h - t, c, t, color, z);
  draw_box(x + w - t, y + h - c, t, c, color, z);
  if (center_dot) draw_box(x + w / 2 - 1, y + h / 2 - 1, 2, 2, color, z);
}

// ---------------------------------------------------------------------------
// Text
// ---------------------------------------------------------------------------

static const FontGlyph* find_glyph(uint32_t cp) {
  int lo = 0, hi = FONT_GLYPH_COUNT - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (FONT_GLYPHS[mid].cp == cp) return &FONT_GLYPHS[mid];
    if (FONT_GLYPHS[mid].cp < cp) lo = mid + 1;
    else hi = mid - 1;
  }
  return nullptr;
}

static uint32_t utf8_next(const char** s) {
  const unsigned char* p = (const unsigned char*)*s;
  uint32_t c = *p;
  if (!c) return 0;
  if (c < 0x80) { *s += 1; return c; }
  if ((c >> 5) == 6 && p[1]) { *s += 2; return ((c & 0x1F) << 6) | (p[1] & 0x3F); }
  if ((c >> 4) == 14 && p[1] && p[2]) {
    *s += 3;
    return ((c & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
  }
  if ((c >> 3) == 30) {
    *s += 4;
    return ((c & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) |
           (p[3] & 0x3F);
  }
  *s += 1;
  return '?';
}

static float glyph_w(uint32_t cp, float scale) {
  const FontGlyph* g = find_glyph(cp);
  return g ? g->adv * scale : 0;
}

static void draw_glyph(const FontGlyph* g, float x, float y, float scale,
                       u32 color) {
  if (g->w <= 0 || g->h <= 0) return;
  Tex3DS_SubTexture st = make_sub(A_font, g->x, g->y, g->w, g->h);
  C2D_Image img{A_font.tex, &st};
  C2D_ImageTint t = make_tint(color);
  C2D_DrawImageAt(img, x, y, 0.5f, &t, scale, scale);
}

void text(const char* s, float x, float y, u32 color, float scale) {
  if (!A_font.ok()) return;
  const u32 shadow = C2D_Color32(0, 0, 0, 170);
  const float sh = (scale >= 1.5f) ? 2.0f : 1.0f;
  float pen = x;
  const char* p = s;
  while (*p) {
    uint32_t cp = utf8_next(&p);
    if (cp == '\n') continue;
    const FontGlyph* g = find_glyph(cp);
    if (!g) { pen += 6 * scale; continue; }
    if (g->w > 0 && g->h > 0) {
      // Integer-snapped placement keeps every source pixel on the grid.
      float gx = roundf(pen + g->xoff * scale);
      float gy = roundf(y + g->yoff * scale);
      draw_glyph(g, gx + sh, gy + sh, scale, shadow);
      draw_glyph(g, gx, gy, scale, color);
    }
    pen += g->adv * scale;
  }
}

void textf(float x, float y, u32 color, float scale, const char* fmt, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  text(buf, x, y, color, scale);
}

float text_w(const char* s, float scale) {
  float w = 0;
  const char* p = s;
  while (*p) {
    uint32_t cp = utf8_next(&p);
    w += glyph_w(cp, scale);
  }
  return w;
}

void text_c(const char* s, float x, float w, float y, u32 color, float scale) {
  text(s, x + (w - text_w(s, scale)) * 0.5f, y, color, scale);
}

float text_wrap(const char* s, float x, float y, float w, u32 color, float scale,
                float gap) {
  char line[256];
  int n = 0;
  float cy = y;
  const char* p = s;
  while (*p) {
    uint32_t cp = utf8_next(&p);
    if (cp == '\n') {
      line[n] = 0;
      text(line, x, cy, color, scale);
      cy += (FONT_LINE_H + gap) * scale;
      n = 0;
      continue;
    }
    int len = 0;
    // re-encode for measurement is awkward; measure via table directly
    const FontGlyph* g = find_glyph(cp);
    float adv = g ? g->adv * scale : 6 * scale;
    if ((text_w(line, scale) + adv) > w && n > 0) {
      line[n] = 0;
      text(line, x, cy, color, scale);
      cy += (FONT_LINE_H + gap) * scale;
      n = 0;
    }
    // store utf8 bytes (up to 4)
    unsigned char buf[5] = {0};
    if (cp < 0x80) { buf[0] = (unsigned char)cp; len = 1; }
    else if (cp < 0x800) { buf[0] = 0xC0 | (cp >> 6); buf[1] = 0x80 | (cp & 0x3F); len = 2; }
    else if (cp < 0x10000) { buf[0] = 0xE0 | (cp >> 12); buf[1] = 0x80 | ((cp >> 6) & 0x3F); buf[2] = 0x80 | (cp & 0x3F); len = 3; }
    else { buf[0] = 0xF0 | (cp >> 18); buf[1] = 0x80 | ((cp >> 12) & 0x3F); buf[2] = 0x80 | ((cp >> 6) & 0x3F); buf[3] = 0x80 | (cp & 0x3F); len = 4; }
    if (n + len < (int)sizeof(line)) {
      memcpy(line + n, buf, len);
      n += len;
    }
  }
  if (n > 0) {
    line[n] = 0;
    text(line, x, cy, color, scale);
    cy += (FONT_LINE_H + gap) * scale;
  }
  return cy - y;
}
