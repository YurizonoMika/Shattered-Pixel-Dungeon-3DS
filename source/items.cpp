#include "items.h"

// Sprite indices follow Shattered Pixel Dungeon's ItemSpriteSheet groups.
// items.png is a 16x16 grid, so the cell is (index % 16, index / 16).
const ItemDef ITEM_DEFS[IT_COUNT] = {
    {"无", K_QUEST, 0, 0, -1},
    {"治疗药水", K_USE, 15, 0, 352},      // POTIONS+0
    {"力量药水", K_USE, 1, 0, 353},       // POTIONS+1
    {"护盾药水", K_USE, 12, 0, 355},      // POTIONS+3
    {"探图卷轴", K_USE, 0, 0, 304},       // SCROLLS+0
    {"强化卷轴", K_USE, 0, 0, 305},       // SCROLLS+1
    {"传送卷轴", K_USE, 0, 0, 306},       // SCROLLS+2
    {"干粮", K_FOOD, 5, 0, 432},          // FOOD+0
    {"肉排", K_FOOD, 10, 0, 437},         // RATION
    {"炸弹", K_BOMB, 9, 0, 80},           // BOMBS+0
    {"匕首", K_WEAPON, 3, 0, 100},        // WEP_TIER1+4
    {"短剑", K_WEAPON, 5, 0, 96},         // WEP_TIER1+0
    {"木棒", K_WEAPON, 4, 0, 97},         // WEP_TIER1+1
    {"布甲", K_ARMOR, 1, 0, 176},         // ARMOR+0
    {"皮甲", K_ARMOR, 3, 0, 177},         // ARMOR+1
    {"魔法飞弹法杖", K_WAND, 5, 3, 208},  // WANDS+0
    {"火焰法杖", K_WAND, 7, 3, 209},      // WANDS+1
    {"钥匙", K_KEY, 0, 0, 55},            // IRON_KEY
    {"金币", K_GOLD, 0, 0, 18},           // GOLD
    {"幽魂护符", K_QUEST, 0, 0, 61},      // AMULET
};

const char* item_name(uint8_t id) { return item_def(id).name; }

void item_sprite_cell(uint8_t id, int& cx, int& cy) {
  int idx = item_def(id).sprite;
  if (idx < 0) { cx = 0; cy = 0; return; }
  cx = (idx % 16) * 16;
  cy = (idx / 16) * 16;
}

bool item_is_equipment(uint8_t id) {
  uint8_t k = item_def(id).kind;
  return k == K_WEAPON || k == K_ARMOR;
}

bool item_is_stackable(uint8_t id) {
  uint8_t k = item_def(id).kind;
  return k == K_USE || k == K_FOOD || k == K_BOMB || k == K_KEY || k == K_GOLD;
}
