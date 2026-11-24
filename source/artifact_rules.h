#pragma once
// SPD v4.0.2 Generator.ARTIFACT and default charge formulas. GPL-3.0-or-later.
#include "items.h"
#include <algorithm>
#include <cmath>
namespace artifact_rules {
constexpr int count = 13;
inline int index(uint8_t id) { return int(id) - IT_ART_TOOLKIT; }
inline bool valid(uint8_t id) { return index(id) >= 0 && index(id) < count; }
inline int level_cap(uint8_t id) {
  return id == IT_ART_CHAINS || id == IT_ART_HOURGLASS ? 5
         : id == IT_ART_SANDALS                        ? 3
                                                       : 10;
}
inline int capacity(uint8_t id, int level) {
  switch (id) {
  case IT_ART_CLOAK:
  case IT_ART_TOME:
    return std::min(10, 3 + level);
  case IT_ART_ROSE:
  case IT_ART_SANDALS:
  case IT_ART_FORESIGHT:
    return 100;
  case IT_ART_CHAINS:
    return 5 + 2 * level;
  case IT_ART_HORN:
  case IT_ART_ARMBAND:
    return 5 + level / 2;
  case IT_ART_KEY:
    return 3 + level / 2;
  case IT_ART_HOURGLASS:
    return 5 + level;
  case IT_ART_SPELLBOOK:
    return 2 + int(.6f * level);
  case IT_ART_TOOLKIT:
    return 255;
  default:
    return 0;
  }
}
inline int initial_charge(uint8_t id, int level = 0) {
  return id == IT_ART_CLOAK || id == IT_ART_TOME || id == IT_ART_ROSE ||
                 id == IT_ART_CHAINS || id == IT_ART_KEY ||
                 id == IT_ART_HOURGLASS || id == IT_ART_SPELLBOOK
             ? capacity(id, level)
             : 0;
}
inline float recharge(uint8_t id, int level, int charge) {
  const float missing = capacity(id, level) - charge;
  switch (id) {
  case IT_ART_CLOAK:
  case IT_ART_TOME:
    return 1.f / (45 - missing - (level > 7 ? 5 * (level - 7) / 3.f : 0));
  case IT_ART_ROSE:
    return .2f;
  case IT_ART_CHAINS:
    return 1.f / (40 - 2 * missing);
  case IT_ART_KEY:
    return 1.f / (120 - 7.5f * missing);
  case IT_ART_FORESIGHT:
    return .05f + .005f * level;
  case IT_ART_HOURGLASS:
    return 1.f / (90 - 3 * missing);
  case IT_ART_SPELLBOOK:
    return 1.f / (120 - 5 * missing);
  default:
    return 0;
  }
}
inline bool ordinary(uint8_t id) {
  return valid(id) && id != IT_ART_CLOAK && id != IT_ART_TOME;
}
} // namespace artifact_rules
