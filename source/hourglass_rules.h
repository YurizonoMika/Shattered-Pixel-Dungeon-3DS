#pragma once
// SPD v4.0.2 TimekeepersHourglass.timeFreeze. GPL-3.0-or-later.
// Copyright (C) 2014-2026 Evan Debenham.
namespace hourglass_rules {
inline int stasis_duration(int charge) { return 5 * (charge < 2 ? charge : 2); }
inline int stasis_hunger(int hunger, int charge, int starving = 450) {
  return hunger >= starving ? hunger : (hunger > stasis_duration(charge) ? hunger - stasis_duration(charge) : 0);
}
inline bool process(int &charge, float &turns_to_cost, float time) {
  turns_to_cost -= time;
  while (turns_to_cost < -.001f) {
    turns_to_cost += 2;
    --charge;
  }
  if (charge < 0 || (charge == 0 && turns_to_cost <= 0)) {
    charge = 0;
    return true;
  }
  return false;
}
}
