#include "items.h"

// Sprite indices follow Shattered Pixel Dungeon's ItemSpriteSheet groups.
// items.png is a 16x16 grid, so the cell is (index % 16, index / 16).
const ItemDef ITEM_DEFS[IT_COUNT] = {
    {"无", "什么都没有。", K_QUEST, CAT_MISC, 0, 0, -1},
    {"治疗药水", "恢复 15 点生命。", K_USE, CAT_POTION, 15, 0, 352},
    {"力量药水", "永久增加 2 点最大生命。", K_USE, CAT_POTION, 1, 0, 353},
    {"护盾药水", "获得 12 点护盾，优先吸收伤害。", K_USE, CAT_POTION, 12, 0, 355},
    {"探图卷轴", "揭示本层的完整地图。", K_USE, CAT_SCROLL, 0, 0, 304},
    {"强化卷轴", "强化当前武器或护甲。", K_USE, CAT_SCROLL, 0, 0, 305},
    {"传送卷轴", "随机传送到本层某处。", K_USE, CAT_SCROLL, 0, 0, 306},
    {"干粮", "恢复 5 点生命的干粮。", K_FOOD, CAT_MISC, 5, 0, 432},
    {"肉排", "恢复 10 点生命的肉排。", K_FOOD, CAT_MISC, 10, 0, 437},
    {"炸弹", "瞄准后投掷，对周围造成伤害。", K_BOMB, CAT_MISC, 9, 0, 80},
    {"匕首", "轻巧的近战武器。", K_WEAPON, CAT_GEAR, 3, 0, 100},
    {"短剑", "可靠的近战武器。", K_WEAPON, CAT_GEAR, 5, 0, 96},
    {"木棒", "粗糙但结实的武器。", K_WEAPON, CAT_GEAR, 4, 0, 97},
    {"布甲", "最基础的护甲。", K_ARMOR, CAT_GEAR, 1, 0, 176},
    {"皮甲", "轻便的皮制护甲。", K_ARMOR, CAT_GEAR, 3, 0, 177},
    {"魔法飞弹法杖", "瞄准后施法，消耗 1 点充能。", K_WAND, CAT_WAND, 5, 3, 208},
    {"火焰法杖", "瞄准后施法，造成范围伤害。", K_WAND, CAT_WAND, 7, 3, 209},
    {"长剑", "监狱铁匠打造的长剑。", K_WEAPON, CAT_GEAR, 7, 0, 104},
    {"战锤", "沉重的战锤，破坏力惊人。", K_WEAPON, CAT_GEAR, 9, 0, 112},
    {"巨斧", "矮人风格的巨斧。", K_WEAPON, CAT_GEAR, 11, 0, 120},
    {"符文剑", "刻满符文的远古之剑。", K_WEAPON, CAT_GEAR, 13, 0, 128},
    {"锁甲", "由铁环编成的护甲。", K_ARMOR, CAT_GEAR, 5, 0, 178},
    {"鳞甲", "层层甲片覆盖的护甲。", K_ARMOR, CAT_GEAR, 7, 0, 179},
    {"板甲", "矮人锻造的重型板甲。", K_ARMOR, CAT_GEAR, 9, 0, 180},
    {"符文甲", "刻有守护符文的铠甲。", K_ARMOR, CAT_GEAR, 11, 0, 181},
    {"寒霜法杖", "瞄准后施法，冻结目标。", K_WAND, CAT_WAND, 6, 3, 210},
    {"闪电法杖", "瞄准后施法，释放闪电。", K_WAND, CAT_WAND, 9, 3, 211},
    {"钥匙", "用来打开上锁的门。", K_KEY, CAT_MISC, 0, 0, 55},
    {"金币", "闪闪发光的金币。", K_GOLD, CAT_MISC, 0, 0, 18},
    {"幽魂护符", "幽魂遗失的护符，任务物品。", K_QUEST, CAT_MISC, 0, 0, 61},
};

const char* item_name(uint8_t id) { return item_def(id).name; }

// Base value in coins, loosely following SPD's item value scale.
int item_value(uint8_t id) {
  const ItemDef& d = item_def(id);
  switch (d.kind) {
    case K_USE: return id >= IT_SCROLL_MAP && id <= IT_SCROLL_TELE ? 40 : 30;
    case K_FOOD: return 20;
    case K_BOMB: return 30;
    case K_KEY: return 10;
    case K_WEAPON: return 40 + d.power * 10;
    case K_ARMOR: return 35 + d.power * 10;
    case K_WAND: return 80 + d.power * 10;
    default: return 0;
  }
}

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
