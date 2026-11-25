#pragma once
// SPD v4.0.2 Frost / Chill / Freezing.affect. GPL-3.0-or-later.
// Copyright (C) 2014-2026 Evan Debenham.
#include <algorithm>
namespace frost_rules {
inline float duration(bool water) { return water ? 30.f : 10.f; }
inline float speed(float chill) { return std::max(.5f, 1 - chill * .1f); }
inline void detach(float &frost, float &chill, bool water) {
  if (frost <= 0) return;
  frost = 0;
  if (water) chill = std::max(chill, 5.f);
}
}
