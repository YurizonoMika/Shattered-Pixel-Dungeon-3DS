#pragma once
#include <cstddef>
#include <cstring>
#include <type_traits>
// Frozen V12/V13 native value layouts. Do not add fields to these structures.
namespace legacy_save {
struct Mob {
  int x = 0, y = 0;
  float vx = 0, vy = 0;
  int anim_ms = 0;
  uint8_t type = M_RAT;
  int hp = 1, maxhp = 1, atk = 1, def = 0;
  bool alive = true;
  uint8_t sleep_t = 0, terror_t = 0;
  ::Mob::Ai ai = ::Mob::Wandering;
  int target = -1;
  float next_action = 0;
  uint8_t variant = 0;
  bool beam_charged = false;
  uint8_t beam_cooldown = 0;
  int beam_target = -1;
  bool chains_used = false, focused = false;
  float focus_cooldown = 0;
  int minion = -1, summoning_pos = -1, owner = -1;
  bool first_summon = true, has_raged = false;
  int rage_shield = 0;
  uint8_t adrenaline = 0, pumped_up = 0;
  CombatPose combat;
};
struct FloorItem {
  int x = 0, y = 0;
  uint8_t id = IT_NONE;
  int16_t level = 0;
  uint8_t flags = IF_KNOWN, charges = 0;
  float partial_charge = 0;
  uint8_t augment = 0;
  Identification identification;
};
struct Item {
  uint8_t id = IT_NONE, count = 1, level = 0, charges = 0, flags = IF_KNOWN;
  float partial_charge = 0;
  uint8_t augment = 0;
  Identification identification;
};
// Appended members start after these frozen prefixes. Copy only the prefix;
// all new members retain their migration defaults.
template <class New, class Old>
New migrate(const Old& old) {
  New value{};
  static_assert(std::is_trivially_copyable<New>::value && std::is_trivially_copyable<Old>::value);
  static_assert(sizeof(New) >= sizeof(Old));
  std::memcpy(static_cast<void*>(&value), &old, sizeof(Old));
  return value;
}
static_assert(sizeof(Item) == 24 && sizeof(FloorItem) == 32);
static_assert(offsetof(::Item, magic) == sizeof(Item));
static_assert(offsetof(::FloorItem, magic) == sizeof(FloorItem));
static_assert(offsetof(::Mob, effects) == sizeof(Mob));
}  // namespace legacy_save
