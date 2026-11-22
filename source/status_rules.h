#pragma once

// SPD v4.0.2 base debuff constants/formulas. GPL-3.0-or-later.
// Copyright (C) 2014-2026 Evan Debenham.
#include <algorithm>
#include <cmath>
namespace status_rules {
enum Type { Poison, Cripple, Weakness, Vulnerable, Hex, Degrade, Ooze, Count };
inline int duration(Type type) {
  switch (type) {
    case Cripple: return 10;
    case Weakness: case Vulnerable: return 20;
    case Hex: case Degrade: return 30;
    default: return 0;
  }
}
inline int degraded_level(int level) {
  return level <= 0 ? level : int(std::floor(std::sqrt(2.f*(level-1))+1.5f));
}
inline int poison_damage(int left) { return left/3+1; }
} // namespace status_rules
