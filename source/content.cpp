#include "content.h"

// name, asset, fw, fh, hp, atk, def, xp, tint, boss
const MobDef MOB_DEFS[M_COUNT] = {
    {"巨鼠", 0, 16, 15, 5, 1, 0, 3, 0, false},
    {"豺狼人", 1, 12, 15, 9, 2, 1, 6, 0, false},
    {"下水道螃蟹", 2, 16, 16, 13, 2, 3, 9, 0, false},
    {"白色巨鼠", 0, 16, 15, 11, 3, 1, 8, 0xFFD6E6FF, false},
    {"骷髅", 3, 12, 16, 14, 3, 2, 11, 0, false},
    {"窃贼", 4, 12, 13, 11, 3, 1, 10, 0, false},
    {"监狱守卫", 5, 12, 16, 20, 3, 3, 14, 0, false},
    {"洞穴蝙蝠", 6, 15, 15, 12, 3, 1, 11, 0, false},
    {"洞穴蛮兵", 7, 12, 16, 24, 5, 2, 18, 0, false},
    {"萨满", 8, 12, 15, 16, 4, 1, 15, 0, false},
    {"洞穴蜘蛛", 9, 16, 16, 18, 4, 2, 16, 0, false},
    {"石魔像", 10, 17, 19, 34, 7, 5, 30, 0, false},
    {"武僧", 11, 15, 14, 26, 6, 3, 24, 0, false},
    {"术士", 12, 12, 15, 24, 6, 2, 22, 0, false},
    {"魅魔", 13, 12, 15, 30, 7, 3, 28, 0, false},
    {"魔眼", 14, 16, 18, 26, 6, 3, 26, 0, false},
    {"撕裂者", 15, 15, 14, 32, 8, 3, 30, 0, false},
    {"怨灵", 16, 14, 15, 24, 7, 2, 27, 0, false},
    {"史莱姆王", 17, 20, 14, 50, 5, 2, 60, 0, true},
    {"天狗", 18, 14, 16, 70, 7, 3, 100, 0, true},
    {"DM-300", 19, 25, 22, 110, 9, 6, 160, 0, true},
    {"矮人国王", 20, 16, 16, 150, 10, 7, 220, 0, true},
    {"尤格-泽瓦", 21, 20, 19, 220, 13, 8, 400, 0, true},
    {"幽魂", 22, 16, 16, 999, 0, 0, 0, 0x9FC0FFFF, false},
    {"店主", 23, 16, 16, 999, 0, 0, 0, 0, false},
    {"铁匠", 24, 16, 16, 999, 0, 0, 0, 0, false},
    {"小鬼", 25, 16, 16, 999, 0, 0, 0, 0, false},
};

const RegionDef REGIONS[5] = {
    {"下水道", {M_RAT, M_GNOLL, M_CRAB, M_ALBINO}, M_GOO, 5, 0},
    {"监狱", {M_SKELETON, M_THIEF, M_GUARD, M_THIEF}, M_TENGU, 10, 1},
    {"洞穴", {M_BAT, M_BRUTE, M_SHAMAN, M_SPINNER}, M_DM300, 15, 2},
    {"矮人城", {M_GOLEM, M_MONK, M_WARLOCK, M_MONK}, M_KING, 20, 3},
    {"恶魔大厅", {M_SUCCUBUS, M_EYE, M_RIPPER, M_WRAITH}, M_YOG, 25, 4},
};

int region_of_floor(int floor) {
  int r = (floor - 1) / 5;
  if (r < 0) r = 0;
  if (r > 4) r = 4;
  return r;
}
