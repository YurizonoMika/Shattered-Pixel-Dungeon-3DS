#pragma once

#include "common.h"
#include "game.h"

extern Settings S;
extern SlotMeta SLOTS[3];
extern std::vector<ScoreEntry> SCORES;
extern Scene SCENE;
extern Game G;
extern Input IN;
extern int ACTIVE_SLOT;
extern float TIME_S;
extern float FPS;
extern Scene SETTINGS_BACK;
extern int g_bag_sel;
extern bool g_bag_action;

void dlog(const char* fmt, ...);
void go_scene(Scene s);
void start_new_game(int hero);
void load_slot(int i);
void leave_game_to_saves(bool save);
void record_run_end(bool victory);

// Shared widgets (main.cpp)
void draw_button(float x, float y, float w, float h, const char* label,
                 bool focused, float scale = 2.0f);
void draw_panel(float x, float y, float w, float h, float scale = 2.0f);
void draw_icon(int sx, int sy, int sw, int sh, float x, float y, float scale,
               u32 tint = 0);
void draw_item_icon(uint8_t id, float x, float y, float scale);
void draw_hero_icon(int hero, float x, float y, float scale);
void draw_top_hint(const char* s);
void draw_bg(bool bottom);


// game_screen.cpp
void game_update();
void game_draw_top();
void game_draw_bottom();
