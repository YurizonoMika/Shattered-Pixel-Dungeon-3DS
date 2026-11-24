#pragma once
// SPD v4.0.2 enchantment/glyph catalogs and base formulas. GPL-3.0-or-later.
// Copyright (C) 2014-2026 Evan Debenham.
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace magic_rules {
enum Enchant : uint8_t {
  None,
  Blazing,
  Chilling,
  Kinetic,
  Shocking,
  Venomous,
  Blocking,
  Blooming,
  Eldritch,
  Elastic,
  Lucky,
  Projecting,
  Unstable,
  Vorpal,
  Corrupting,
  Crystal,
  Grim,
  Vampiric,
  Annoying,
  Displacing,
  Dazzling,
  Explosive,
  Friendly,
  Polarized,
  Pressurized,
  Sacrificial,
  Wayward,
  Wondrous,
  EnchantCount
};
enum Glyph : uint8_t {
  NoGlyph,
  Obfuscation,
  Swiftness,
  Viscosity,
  Potential,
  Brimstone,
  Stone,
  Entanglement,
  Repulsion,
  Camouflage,
  Flow,
  Affection,
  AntiMagic,
  Thorns,
  AntiEntropy,
  Corrosion,
  Displacement,
  Metabolism,
  Multiplicity,
  Stench,
  Overgrowth,
  Bulk,
  GlyphCount
};
inline const char* enchant_name(int id) {
  static const char* names[] = {"无附魔", "烈焰", "寒霜", "恒动", "电击", "淬毒", "招架",
                                "繁茂",   "骇人", "弹性", "幸运", "索敌", "紊乱", "血饮",
                                "腐化",   "晶化", "死神", "吸血", "喧闹", "驱逐", "炫目",
                                "爆炸",   "友好", "极化", "气压", "血祭", "妄为", "奇想"};
  return names[std::clamp(id, 0, int(EnchantCount) - 1)];
}
inline const char* glyph_name(int id) {
  static const char* names[] = {"无刻印", "晦暗", "迅捷", "粘稠", "电势", "磷火", "势大", "缠绕",
                                "斥力",   "迷彩", "涌动", "魅惑", "敌法", "荆棘", "反熵", "腐蚀",
                                "位移",   "代谢", "分身", "恶臭", "丛生", "臃肿"};
  return names[std::clamp(id, 0, int(GlyphCount) - 1)];
}
inline bool curse_enchant(int id) { return id >= Annoying && id < EnchantCount; }
inline bool curse_glyph(int id) { return id >= AntiEntropy && id < GlyphCount; }
inline float enchant_chance(int id, int level, float missing_health = 0.f) {
  const float l = std::max(0, level);
  switch (id) {
    case Blazing:
    case Blooming:
      return (l + 1) / (l + 3);
    case Chilling:
    case Eldritch:
      return (l + 1) / (l + 4);
    case Shocking:
    case Venomous:
      return 1.f / 3;
    case Blocking:
    case Lucky:
      return (l + 4) / (l + 40);
    case Elastic:
      return (l + 1) / (l + 5);
    case Vorpal:
      return .25f;
    case Corrupting:
      return (l + 5) / (l + 20);
    case Vampiric:
      return .05f + .25f * missing_health;
    case Grim:
      return .5f + .05f * l;
    case Annoying:
      return 1.f / 20;
    case Displacing:
      return 1.f / 12;
    case Dazzling:
    case Friendly:
    case Sacrificial:
      return .1f;
    case Polarized:
      return .5f;
    case Pressurized:
    case Wondrous:
      return .125f;
    case Wayward:
      return .25f;
    default:
      return 1.f;
  }
}
inline float glyph_chance(int id, int level) {
  const float l = std::max(0, level);
  switch (id) {
    case Potential:
      return (l + 1) / (l + 6);
    case Entanglement:
      return .25f;
    case Repulsion:
      return (l + 1) / (l + 5);
    case Affection:
      return (l + 3) / (l + 20);
    case Thorns:
      return (l + 2) / (l + 12);
    case AntiEntropy:
    case Stench:
      return .125f;
    case Corrosion:
      return .1f;
    case Displacement:
    case Multiplicity:
    case Overgrowth:
      return .05f;
    case Metabolism:
      return 1.f / 6;
    default:
      return 1.f;
  }
}
inline float arcana(int bonus) { return std::pow(1.175f, bonus); }
inline float stone_factor(float accuracy, float evasion) {
  float chance =
      evasion >= accuracy ? (evasion > 0 ? accuracy / evasion / 2 : .5f) : 1 - evasion / accuracy / 2;
  return std::clamp((1 + 3 * chance) / 4, .25f, 1.f);
}
struct State {
  uint8_t effect = 0;
  float durability = 100.f, stored = 0.f;
  int experience = 0, aux = 0, aux2 = 0;
  float cooldown = 0.f;
  uint32_t consumed = 0;
};
struct Effects {
  float burning = 0, chill = 0, poison = 0, poison_delay = 0, bleeding = 0, blind = 0, paralysis = 0,
        roots = 0, charm = 0;
  bool ally = false, stolen = false;
};
}  // namespace magic_rules
