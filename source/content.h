#pragma once

#include <stdint.h>

enum MobType : uint8_t {
  M_RAT = 0,
  M_GNOLL,
  M_CRAB,
  M_ALBINO,
  // prison
  M_SKELETON,
  M_THIEF,
  M_GUARD,
  // caves
  M_BAT,
  M_BRUTE,
  M_SHAMAN,
  M_SPINNER,
  // city
  M_GOLEM,
  M_MONK,
  M_WARLOCK,
  // halls
  M_SUCCUBUS,
  M_EYE,
  M_RIPPER,
  M_WRAITH,
  // quest minibosses
  M_FETID_RAT, M_TRICKSTER, M_GREAT_CRAB,
  // bosses
  M_GOO,
  M_TENGU,
  M_DM300,
  M_KING,
  M_YOG,
  // npcs
  M_NPC_GHOST,
  M_NPC_SHOP,
  M_NPC_SMITH,
  M_NPC_WANDMAKER,
  M_NPC_IMP,
  // depth-specific rotation mobs (appended to keep old save values stable)
  M_SNAKE, M_SWARM, M_SLIME, M_DM100, M_NECROMANCER, M_DM200, M_GHOUL,
  M_ELEMENTAL, M_SCORPIO, M_MIMIC,
  M_COUNT
};

struct MobDef {
  const char* name;
  uint8_t asset;   // index into A_mobs
  uint8_t fw, fh;  // sprite frame size
  int16_t hp, atk, def, xp;
  uint32_t tint;   // 0 = no tint
  bool boss;
};

struct RegionDef {
  const char* name;
  uint8_t mobs[4];
  uint8_t boss;
  uint8_t boss_floor;  // 5, 10, 15, 20, 25
  uint8_t tiles;       // index into A_tiles
};

extern const MobDef MOB_DEFS[M_COUNT];
extern const RegionDef REGIONS[5];

inline const MobDef& mob_def(uint8_t type) {
  return MOB_DEFS[type < M_COUNT ? type : 0];
}
int region_of_floor(int floor);
