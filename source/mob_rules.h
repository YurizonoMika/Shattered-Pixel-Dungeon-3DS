#pragma once

// Base melee damage and XP caps, SPD v4.0.2. GPL-3.0-or-later.
// Copyright (C) 2014-2026 Evan Debenham. Special abilities are separate.
#include "content.h"
#include "hero_rules.h"

namespace mob_rules {
inline float movement_delay(uint8_t type) {
  return type == M_CRAB || type == M_BAT ? .5f : 1.f;
}
inline float attack_delay(uint8_t type) {
  return type == M_RIPPER || type == M_MONK || type == M_THIEF ? .5f : 1.f;
}
struct Base { int min, max, xp, max_level; };
inline Base base(uint8_t type, int depth, bool half_health = false) {
  switch (type) {
    case M_RAT: return {1,4,1,5};
    case M_ALBINO: return {1,4,2,5};
    case M_GNOLL: return {1,6,2,8};
    case M_CRAB: return {1,7,4,9};
    case M_SNAKE: return {1,4,2,7};
    case M_SWARM: return {1,4,3,9};
    case M_SLIME: return {2,5,4,9};
    case M_SKELETON: return {2,10,5,10};
    case M_THIEF: return {1,10,5,11};
    case M_DM100: return {2,8,6,13};
    case M_GUARD: return {4,12,7,14};
    case M_NECROMANCER: return {0,0,7,14};
    case M_BAT: return {5,18,7,15};
    case M_BRUTE: return {5,25,8,16};
    case M_SHAMAN: return {5,10,8,16};
    case M_SPINNER: return {10,20,9,17};
    case M_DM200: return {10,25,9,17};
    case M_GHOUL: return {16,22,5,20};
    case M_ELEMENTAL: return {20,25,10,20};
    case M_WARLOCK: return {12,18,11,21};
    case M_MONK: return {12,25,11,21};
    case M_GOLEM: return {25,30,12,22};
    case M_SUCCUBUS: return {25,30,12,25};
    case M_EYE: return {20,30,13,26};
    case M_SCORPIO: return {30,40,14,27};
    case M_RIPPER: return {15,25,0,-2};
    case M_WRAITH: return {1+depth/2,2+depth,0,-2};
    case M_MIMIC: return {1+depth,2+2*depth,0,29};
    case M_FETID_RAT: return {1,4,4,29};
    case M_TRICKSTER: return {1,6,5,29};
    case M_GREAT_CRAB: return {1,7,6,29};
    case M_GOO: return {1,half_health ? 12 : 8,10,29};
    case M_TENGU: return {6,12,20,29};
    case M_DM300: return {15,25,30,29};
    case M_KING: return {15,25,40,29};
    // Yog has no ordinary melee attack; native finale remains a placeholder.
    case M_YOG: return {0,0,50,29};
    default: return {0,0,0,-1};
  }
}
} // namespace mob_rules
