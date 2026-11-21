#pragma once

#include <stdint.h>

enum ItemId : uint8_t {
  IT_NONE = 0,
  IT_POTION_HEAL,
  IT_POTION_STR,
  IT_POTION_SHIELD,
  IT_SCROLL_MAP,
  IT_SCROLL_UP,
  IT_SCROLL_TELE,
  IT_FOOD_RATION,
  IT_FOOD_MEAT,
  IT_BOMB,
  IT_DAGGER,
  IT_SHORTSWORD,
  IT_CUDGEL,
  IT_CLOTH_ARMOR,
  IT_LEATHER_ARMOR,
  IT_WAND_MAGIC,
  IT_WAND_FIRE,
  IT_KEY,
  IT_GOLD,
  IT_AMULET,
  IT_COUNT
};

enum ItemKind : uint8_t {
  K_USE = 0,
  K_FOOD,
  K_WEAPON,
  K_ARMOR,
  K_WAND,
  K_BOMB,
  K_KEY,
  K_GOLD,
  K_QUEST,
};

struct ItemDef {
  const char* name;
  const char* desc;
  uint8_t kind;
  int8_t power;       // heal amount / weapon or armor bonus / wand damage
  uint8_t max_charge; // wands
  int16_t sprite;     // index into items.png (16x16 grid)
};

extern const ItemDef ITEM_DEFS[IT_COUNT];

inline const ItemDef& item_def(uint8_t id) {
  return ITEM_DEFS[id < IT_COUNT ? id : 0];
}
const char* item_name(uint8_t id);
void item_sprite_cell(uint8_t id, int& cx, int& cy);
bool item_is_equipment(uint8_t id);
bool item_is_stackable(uint8_t id);
