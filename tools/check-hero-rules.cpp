#include "hero_rules.h"
#include "mob_rules.h"
#include <cassert>
#include <cstdio>

int main() {
  using namespace hero_rules;
  assert(starting_hp == 20 && starting_strength == 10 && max_level == 30);
  int experience = 0;
  for (int level = 1; level < max_level; ++level) experience += max_exp(level);
  assert(experience == 2320);
  assert(food_energy(IT_FOOD_RATION) == 300 && food_energy(IT_FOOD_MEAT) == 150 &&
         food_energy(IT_FOOD_PASTY) == 450 && food_energy(IT_POTION_HEAL) == 0);
  const uint8_t armors[] = {IT_CLOTH_ARMOR,IT_LEATHER_ARMOR,IT_MAIL,IT_SCALE,IT_PLATE};
  for (int tier = 1; tier <= 5; ++tier) {
    assert(equipment_tier(armor_for_tier(tier)) == tier);
    for (int choice = 0; choice < 3; ++choice)
      assert(equipment_tier(weapon_for_tier(tier,choice)) == tier);
    for (int level = 0; level <= 30; ++level) {
      const auto dr = armor_range(armors[tier-1], level);
      assert(dr.min == level && dr.max == tier * (2+level));
      int reductions = 0;
      for (int n = 1; n*(n+1)/2 <= level; ++n) ++reductions;
      assert(strength_requirement(armors[tier-1],level) == 8+2*tier-reductions);
    }
  }
  const auto dagger = weapon_range(IT_DAGGER,0,10,false);
  const auto surprise = weapon_range(IT_DAGGER,0,10,true);
  assert(dagger.min == 1 && dagger.max == 8 && surprise.min == 6 && surprise.max == 8);
  const auto sword = weapon_range(IT_SHORTSWORD,0,12,false);
  assert(sword.min == 2 && sword.max == 15 && strength_requirement(IT_SHORTSWORD,0) == 12);
  assert(strength_requirement(IT_GREATAXE,0) == 20);
  assert(equipment_tier(IT_PICKAXE) == 2 && strength_requirement(IT_PICKAXE,0) == 14);
  assert(weapon_range(IT_PICKAXE,0,14,false).max == 15);
  assert(weapon_range(IT_GREATAXE,3,20,false).max == 63);
  assert(weapon_range(IT_RUNEBLADE,3,20,false).max == 38);
  assert(weapon_range(IT_HALBERD,3,20,false).max == 64);
  assert(weapon_reach(IT_WHIP) == 3 && weapon_reach(IT_SPEAR) == 2);
  assert(attack_delay(IT_SPEAR,0,12) == 1.5f);
  assert(attack_delay(IT_SCIMITAR,0,14) == .8f);
  assert(attack_delay(IT_GREATAXE,0,10) > 6.f);
  assert(movement_delay(IT_PLATE,0,10) > 4.f);
  assert(unarmed_force_range(10,1).min == 2 && unarmed_force_range(10,1).max == 12);
  assert(mob_rules::base(M_WRAITH,20).xp == 0);
  assert(mob_rules::base(M_RAT,1).max_level == 5);
  assert(mob_rules::base(M_GOO,5,true).max == 12);
  assert(mob_rules::base(M_DM200,11).max == 25);
  std::puts("Hero rule boundaries passed: XP curve, food, 155 armor ranges/STR thresholds, weapon ranges.");
}
