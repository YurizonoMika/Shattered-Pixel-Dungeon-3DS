#pragma once

// AscensionChallenge v4.0.2. GPL-3.0-or-later.
// Copyright (C) 2014-2026 Evan Debenham.
#include "content.h"
namespace ascent_rules {
inline float modifier(uint8_t type) {
  switch(type) {
    case M_RAT:case M_ALBINO:case M_FETID_RAT:return 10.f;
    case M_SNAKE:case M_GNOLL:case M_TRICKSTER:return 9.f;
    case M_SWARM:return 8.5f;
    case M_CRAB:case M_GREAT_CRAB:case M_SLIME:return 8.f;
    case M_SKELETON:case M_THIEF:return 5.f;
    case M_DM100:return 4.5f;
    case M_GUARD:case M_NECROMANCER:return 4.f;
    case M_BAT:return 2.5f;
    case M_BRUTE:case M_SHAMAN:return 2.25f;
    case M_SPINNER:case M_DM200:return 2.f;
    case M_GHOUL:case M_ELEMENTAL:return 1.67f;
    case M_WARLOCK:case M_MONK:return 1.5f;
    case M_GOLEM:return 1.33f;
    case M_RIPPER:case M_SUCCUBUS:return 1.2f;
    case M_EYE:case M_SCORPIO:return 1.1f;
    default:return 1.f;
  }
}
inline float kill_reduction(uint8_t type) {
  return modifier(type)==1.f ? 0.f : type==M_GHOUL||type==M_RIPPER ? .5f : 1.f;
}
} // namespace ascent_rules
