#pragma once
// Shattered v4.0.2 MissileWeapon, Dart and concrete missile definitions.
// GPL-3.0-or-later; copyright (C) 2014-2026 Evan Debenham.
#include "hero_rules.h"
#include "magic_rules.h"
namespace projectile_rules {
inline bool dart(uint8_t id) { return id == IT_DART || id == IT_DART_TIPPED; }
inline int tier(uint8_t id) {
  switch (id) {
  case IT_KNIFE:
  case IT_THROWING_SPIKE:
  case IT_THROWING_STONE:
  case IT_DART:
  case IT_DART_TIPPED:
    return 1;
  case IT_FISHING_SPEAR:
  case IT_SHURIKEN:
  case IT_THROWING_CLUB:
    return 2;
  case IT_THROWING_SPEAR:
  case IT_BOLAS:
  case IT_KUNAI:
    return 3;
  case IT_JAVELIN:
  case IT_TOMAHAWK:
  case IT_BOOMERANG:
    return 4;
  case IT_TRIDENT:
  case IT_THROWING_HAMMER:
  case IT_FORCE_CUBE:
    return 5;
  default:
    return 0;
  }
}
inline int base_uses(uint8_t id) {
  if (dart(id))
    return 1000;
  switch (id) {
  case IT_THROWING_SPIKE:
  case IT_THROWING_CLUB:
  case IT_THROWING_HAMMER:
    return 12;
  case IT_FISHING_SPEAR:
  case IT_THROWING_SPEAR:
  case IT_KUNAI:
  case IT_JAVELIN:
  case IT_TRIDENT:
    return 8;
  default:
    return 5;
  }
}
inline int strength(uint8_t id, int level) {
  return 7 + 2 * tier(id) -
         int((std::sqrt(8. * std::max(0, level) + 1) - 1) / 2);
}
inline hero_rules::DamageRange
damage(uint8_t id, int level, bool surprise = false, int crossbow = -1) {
  const int t = tier(id);
  int lo = 2 * t + level, hi = 5 * t + t * level;
  if (id == IT_KNIFE)
    hi = 6 * t + 2 * level;
  if (id == IT_SHURIKEN || id == IT_THROWING_CLUB || id == IT_THROWING_HAMMER ||
      id == IT_KUNAI)
    hi = 4 * t + t * level;
  if (id == IT_BOOMERANG)
    hi = 4 * t + (t - 1) * level;
  if (id == IT_TOMAHAWK) {
    lo = int(std::lround(1.5f * t)) + level;
    hi = 4 * t + (t - 1) * level;
  }
  if (id == IT_FORCE_CUBE)
    hi = 6 * t + t * level;
  if (id == IT_BOLAS) {
    lo = 4;
    hi = 9 + 2 * level;
  }
  if (dart(id)) {
    lo = 1 + level;
    hi = 2 + 2 * level;
    if (crossbow >= 0) {
      lo = 4 + crossbow + level;
      hi = 12 + 3 * crossbow + 2 * level;
    }
  }
  lo = std::max(0, lo);
  hi = std::max(lo, hi);
  if (surprise && (id == IT_KNIFE || id == IT_KUNAI))
    lo += int(std::lround((hi - lo) * (id == IT_KNIFE ? .75f : .6f)));
  return {lo, hi};
}
inline float durability_cost(uint8_t id, int level, int ring, int augment,
                             int effect) {
  float uses = base_uses(id) * float(std::pow(1.5, level));
  uses /= augment == 1 ? 2.f / 3.f : augment == 2 ? 5.f / 3.f : 1.f;
  uses *= float(std::pow(1.2, ring));
  if (effect == magic_rules::Crystal)
    uses = std::min(uses / 2, 30.f);
  return uses >= 100 ? 0.f : 100.f / std::max(1.f, std::round(uses)) + .001f;
}
inline bool sticky(uint8_t id) {
  return id != IT_THROWING_STONE && id != IT_THROWING_CLUB &&
         id != IT_THROWING_HAMMER && id != IT_FORCE_CUBE && id != IT_BOOMERANG;
}
} // namespace projectile_rules
