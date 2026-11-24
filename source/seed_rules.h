#pragma once
// v4.0.2 SandalsOfNature seed catalog and costs. GPL-3.0-or-later.
#include "items.h"
#include "mechanics_rules.h"
#include "magic_rules.h"
namespace seed_rules {
constexpr float blessing_duration = 30, blindness_duration = 10, cripple_duration = 10;
inline void cure(magic_rules::Effects &status, mechanics_rules::Actor &effect) {
  status.poison = status.poison_delay = status.bleeding = status.blind = 0;
  effect.cripple = effect.slow = effect.weakness = effect.vulnerable = 0;
  effect.drowsy = effect.vertigo = 0;
}
inline int absorb(float &pool, int damage, int depth) {
  if (pool <= 0 || damage <= 0) return damage;
  const int block = std::min(damage, (depth + 5) / 2);
  pool = std::max(0.f, pool - block);
  return damage - block;
}
inline int heal_tick(float &pool, float &partial, int maximum) {
  if (pool <= 0) return 0;
  partial += (40 + maximum) / 150.f;
  if (partial <= 1) return 0;
  const int amount = int(partial);
  partial -= amount;
  pool = std::max(0.f, pool - amount);
  return amount;
}
constexpr uint8_t ids[] = {
    IT_SEED_ROTBERRY,   IT_SEED_FIREBLOOM, IT_SEED_SWIFTTHISTLE,
    IT_SEED_SUNGRASS,   IT_SEED_ICECAP,    IT_SEED_STORMVINE,
    IT_SEED_SORROWMOSS, IT_SEED_MAGEROYAL, IT_SEED_EARTHROOT,
    IT_SEED_STARFLOWER, IT_SEED_FADELEAF,  IT_SEED_BLINDWEED};
inline int index(uint8_t id) {
  for (int i = 0; i < 12; ++i)
    if (ids[i] == id)
      return i;
  return -1;
}
inline int cost(uint8_t id) {
  constexpr int costs[] = {8, 20, 20, 80, 20, 20, 20, 12, 40, 40, 12, 12};
  int i = index(id);
  return i < 0 ? 0 : costs[i];
}
} // namespace seed_rules
