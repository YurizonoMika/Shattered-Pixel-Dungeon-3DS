#pragma once
// SPD v4.0.2 Swiftthistle.TimeBubble. GPL-3.0-or-later.
// Copyright (C) 2014-2026 Evan Debenham.
namespace time_bubble_rules {
inline float reset(int turns = 6) { return turns + 1.f; }
inline bool process(float &left, float time) {
  left -= time;
  return left < -.001f;
}
}
