#pragma once
#include <cstdint>
// Persistent state for independently applied plant/spell effects. Keep the
// V12-V14 entity prefixes frozen; this state lives in the V15 extension.
namespace mechanics_rules {
struct Actor {
  float vertigo = 0, bless = 0, haste = 0, levitation = 0, healing = 0,
        partial_heal = 0;
  float earth = 0, magic_immunity = 0, foresight = 0, artifact_recharge = 0;
  float holy_weapon = 0, holy_ward = 0, light_shield = 0, light = 0;
  int anchor = -1, shield_target = -1;
  bool illuminated = false, was_illuminated = false, sunray_used = false;
  float sunray_recent = 0;
  float dread = 0, prismatic = 0, arena = 0;
  int arena_cell = -1;
  float aura = 0, divine = 0, divine_left = 0, wall = 0, lifelink = 0;
  int aura_rank = 0, shield_rank = 0;
  float wayward = 0, shuriken_cooldown = 0, body_left = 0, spirit_left = 0;
  uint8_t body_effect = 0, spirit_ring = 0;
  bool body_armor = false;
  float shield = 0;
  float regen_partial = 0, mirror = 0, prismatic_guard = 0, cleanse = 0;
  float hallowed = 0;
  int hallowed_cell = -1, hallowed_rank = 0;
  float power_left = 0;
  bool light_ally = false;
  float deferred = 0, kinetic = 0;
  float wand_recharge = 0, drowsy = 0, amok = 0;
  bool magical_sleep = false;
  bool satiated_spell = false;
  float body_durability = 100;
  float scry = 0;
  int earth_anchor = -1;
  float cripple = 0, slow = 0, weakness = 0, vulnerable = 0;
  float time_bubble = 0;
  bool bubble_active = false;
};
struct HeapAwareness {
  int depth = 0, cell = 0;
  float left = 0;
};
} // namespace mechanics_rules
