#pragma once
// v4.0.2 ClericSpell.getAllSpells / Talent tier gates. GPL-3.0-or-later.
#include <algorithm>
namespace cleric_rules {
enum Spell {
  GuidingLight,
  HolyWeapon,
  HolyWard,
  HolyIntuition,
  ShieldOfLight,
  RecallInscription,
  Sunray,
  DivineSense,
  Bless,
  Cleanse,
  Radiance,
  Smite,
  LayOnHands,
  AuraOfProtection,
  WallOfLight,
  HolyLance,
  HallowedGround,
  MnemonicPrayer,
  DivineIntervention,
  Judgement,
  Flash,
  BodyForm,
  MindForm,
  SpiritForm,
  BeamingRay,
  LifeLink,
  Stasis,
  SatiatedSpells,
  SearingLight,
  EnlighteningMeal,
  LightReading,
  HeroicEnergy,
  Count
};
constexpr const char *names[] = {
    "引导之光", "神圣武器", "神圣护甲", "神圣直觉", "光之护盾", "铭文召回",
    "阳光射线", "神圣感知", "祝福术",   "净化",     "光辉",     "惩击",
    "圣疗",     "守护光环", "光之墙",   "神圣长枪", "圣地",     "记忆祷言",
    "神圣干预", "审判",     "闪现",     "躯体形态", "心智形态", "灵体形态",
    "光束传送", "生命联结", "停滞",     "饱食施法", "灼热之光", "启迪一餐",
    "轻便阅读", "英勇能量"};
inline int tier(int spell) {
  if (spell == SatiatedSpells || spell == SearingLight)
    return 1;
  if (spell == EnlighteningMeal)
    return 2;
  if (spell == LightReading)
    return 3;
  if (spell <= ShieldOfLight)
    return 1;
  if (spell <= Bless)
    return 2;
  if (spell <= MnemonicPrayer)
    return 3;
  return 4;
}
inline int rank_cap(int spell) {
  return tier(spell) == 1 || tier(spell) == 2 ? 2 : tier(spell) == 3 ? 3 : 4;
}
inline int cost(int spell, int rank = 1) {
  switch (spell) {
  case HolyWeapon:
  case DivineSense:
  case Cleanse:
  case Radiance:
  case Smite:
  case AuraOfProtection:
  case HallowedGround:
  case Flash:
  case BodyForm:
  case LifeLink:
  case Stasis:
    return 2;
  case HolyIntuition:
    return 4 - rank;
  case HolyLance:
  case SpiritForm:
    return 4;
  case DivineIntervention:
    return 5;
  case Judgement:
  case WallOfLight:
  case MindForm:
    return 3;
  default:
    return 1;
  }
}
inline int branch(int spell) {
  switch (spell) {
  case Radiance:
  case HolyLance:
  case HallowedGround:
  case MnemonicPrayer:
    return 1; // priest
  case Smite:
  case LayOnHands:
  case AuraOfProtection:
  case WallOfLight:
    return 2; // paladin
  default:
    return 0;
  }
}
inline int ability(int spell) {
  if (spell >= SatiatedSpells)
    return 0;
  return spell < DivineIntervention ? 0
         : spell <= Flash           ? 1
         : spell <= SpiritForm      ? 2
                                    : 3;
}
inline bool innate(int spell) {
  return spell <= HolyWard || spell == Radiance || spell == Smite;
}
inline int available(int tier, int level, int spent) {
  constexpr int starts[] = {0, 2, 7, 13, 21, 31};
  return std::max(0,
                  std::min(level + 1, starts[tier + 1]) - starts[tier] - spent);
}
} // namespace cleric_rules
