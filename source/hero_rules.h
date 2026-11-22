#pragma once

// Base Shattered Pixel Dungeon v4.0.2 rules, cross-checked with
// pixeldungeon.wiki (Hero, Food, Equipment, Armor, Potion of Strength).
// GPL-3.0-or-later. Upstream copyright 2014-2026 Evan Debenham.
#include <algorithm>
#include <cmath>
#include "items.h"

namespace hero_rules {
constexpr int starting_strength = 10;
constexpr int starting_hp = 20;
constexpr int max_level = 30;
constexpr int hungry = 300;
constexpr int starving = 450;
constexpr int regeneration_period = 10;
inline int max_exp(int level) { return 5 + level * 5; }

inline int food_energy(uint8_t id) {
  return id == IT_FOOD_RATION ? 300 : id == IT_FOOD_PASTY ? 450 : id == IT_FOOD_MEAT ? 150 : 0;
}

inline int equipment_tier(uint8_t id) {
  switch (id) {
    case IT_DAGGER: case IT_CUDGEL: case IT_CLOTH_ARMOR: return 1;
    case IT_SHORTSWORD: case IT_HAND_AXE: case IT_SPEAR: case IT_PICKAXE: case IT_LEATHER_ARMOR: return 2;
    case IT_SCIMITAR: case IT_WHIP: case IT_MAIL: return 3;
    case IT_LONGSWORD: case IT_BATTLEAXE: case IT_RUNEBLADE: case IT_SCALE: return 4;
    case IT_GREATAXE: case IT_WARHAMMER: case IT_GREATSWORD: case IT_HALBERD: case IT_PLATE: return 5;
    // Legacy items have no direct upstream counterpart; do not call these parity.
    case IT_GIANT_HAMMER: return 5;
    case IT_RUNE_ARMOR: return 5;
    default: return 0;
  }
}

inline uint8_t armor_for_tier(int tier) {
  constexpr uint8_t ids[] = {IT_CLOTH_ARMOR,IT_LEATHER_ARMOR,IT_MAIL,IT_SCALE,IT_PLATE};
  return ids[std::clamp(tier,1,5)-1];
}

inline uint8_t weapon_for_tier(int tier, int choice) {
  // The supported subset of each real tier. No invented item fills a gap.
  constexpr uint8_t ids[5][3] = {
    {IT_DAGGER,IT_CUDGEL,IT_DAGGER}, {IT_SHORTSWORD,IT_HAND_AXE,IT_SPEAR},
    {IT_SCIMITAR,IT_WHIP,IT_SCIMITAR}, {IT_LONGSWORD,IT_BATTLEAXE,IT_RUNEBLADE},
    {IT_GREATSWORD,IT_WARHAMMER,IT_HALBERD}
  };
  return ids[std::clamp(tier,1,5)-1][std::clamp(choice,0,2)];
}

inline int strength_requirement(uint8_t id, int level) {
  int tier = equipment_tier(id);
  if (!tier) return 0;
  if (id == IT_GREATAXE || id == IT_PICKAXE) ++tier;
  const int reduction = int((std::sqrt(8.0 * std::max(0, level) + 1) - 1) / 2);
  return 8 + 2 * tier - reduction;
}

struct DamageRange { int min, max; };
inline DamageRange armor_range(uint8_t id, int level) {
  const int tier = equipment_tier(id);
  if (!tier) return {0,0};
  const int raw_max = tier * (2+level);
  const int max = level > raw_max ? (level-raw_max+1)/2 : raw_max;
  return {level >= max ? level-max : level,max};
}

inline DamageRange weapon_range(uint8_t id, int level, int strength, bool surprise) {
  const int tier = equipment_tier(id);
  if (!tier) return {1, std::max(strength - 8, 1)};
  int min = tier + level, max = 5 * (tier + 1) + level * (tier + 1);
  switch (id) {
    case IT_DAGGER: case IT_CUDGEL: case IT_HAND_AXE: case IT_SCIMITAR:
    case IT_BATTLEAXE: case IT_WARHAMMER:
      max = 4 * (tier + 1) + level * (tier + 1); break;
    case IT_GREATAXE: max = 45 + level * 6; break;
    case IT_RUNEBLADE: max = 20 + level * 6; break;
    case IT_SPEAR: max = 20 + level * 4; break;
    case IT_HALBERD: max = 40 + level * 8; break;
    case IT_WHIP: max = 15 + level * 3; break;
    default: break;
  }
  if (id == IT_DAGGER && surprise) min += int(std::floor((max-min) * .75f + .5f));
  return {min,max};
}

inline DamageRange unarmed_force_range(int strength, int bonus) {
  float tier = std::max(1.f, (strength-8)/2.f);
  if (tier > 5) tier = 5 + (tier-5)/2.f;
  return {int(std::floor(tier+bonus+.5f)),
          int(std::floor(5*(tier+1) + bonus*(tier+1) + .5f))};
}

inline float weapon_accuracy(uint8_t id) {
  switch (id) {
    case IT_CUDGEL: return 1.40f;
    case IT_HAND_AXE: return 1.32f;
    case IT_BATTLEAXE: return 1.24f;
    case IT_WARHAMMER: return 1.20f;
    default: return 1.f;
  }
}

inline int weapon_reach(uint8_t id) {
  return id == IT_WHIP ? 3 : id == IT_SPEAR || id == IT_HALBERD ? 2 : 1;
}

inline float attack_delay(uint8_t id, int level, int strength) {
  const int missing = std::max(0, strength_requirement(id,level) - strength);
  const float delay = id == IT_SPEAR || id == IT_HALBERD ? 1.5f : id == IT_SCIMITAR ? .8f : 1.f;
  return delay * std::pow(1.2f, missing);
}

inline float movement_delay(uint8_t armor, int level, int strength) {
  return std::pow(1.2f, std::max(0, strength_requirement(armor,level) - strength));
}
} // namespace hero_rules
