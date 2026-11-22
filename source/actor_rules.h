#pragma once

// Scheduling rules from SPD v4.0.2 Actor: earliest time, then descending
// priority. The hero (priority 0) takes input before other actors at a tie.
// GPL-3.0-or-later. Copyright (C) 2014-2026 Evan Debenham.
#include <cmath>

namespace actor_rules {
constexpr int hero_priority = 0;
constexpr int health_priority = -1;
constexpr int blob_priority = -10;
constexpr int mob_priority = -20;
constexpr int buff_priority = -30;

inline float spend(float time, float duration) {
  time += duration;
  // Match Actor.spendConstant, including its one-sided remainder test.
  if (std::fabs(std::fmod(time, 1.f)) < .001f) time = std::round(time);
  return time;
}

struct Event {
  float time;
  int priority;
  int id;
};
inline bool before(const Event& candidate, const Event& selected) {
  return candidate.time < selected.time ||
      (candidate.time == selected.time && candidate.priority > selected.priority);
}
} // namespace actor_rules
