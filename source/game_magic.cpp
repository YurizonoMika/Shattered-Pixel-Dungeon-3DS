#include "artifact_rules.h"
#include "foresight_rules.h"
#include "hourglass_rules.h"
#include "frost_rules.h"
#include "game.h"
#include "hero_rules.h"
#include "seed_rules.h"
#include <algorithm>
#include <cmath>

using namespace magic_rules;
namespace {
uint32_t draw(Game &g) {
  g.rng ^= g.rng << 13;
  g.rng ^= g.rng >> 17;
  g.rng ^= g.rng << 5;
  return g.rng;
}
float uniform(Game &g) { return (draw(g) >> 8) * (1.f / 16777216.f); }
int integer(Game &g, int a, int b) {
  return b < a ? a : a + draw(g) % uint32_t(b - a + 1);
}
int rounded(float v) { return int(std::floor(v + .5f)); }
int normal(Game &g, int a, int b) {
  return a + int((uniform(g) + uniform(g)) * (b - a + 1) * .5f);
}
int distance(int x, int y, int tx, int ty) {
  return std::max(std::abs(x - tx), std::abs(y - ty));
}
bool npc(const Mob &m) { return m.type >= M_NPC_GHOST && m.type <= M_NPC_IMP; }
void add_charge(State &state, float amount, int capacity) {
  state.stored += amount;
  while (state.stored >= 1 && state.aux < capacity) {
    ++state.aux;
    --state.stored;
  }
  if (state.aux >= capacity)
    state.stored = 0;
}
int seed_cost(int id) { return seed_rules::cost(id); }
} // namespace

float Game::enchant_power() const {
  return magic_actor >= 0
             ? 1.f
             : arcana(std::clamp(accessory_bonus(IT_RING_ARCANA), -4, 30));
}
int Game::random_enchant(bool arm, bool cursed, int rarity, int exclude) {
  static const int weapons[3][8] = {
      {Blazing, Chilling, Kinetic, Shocking, Venomous},
      {Blocking, Blooming, Eldritch, Elastic, Lucky, Projecting, Unstable,
       Vorpal},
      {Corrupting, Crystal, Grim, Vampiric}};
  static const int armors[3][6] = {
      {Obfuscation, Swiftness, Viscosity, Potential},
      {Brimstone, Stone, Entanglement, Repulsion, Camouflage, Flow},
      {Affection, AntiMagic, Thorns}};
  if (cursed)
    return integer(*this, arm ? int(AntiEntropy) : int(Annoying),
                   arm ? GlyphCount - 1 : EnchantCount - 1);
  if (rarity < 0) {
    const int value = integer(*this, 0, 99);
    rarity = value < 50 ? 0 : value < 90 ? 1 : 2;
  }
  rarity = std::clamp(rarity, 0, 2);
  const int counts[2][3] = {{5, 8, 4}, {4, 6, 3}};
  const int *group = arm ? armors[rarity] : weapons[rarity];
  const int count = counts[arm][rarity];
  const int excluded = std::find(group, group + count, exclude) - group;
  const int index = integer(*this, 0, count - 1 - (excluded < count));
  return group[index + (index >= excluded)];
}
void Game::enchant_item(int target, int effect) {
  Item item = selection_item(target);
  const auto kind = item_def(item.id).kind;
  if (effect <= 0 || ((kind == K_WEAPON || kind == K_MISSILE)
                          ? effect >= Annoying
                          : kind != K_ARMOR || effect >= AntiEntropy))
    return;
  int source = -1;
  for (int i = 0; i < inv_n; ++i)
    if (inv[i].id == selection_scroll) {
      source = i;
      break;
    }
  const bool free = book_empowered && book_scroll == selection_scroll;
  if (source < 0 && !free)
    return;
  const int set_id = item.magic.aux2;
  const float durability = item.magic.durability;
  item.magic = {};
  if (kind == K_MISSILE) {
    item.magic.aux2 = set_id;
    item.magic.durability = durability;
  }
  item.magic.effect = effect;
  discover_item(item);
  if (target >= 0)
    inv[target] = item;
  else if (target == -1)
    weapon_magic = item.magic;
  else if (target == -2)
    armor_magic = item.magic;
  remember_inscription(selection_scroll);
  if (free) {
    book_scroll = IT_NONE;
    book_empowered = inscription_replay = false;
  } else
    remove_item(source);
  transmute_open = false;
  message("装备获得了新的魔法效果。");
  spend_action(1.f);
}
void Game::push_mob(int index, int from_x, int from_y, int amount) {
  if (index < 0 || index >= int(mobs.size()) || !mobs[index].alive ||
      mob_def(mobs[index].type).boss)
    return;
  auto &mob = mobs[index];
  const int dx = (mob.x > from_x) - (mob.x < from_x),
            dy = (mob.y > from_y) - (mob.y < from_y);
  for (int n = 0; n < amount; ++n) {
    int x = mob.x + dx, y = mob.y + dy;
    if (!walkable(x, y) || mob_at(x, y) >= 0 || (x == px && y == py))
      break;
    mob.x = x;
    mob.y = y;
  }
  mob.vx = mob.x;
  mob.vy = mob.y;
}
void Game::teleport_mob(int index) {
  if (index < 0 || index >= int(mobs.size()) || mob_def(mobs[index].type).boss)
    return;
  for (int n = 0; n < 200; ++n) {
    int x = integer(*this, 1, map_width - 2),
        y = integer(*this, 1, map_height - 2);
    if (walkable(x, y) && mob_at(x, y) < 0 && !(x == px && y == py)) {
      auto &m = mobs[index];
      m.x = m.vx = x;
      m.y = m.vy = y;
      m.ai = Mob::Wandering;
      break;
    }
  }
}
int Game::melee_reach() const {
  return hero_rules::weapon_reach(weapon) +
         (weapon_magic.effect == Projecting
              ? std::max(1, rounded(enchant_power()))
              : 0);
}
float Game::movement_cost(int x, int y) const {
  if (!in_map(x, y)) {
    x = px;
    y = py;
  }
  float delay =
      hero_rules::movement_delay(armor, armor_level, effective_strength()) /
      std::pow(1.175f, accessory_bonus(IT_RING_HASTE));
  if (haste_t || spell_effects.haste > 0)
    delay /= 3;
  if (debuffs[status_rules::Cripple] || spell_effects.cripple > 0)
    delay *= 2;
  delay *= spell_effects.slow > 0 ? 2.f : 1.f/frost_rules::speed(hero_effects.chill);
  const int effect = armor_magic.effect, l = buffed_level(armor_level);
  const float power = enchant_power();
  if (effect == Flow && tiles[y][x] == T_WATER)
    delay /= std::max(.1f, (2 + .5f * l) * power);
  if (effect == Bulk && (tiles[y][x] == T_DOOR || tiles[y][x] == T_DOOR_OPEN))
    delay *= 3 / power;
  if (effect == Swiftness) {
    bool nearby = false;
    for (const auto &m : mobs)
      if (m.alive && !npc(m) && !m.effects.ally &&
          distance(x, y, m.x, m.y) <= 3 && los(x, y, m.x, m.y))
        nearby = true;
    if (!nearby)
      delay /= std::max(.1f, (1.2f + .04f * l) * power);
  }
  if (ascending && ascent_stacks >= 6)
    delay = std::max(1.f, delay * 2);
  return delay;
}

int Game::weapon_proc(int target, int damage, int &enchantment) {
  if (target < 0 || target >= int(mobs.size()) || !mobs[target].alive ||
      spell_effects.magic_immunity > 0)
    return damage;
  const bool holy = spell_effects.holy_weapon > 0 && !projectile_proc;
  if (!holy || cleric_subclass == 2 || curse_enchant(weapon_magic.effect))
    damage = weapon_enchant_proc(target, damage, enchantment);
  if (mobs[target].alive && !projectile_proc && spell_effects.body_left > 0 &&
      !spell_effects.body_armor && spell_effects.body_effect &&
      spell_effects.body_effect != weapon_magic.effect) {
    const auto original = weapon_magic;
    const auto original_weapon = weapon, original_level = weapon_level,
               original_augment = weapon_augment, original_flags = weapon_flags;
    const auto original_identification = weapon_identification;
    weapon_magic = {};
    weapon_magic.effect = spell_effects.body_effect;
    weapon_magic.durability = spell_effects.body_durability;
    const bool original_extra = extra_proc;
    extra_proc = true;
    damage = weapon_enchant_proc(target, damage, enchantment);
    spell_effects.body_durability = weapon_magic.durability;
    extra_proc = original_extra;
    weapon_magic = original;
    weapon = original_weapon;
    weapon_level = original_level;
    weapon_augment = original_augment;
    weapon_flags = original_flags;
    weapon_identification = original_identification;
  }
  if (mobs[target].alive && holy)
    damage_mob(target, int(std::lround((cleric_subclass == 2 ? 6 : 2) *
                                       enchant_power())));
  return damage;
}
int Game::weapon_enchant_proc(int target, int damage, int &enchantment) {
  if (target < 0 || target >= int(mobs.size()) || damage <= 0 ||
      !mobs[target].alive)
    return damage;
  auto &m = mobs[target];
  if (spell_effects.magic_immunity > 0)
    return damage;
  int effect = weapon_magic.effect, l = buffed_level(weapon_level);
  const float power = enchant_power();
  if (effect == Unstable) {
    const int choices[] = {Blazing,    Blocking, Blooming, Chilling, Kinetic,
                           Corrupting, Elastic,  Grim,     Lucky,    Shocking,
                           Vampiric,   Venomous, Vorpal,   Eldritch};
    effect = choices[integer(*this, 0, 13)];
  }
  if (effect == None || effect == Projecting)
    return damage;
  if (effect == Kinetic || effect == Grim || effect == Corrupting) {
    enchantment |= 1 << effect;
    if (effect == Kinetic)
      damage += int(std::ceil(kinetic_damage));
    return damage;
  }
  if (effect == Crystal) {
    const float uses = 50 - std::min(20.f, 20 * l / 6.f);
    if (!projectile_proc)
      weapon_magic.durability -=
          (.5f + (uniform(*this) + uniform(*this)) * .5f) * 100.f / uses *
          melee_delay();
    damage_mob(target, int(std::ceil(damage * .33f * power)));
    if (weapon_magic.durability <= 0) {
      if (extra_proc) {
        spell_effects.body_left = 0;
        weapon_magic.durability = 0;
        return damage;
      }
      weapon = IT_NONE;
      weapon_level = weapon_augment = 0;
      weapon_flags = IF_KNOWN;
      weapon_magic = {};
      weapon_identification = {};
      message("晶化武器碎裂了！");
    }
    return damage;
  }
  if (effect == Explosive) {
    weapon_magic.durability -=
        rounded(integer(*this, 0, 10) * std::min(10.f, power));
    if (weapon_magic.durability <= 0) {
      weapon_magic.durability += 100;
      for (size_t i = 0; i < mobs.size(); ++i)
        if (mobs[i].alive && !npc(mobs[i]) &&
            distance(m.x, m.y, mobs[i].x, mobs[i].y) <= 1)
          damage_mob(i, normal(*this, 5 + floor, 10 + 2 * floor));
      if (distance(px, py, m.x, m.y) <= 1)
        take_damage(normal(*this, 5 + floor, 10 + 2 * floor));
    }
    return damage;
  }
  float chance = enchant_chance(effect, l, float(maxhp - hp) / maxhp) * power;
  if (effect == Wayward && spell_effects.wayward > 0) {
    spell_effects.wayward = 0;
    return damage;
  }
  if (effect == Lucky)
    m.lucky = false;
  if (uniform(*this) >= chance)
    return effect == Polarized ? 0 : damage;
  const float scale = std::max(1.f, chance);
  switch (effect) {
  case Blazing:
    if (m.effects.burning <= 0)
      m.effects.burning = 8;
    else
      damage_mob(target,
                 rounded(normal(*this, 1, 3 + floor / 4) * .67f * scale));
    break;
  case Chilling:
    if(actor_effects(target).frost<=0) {
      m.effects.burning=0;
      m.effects.chill = std::min(6.f * scale, m.effects.chill + 3 * scale);
    }
    break;
  case Venomous:
    if (m.effects.poison <= 0)
      m.effects.poison_delay = 3;
    m.effects.poison += scale * (l / 2.f + 3);
    break;
  case Blocking:
    shield = std::max(shield, rounded(scale * (2 + l)));
    break;
  case Blooming: {
    int plants = int(1.5f + .1f * l);
    if (uniform(*this) < std::fmod(1.5f + .1f * l, 1.f))
      ++plants;
    for (int y = m.y - 1; y <= m.y + 1 && plants; ++y)
      for (int x = m.x - 1; x <= m.x + 1 && plants; ++x)
        if (in_map(x, y) &&
            (tiles[y][x] == T_FLOOR || tiles[y][x] == T_GRASS)) {
          tiles[y][x] = T_HIGH_GRASS;
          --plants;
        }
    break;
  }
  case Shocking: {
    std::vector<int> affected{target};
    for (size_t n = 0; n < affected.size(); ++n) {
      const auto source = mobs[affected[n]];
      for (size_t i = 0; i < mobs.size(); ++i)
        if (mobs[i].alive && !npc(mobs[i]) && !mobs[i].effects.ally &&
            std::find(affected.begin(), affected.end(), int(i)) ==
                affected.end() &&
            distance(source.x, source.y, mobs[i].x, mobs[i].y) <=
                (n == 0 || tiles[source.y][source.x] == T_WATER ? 2 : 1) &&
            los(source.x, source.y, mobs[i].x, mobs[i].y))
          affected.push_back(i);
    }
    for (size_t n = 1; n < affected.size(); ++n)
      damage_mob(affected[n], rounded(damage * .5f * scale));
    break;
  }
  case Eldritch:
    for (auto &other : mobs)
      if (other.alive && !npc(other) && &other != &m &&
          (los(other.x, other.y, px, py) || los(other.x, other.y, m.x, m.y)))
        other.terror_t = std::max(int(other.terror_t), rounded(5 * scale));
    break;
  case Elastic:
    push_mob(target, px, py, rounded(2 * scale));
    break;
  case Lucky:
    m.lucky = true;
    break;
  case Vorpal:
    m.effects.bleeding = std::max(m.effects.bleeding, damage * .33f * scale);
    break;
  case Vampiric:
    hp = std::min(maxhp, hp + rounded(damage * .5f * scale));
    break;
  case Annoying:
    invis_t = 0;
    for (auto &other : mobs)
      if (other.alive && !npc(other)) {
        other.ai = Mob::Hunting;
        other.target = py * MW + px;
      }
    break;
  case Displacing:
    teleport_mob(target);
    break;
  case Dazzling:
    hero_effects.blind = 10;
    for (auto &other : mobs)
      if (los(other.x, other.y, m.x, m.y))
        other.effects.blind = 5;
    break;
  case Friendly:
    hero_effects.charm = 20;
    m.effects.charm = 10;
    break;
  case Polarized:
    return rounded(damage * 1.5f);
  case Pressurized:
    if (tiles[m.y][m.x] == T_FLOOR || tiles[m.y][m.x] == T_GRASS ||
        tiles[m.y][m.x] == T_HIGH_GRASS)
      tiles[m.y][m.x] = T_WATER;
    push_mob(target, px, py, 2);
    break;
  case Sacrificial:
    hero_effects.bleeding = std::max(1.f, float(hp) * hp / maxhp / 8.f);
    break;
  case Wayward:
    spell_effects.wayward = 10;
    break;
  case Wondrous: {
    switch (integer(*this, 0, 5)) {
    case 0:
      teleport_mob(target);
      break;
    case 1:
      hp = std::min(maxhp, hp + 10);
      break;
    case 2:
      take_damage(10);
      break;
    case 3:
      m.sleep_t = 10;
      break;
    case 4:
      toxic_gas.seed(py * MW + px, 250);
      break;
    case 5:
      m.effects.burning = 8;
      break;
    }
    break;
  }
  default:
    break;
  }
  return damage;
}

int Game::weapon_final_damage(Mob &enemy, int damage, int enchantment) {
  const int level = buffed_level(weapon_level);
  const float power = enchant_power();
  if (enchantment & (1 << Kinetic)) {
    const int excess =
        std::max(0, damage - enemy.hp - int(std::ceil(kinetic_damage)));
    kinetic_damage = std::min(10000000.f, excess * power);
  }
  if (!mob_def(enemy.type).boss) {
    const float chance = enchant_chance(Grim, level) * power;
    if ((enchantment & (1 << Grim)) && damage < enemy.hp) {
      const float missing =
          1.f - float(enemy.hp - damage) / std::max(1, enemy.maxhp);
      if (uniform(*this) < chance * missing * missing)
        return enemy.hp;
    }
    if ((enchantment & (1 << Corrupting)) && damage >= enemy.hp &&
        uniform(*this) < enchant_chance(Corrupting, level) * power) {
      enemy.effects = {};
      enemy.effects.ally = true;
      enemy.hp = enemy.maxhp;
      enemy.ai = Mob::Hunting;
      return 0;
    }
  }
  return damage;
}

int Game::armor_proc(Mob &attacker, int damage, bool magic) {
  if (time_stasis && magic_actor < 0) return 0;
  if (spell_effects.magic_immunity > 0)
    return damage;
  const bool holy = spell_effects.holy_ward > 0 && !magic;
  if (!holy || cleric_subclass == 2 || curse_glyph(armor_magic.effect))
    damage = armor_glyph_proc(attacker, damage, magic);
  if (spell_effects.body_left > 0 && spell_effects.body_armor &&
      spell_effects.body_effect &&
      spell_effects.body_effect != armor_magic.effect) {
    const auto original = armor_magic;
    armor_magic = {};
    armor_magic.effect = spell_effects.body_effect;
    damage = armor_glyph_proc(attacker, damage, magic);
    armor_magic = original;
  }
  if (holy)
    damage =
        std::max(0, damage - int(std::lround((cleric_subclass == 2 ? 3 : 1) *
                                             enchant_power())));
  return damage;
}
int Game::armor_glyph_proc(Mob &attacker, int damage, bool magic) {
  if (spell_effects.magic_immunity > 0)
    return damage;
  const int effect = armor_magic.effect, l = buffed_level(armor_level);
  const float power = enchant_power();
  if (effect == NoGlyph || damage <= 0)
    return damage;
  if (magic) {
    if (effect == AntiMagic)
      damage = std::max(0, damage - normal(*this, rounded(l * power),
                                           rounded((3 + 1.5f * l) * power)));
    return damage;
  }
  if (effect == Stone)
    return int(std::ceil(
        damage *
        stone_factor(mob_def(attacker.type).accuracy * mob_modifier(attacker),
                     std::max(0.f, hero_evasion()) * power)));
  if (effect == Viscosity) {
    const float percent = (l + 1.f) / (l + 6.f) * power;
    if (percent > 1)
      damage = int(std::lround(damage / percent));
    const int amount = percent > 1 ? damage : int(std::ceil(damage * percent));
    deferred_damage += amount;
    return damage - amount;
  }
  if (effect == Brimstone || effect == Flow || effect == Swiftness ||
      effect == Bulk || effect == Obfuscation || effect == Camouflage ||
      effect == AntiMagic)
    return damage;
  if (uniform(*this) >= glyph_chance(effect, l) * power)
    return damage;
  const float scale = std::max(1.f, glyph_chance(effect, l) * power);
  const int index = int(&attacker - mobs.data());
  switch (effect) {
  case Potential:
    if (magic_actor >= 0)
      break;
    for (int i = 0; i < inv_n; ++i)
      if (item_def(inv[i].id).kind == K_WAND)
        inv[i].charges = std::min(wand_capacity(inv[i]),
                                  int(inv[i].charges) + rounded(scale));
    break;
  case Entanglement:
    shield = std::max(shield, rounded((5 + 2 * l) * scale));
    hero_effects.roots = 5;
    break;
  case Repulsion:
    if (distance(px, py, attacker.x, attacker.y) == 1)
      push_mob(index, px, py, rounded(2 * scale));
    break;
  case Affection:
    attacker.effects.charm = rounded(20 * scale);
    break;
  case Thorns:
    attacker.effects.bleeding = rounded((4 + l) * scale);
    break;
  case AntiEntropy:
    hero_effects.burning = tiles[py][px] == T_WATER ? 0 : 4;
    for (auto &mob : mobs)
      if (distance(px, py, mob.x, mob.y) <= 1)
        mob.effects.chill = 6;
    break;
  case Corrosion:
    if (magic_actor < 0)
      debuffs[status_rules::Ooze] = 10;
    for (auto &mob : mobs)
      if (distance(px, py, mob.x, mob.y) <= 1)
        mob.effects.poison = 10;
    break;
  case Displacement:
    for (int n = 0; n < 200; ++n) {
      int x = integer(*this, 1, map_width - 2),
          y = integer(*this, 1, map_height - 2);
      if (walkable(x, y) && mob_at(x, y) < 0) {
        px = pvx = x;
        py = pvy = y;
        clear_path();
        break;
      }
    }
    damage = 0;
    break;
  case Metabolism:
    if (magic_actor < 0 && hunger < 450) {
      int heal = std::min(4, maxhp - hp);
      hp += heal;
      hunger = std::min(450, hunger + heal * 10);
    }
    break;
  case Multiplicity: {
    if (mob_def(attacker.type).boss || attacker.effects.ally)
      break;
    Mob clone = attacker;
    clone.hp = clone.maxhp;
    clone.minion = clone.owner = clone.summoning_pos = -1;
    clone.effects = {};
    clone.lucky = clone.rose_ally = clone.beam_charged = false;
    clone.combat = {};
    for (int y = py - 1; y <= py + 1; ++y)
      for (int x = px - 1; x <= px + 1; ++x) {
        if (!walkable(x, y) || mob_at(x, y) >= 0 || (x == px && y == py))
          continue;
        const bool occupied = std::any_of(
            pending_swarms.begin(), pending_swarms.end(),
            [&](const Mob &mob) { return mob.x == x && mob.y == y; });
        if (occupied)
          continue;
        clone.x = clone.vx = x;
        clone.y = clone.vy = y;
        clone.effects.ally = integer(*this, 0, 1) == 0;
        clone.next_action = actor_time + 1;
        pending_swarms.push_back(clone);
        return damage;
      }
    break;
  }
  case Stench:
    toxic_gas.seed(py * MW + px, 250);
    break;
  case Overgrowth:
    seed_effect(seed_rules::ids[integer(*this, 1, 11)], py * MW + px);
    break;
  default:
    break;
  }
  return damage;
}

bool Game::has_artifact(uint8_t id) const {
  return artifact_rules::valid(id) &&
         ((ring1 == id && !(ring1_flags & IF_CURSED)) ||
          (ring2 == id && !(ring2_flags & IF_CURSED)));
}
int Game::artifact_level(uint8_t id) const {
  return ring1 == id ? ring1_level : ring2 == id ? ring2_level : -1;
}
uint8_t Game::generate_artifact() {
  std::vector<uint8_t> available;
  for (int i = 0; i < artifact_rules::count; ++i) {
    uint8_t id = IT_ART_TOOLKIT + i;
    if (artifact_rules::ordinary(id) && !(artifact_generated & (1u << i)))
      available.push_back(id);
  }
  if (available.empty())
    return IT_NONE;
  uint8_t id = available[integer(*this, 0, int(available.size()) - 1)];
  artifact_generated |= 1u << artifact_rules::index(id);
  return id;
}

void Game::artifact_menu(int slot) {
  Item item = selection_item(slot);
  if (item_def(item.id).kind != K_ARTIFACT)
    return;
  artifact_slot = slot;
  clear_path();
  char description[256];
  describe_item(item, description, sizeof(description));
  open_dialog(25, display_name(item.id), description, "发动 (A)", "培养 (B)",
              "卸下 (X)", "取消 (Y)");
}

void Game::artifact_experience(float portion) {
  for (int slot : {-3, -4}) {
    const uint8_t id = slot == -3 ? ring1 : ring2;
    auto &state = slot == -3 ? ring1_magic : ring2_magic;
    auto &level = slot == -3 ? ring1_level : ring2_level;
    const uint8_t flags = slot == -3 ? ring1_flags : ring2_flags;
    if (!artifact_rules::valid(id) || (flags & IF_CURSED) ||
        spell_effects.magic_immunity > 0)
      continue;
    const float energy =
        std::pow(1.175f, std::clamp(accessory_bonus(IT_RING_ENERGY), -4, 30));
    if (id == IT_ART_CHAINS) {
      state.experience += rounded(portion * 100);
      add_charge(state, portion * 6 * energy,
                 artifact_rules::capacity(id, level));
      if (state.experience > 100 + 100 * level && level < 5) {
        state.experience -= 100 + 100 * level;
        ++level;
      }
    }
    if (id == IT_ART_HORN)
      add_charge(state, portion * (1.25f + .625f * level) * energy,
                 artifact_rules::capacity(id, level));
    if (id == IT_ART_ARMBAND)
      add_charge(state, portion * (3 + .15f * level) * energy,
                 artifact_rules::capacity(id, level));
    if (id == IT_ART_TOOLKIT)
      add_charge(state, portion * (2 + level),
                 artifact_rules::capacity(id, level));
  }
}
void Game::artifact_charge(float amount) {
  for (int slot : {-3, -4}) {
    auto item = selection_item(slot);
    if (artifact_rules::valid(item.id) && !(item.flags & IF_CURSED) &&
        spell_effects.magic_immunity <= 0) {
      auto &state = slot == -3 ? ring1_magic : ring2_magic;
      const float rate =
          item.id == IT_ART_ROSE                                     ? 4
          : item.id == IT_ART_FORESIGHT || item.id == IT_ART_SANDALS ? 2
          : item.id == IT_ART_ARMBAND || item.id == IT_ART_SPELLBOOK ? .1f
                                                                     : .25f;
      add_charge(state, amount * rate,
                 artifact_rules::capacity(item.id, item.level));
    }
  }
}

void Game::artifact_action(int slot, int action, int target) {
  if (slot != -3 && slot != -4)
    return;
  Item item = selection_item(slot);
  if (!artifact_rules::valid(item.id))
    return;
  artifact_slot = slot;
  auto &state = slot == -3 ? ring1_magic : ring2_magic;
  auto &lvl = slot == -3 ? ring1_level : ring2_level;
  auto &flags = slot == -3 ? ring1_flags : ring2_flags;
  auto &id = slot == -3 ? ring1 : ring2;
  if (action == 3)
    return;
  if (action == 2) {
    if (flags & IF_CURSED) {
      message("诅咒让你无法卸下神器。");
      return;
    }
    if (inv_n >= bag_capacity()) {
      message("背包满了。");
      return;
    }
    if (id == IT_ART_HOURGLASS)
      end_hourglass();
    if (id == IT_ART_CLOAK && state.cooldown > 0) {
      invis_t = 0;
      item.magic.cooldown = 0;
    }
    if (id == IT_ART_ROSE && rose_ghost >= 0 && rose_ghost < int(mobs.size())) {
      mobs[rose_ghost].alive = false;
      rose_ghost = -1;
    }
    inv[inv_n++] = item;
    id = IT_NONE;
    lvl = 0;
    flags = IF_KNOWN;
    state = {};
    spend_action(1);
    return;
  }
  if (flags & IF_CURSED) {
    flags |= IF_CURSE_KNOWN;
    message("神器受到了诅咒，无法发动。");
    return;
  }
  flags |= IF_KNOWN;
  if (spell_effects.magic_immunity > 0) {
    message("魔法免疫期间无法发动神器。");
    return;
  }
  if (action == 0 && id == IT_ART_TOME && target < 0) {
    tome_menu();
    return;
  }
  if (action == 1) {
    if (id != IT_ART_TOOLKIT) {
      artifact_feed_slot = slot;
      bool available = false;
      for (int i = 0; i < inv_n; ++i)
        available |= artifact_feedable(slot, i);
      if (!available) {
        artifact_feed_slot = 0;
        message("没有适合培养该神器的材料。");
        return;
      }
      selection_scroll = id;
      transmute_sel = transmute_page = 0;
      transmute_open = true;
      return;
    }
    if (lvl >= artifact_rules::level_cap(id)) {
      message("神器已达最高等级。");
      return;
    }
    if (alchemy_energy < 6) {
      message("需要6点炼金能量。");
      return;
    }
    alchemy_energy -= 6;
    ++lvl;
    message("神器吸收了材料。");
    spend_action(1);
    return;
  }
  if (id == IT_ART_CHALICE) {
    if (lvl >= 10)
      return;
    const int low = int(std::ceil(3 + 2.5f * lvl * lvl)),
              high = int(std::floor(7 + 3.5f * lvl * lvl));
    if (action != 4) {
      char warning[160];
      snprintf(warning, sizeof(warning), "献血可能造成 %d–%d 点伤害。", low,
               high);
      open_dialog(26, "蓄血圣杯", warning, "献血", "取消");
      return;
    }
    take_damage(std::max(1, normal(*this, low, high) - roll_armor()));
    if (!dead)
      ++lvl;
    spend_action(1);
    return;
  }
  if (id == IT_ART_TOOLKIT) {
    if (state.cooldown > 0) {
      message("工具箱尚未预热完成。");
      return;
    }
    artifact_brewing = alchemy_open = true;
    alchemy_sel = 0;
    return;
  }
  if (id == IT_ART_CLOAK) {
    if (state.cooldown > 0) {
      state.cooldown = 0;
      invis_t = 0;
      return;
    }
    if (state.aux < 1) {
      message("神器充能不足。");
      return;
    }
    state.cooldown = 1;
    invis_t = 3;
    spend_action(1);
    return;
  }
  if (id == IT_ART_HORN) {
    if (state.aux < 1) {
      message("丰饶之角没有食物。");
      return;
    }
    int count = std::min(state.aux, std::max(1, hunger / 90));
    hunger = std::max(0, hunger - 90 * count);
    cleric_food();
    state.aux -= count;
    spend_action(3);
    return;
  }
  if (id == IT_ART_HOURGLASS) {
    if (spell_effects.hourglass_active) {
      end_hourglass();
      return;
    }
    if (state.aux < 1) {
      message("神器充能不足。");
      return;
    }
    if (action == 4) {
      const int used = std::min(2, state.aux);
      const int duration = hourglass_rules::stasis_duration(state.aux);
      hunger = hourglass_rules::stasis_hunger(hunger, state.aux);
      state.aux -= used;
      dispel_action();
      message("你进入了时间静止。");
      time_stasis = true;
      for (int i = 0; i < duration && !dead; ++i)
        spend_action(1);
      time_stasis = false;
    } else if (action == 5) {
      dispel_action();
      freeze_left = 2;
      --state.aux;
      spell_effects.hourglass_active = true;
      message("时间冻结了。");
    } else
      open_dialog(27, "时光沙漏", "选择时间能力。", "时间静止", "冻结时间",
                  "取消");
    return;
  }
  if (id == IT_ART_ROSE && !(rose_ghost >= 0 && rose_ghost < int(mobs.size()) &&
                             mobs[rose_ghost].alive)) {
    if (state.aux < 100) {
      message("玫瑰尚未充能完毕。");
      return;
    }
    for (int y = py - 1; y <= py + 1; ++y)
      for (int x = px - 1; x <= px + 1; ++x) {
        if (!walkable(x, y) || mob_at(x, y) >= 0 || (x == px && y == py))
          continue;
        Mob ghost;
        ghost.type = M_WRAITH;
        ghost.x = ghost.vx = x;
        ghost.y = ghost.vy = y;
        ghost.hp = ghost.maxhp = 40 + 10 * lvl;
        ghost.effects.ally = true;
        ghost.rose_ally = true;
        ghost.atk = 4 + lvl;
        ghost.def = lvl;
        ghost.next_action = actor_time + 1;
        rose_ghost = mobs.size();
        mobs.push_back(ghost);
        state.aux = 0;
        spend_action(1);
        return;
      }
    message("附近没有召唤幽灵的位置。");
    return;
  }
  if (id == IT_ART_SANDALS &&
      (!seed_cost(state.aux2) || state.aux < seed_cost(state.aux2))) {
    message("需要先喂食种子，并积攒足够的自然能量。");
    return;
  }
  if (id == IT_ART_SPELLBOOK) {
    spellbook_read();
    return;
  }
  if (id == IT_ART_FORESIGHT && state.aux < 5) {
    message("护符充能不足。");
    return;
  }
  if (target < 0) {
    magic_target = -1;
    artifact_slot = slot;
    artifact_targeting = true;
    targeting = true;
    target_item = -1;
    backpack = false;
    message("选择神器目标，A发动，B取消。");
    return;
  }
  const int x = target % MW, y = target / MW;
  if (!in_map(x, y) || (!seen[y][x] && id != IT_ART_FORESIGHT)) {
    message("目标尚未探索。");
    return;
  }
  const int mob = mob_at(x, y);
  if (id == IT_ART_ROSE) {
    if (x == px && y == py) {
      rose_command = rose_enemy = -1;
      mobs[rose_ghost].target = -1;
    } else if (mob >= 0 && !npc(mobs[mob]) && !mobs[mob].effects.ally) {
      rose_enemy = mob;
      rose_command = -1;
    } else if (walkable(x, y)) {
      rose_command = target;
      rose_enemy = -1;
      mobs[rose_ghost].target = target;
    }
    return;
  }
  if (id == IT_ART_SANDALS) {
    if (!vis[y][x] || distance(px, py, x, y) > 3)
      return;
    state.aux -= seed_cost(state.aux2);
    dispel_action();
    seed_effect(state.aux2, target);
    spend_action(1);
    return;
  }
  if (id == IT_ART_CHAINS) {
    const int range = distance(px, py, x, y);
    if (!range || !vis[y][x])
      return;
    const auto ray = cast_ray(py * MW + px, target, ballistica::stop_target);
    if (mob >= 0) {
      auto &enemy = mobs[mob];
      if (npc(enemy) || enemy.effects.ally || mob_def(enemy.type).boss)
        return;
      for (int i = 1; i < ray.distance; ++i) {
        const int cell = ray.path[i], cx = cell % MW, cy = cell / MW;
        if (!walkable(cx, cy) || mob_at(cx, cy) >= 0)
          continue;
        const int cost = distance(x, y, cx, cy);
        if (cost > state.aux) {
          message("锁链充能不足。");
          return;
        }
        enemy.x = enemy.vx = cx;
        enemy.y = enemy.vy = cy;
        state.aux -= cost;
        trigger_trap(feature_at(cx, cy), &enemy);
        spend_action(1);
        return;
      }
    } else if (walkable(x, y) && range <= state.aux) {
      px = pvx = x;
      py = pvy = y;
      state.aux -= range;
      clear_path();
      trample_grass(x, y, true);
      trigger_tile_features();
      spend_action(1);
    } else
      message("目标无法到达或锁链充能不足。");
    return;
  }
  if (id == IT_ART_FORESIGHT) {
    if (target == py * MW + px) return;
    const auto scan = foresight_rules::scan(py * MW + px, target, MW,
        map_width, map_height, lvl, state.aux);
    std::vector<uint8_t> scanned(MW * map_height);
    for (int cell : scan.cells) {
      scanned[cell] = 1;
      const int cx = cell % MW, cy = cell / MW;
      bool discoverable = false;
      for (int ny = std::max(0, cy - 1); ny <= std::min(map_height - 1, cy + 1); ++ny)
        for (int nx = std::max(0, cx - 1); nx <= std::min(map_width - 1, cx + 1); ++nx)
          discoverable |= tiles[ny][nx] != T_WALL;
      if (discoverable && !(seen[cy][cx] & 3)) { seen[cy][cx] |= 2; ++state.experience; }
      if (tiles[cy][cx] == T_SECRET) {
        tiles[cy][cx] = T_DOOR;
        state.experience += 100;
      }
    }
    for (auto &feature : features)
      if (scanned[feature.y * MW + feature.x] &&
          feature.type == F_TRAP) {
        feature.type = F_TRAP_FOUND;
        state.experience += 10;
      }
    mob_effects.resize(mobs.size());
    for (size_t i = 0; i < mobs.size(); ++i) {
      const auto &enemy = mobs[i];
      if (!enemy.alive || npc(enemy) || enemy.effects.ally ||
          !scanned[enemy.y * MW + enemy.x]) continue;
      mob_effects[i].scry = std::max(mob_effects[i].scry, float(5 + 2 * lvl));
      if (!vis[enemy.y][enemy.x]) state.experience += 10;
      if (cleric_subclass == 1 && mob_effects[i].illuminated) {
        mob_effects[i].illuminated = false;
        damage_mob(int(i), 5 + level);
      }
    }
    // The native world stores loose items individually; award once per heap.
    std::vector<uint8_t> heaps(MW * map_height);
    for (const auto &drop : items) heaps[drop.y * MW + drop.x] = 1;
    for (const auto &feature : features)
      if (feature.type == F_CHEST || feature.type == F_CHEST_OPEN ||
          feature.type == F_CHEST_LOCKED || feature.type == F_CHEST_CRYSTAL ||
          feature.type == F_TOMB || feature.type == F_TOMB_USED ||
          feature.type == F_REMAINS)
        heaps[feature.y * MW + feature.x] = 1;
    for (int cell : scan.cells) if (heaps[cell]) {
      if (!(seen[cell / MW][cell % MW] & 4) && !vis[cell / MW][cell % MW])
        state.experience += 10;
      bool found = false;
      for (auto &awareness : scry_heaps)
        if (awareness.depth == floor && awareness.cell == cell) {
          awareness.left = std::max(awareness.left, float(5 + 2 * lvl));
          found = true;
        }
      if (!found) scry_heaps.push_back({floor, cell, float(5 + 2 * lvl)});
    }
    foresight_rules::spend(state.aux, state.stored, scan.distance);
    if (state.experience >= 100 + 50 * lvl && lvl < 10) {
      state.experience -= 100 + 50 * lvl;
      ++lvl;
    }
    dispel_action();
    recompute_fov();
    spend_action(1);
    return;
  }
  if (id == IT_ART_KEY) {
    const int feature = feature_at(x, y);
    int cost = 0;
    if (distance(px, py, x, y) == 1) {
      if (tiles[y][x] == T_DOOR_LOCKED)
        cost = 1;
      else if (feature >= 0 && features[feature].type == F_CHEST_LOCKED)
        cost = 2;
      else if (feature >= 0 && features[feature].type == F_CHEST_CRYSTAL)
        cost = 5;
    }
    if (cost) {
      if (state.aux < cost || (cost == 1 && boss_alive())) {
        message("无法打开这把锁。");
        return;
      }
      if (cost == 1)
        tiles[y][x] = T_DOOR;
      else
        features[feature].type = F_CHEST;
      state.experience += 2 + cost;
    } else {
      if (state.aux < 2 || (x == px && y == py)) {
        message("万能钥匙充能不足或目标无效。");
        return;
      }
      const int dx = (x > px) - (x < px), dy = (y > py) - (y < py);
      const int cells[3][2] = {{px + dx, py + dy},
                               {px + dx - dy, py + dy + dx},
                               {px + dx + dy, py + dy - dx}};
      bool placed = false;
      for (const auto &cell : cells) {
        const int cx = cell[0], cy = cell[1];
        if (!walkable(cx, cy) || mob_at(cx, cy) >= 0 ||
            feature_at(cx, cy) >= 0 || item_at(cx, cy) >= 0)
          continue;
        if (tiles[cy][cx] != T_FLOOR && tiles[cy][cx] != T_GRASS &&
            tiles[cy][cx] != T_WATER)
          continue;
        features.push_back({cx, cy, F_KEY_WALL, 12});
        placed = true;
      }
      if (!placed) {
        message("前方没有布置钥匙之墙的位置。");
        return;
      }
      cost = 2;
      state.experience += 2;
    }
    state.aux -= cost;
    if (state.experience > 4 + lvl && lvl < 10) {
      state.experience -= 4 + lvl;
      ++lvl;
    }
    spend_action(1);
    return;
  }
  if (id == IT_ART_ARMBAND) {
    if (state.aux < 1 || mob < 0 || npc(mobs[mob]) || mobs[mob].effects.ally ||
        distance(px, py, x, y) > 1) {
      message("靠近敌人后才能偷窃。");
      return;
    }
    auto &enemy = mobs[mob];
    const bool surprised = invis_t > 0 || enemy.ai != Mob::Hunting;
    --state.aux;
    state.experience += surprised ? 5 : 3;
    enemy.effects.blind = enemy.effects.chill =
        3 + lvl / 2 + (surprised ? 2 : 0);
    if (!enemy.effects.stolen && !mob_def(enemy.type).boss &&
        uniform(*this) < .1f * (1 + .1f * lvl + (surprised ? .5f : 0.f))) {
      gold += integer(*this, 10 + floor * 2, 20 + floor * 4);
      enemy.effects.stolen = true;
      message("你偷到了一些金币。");
    }
    dispel_action();
    if (state.experience >= 10 + rounded(3.33f * lvl) && lvl < 10) {
      state.experience -= 10 + rounded(3.33f * lvl);
      ++lvl;
    }
    spend_action(1);
    return;
  }
  if (id == IT_ART_TOME) {
    tome_cast(magic_target, target);
    return;
  }
}

void Game::tick_magic() {
  if (cleric_ranks[cleric_rules::LightReading] && !boss_alive() &&
      spell_effects.magic_immunity <= 0)
    for (int i = 0; i < inv_n; ++i)
      if (inv[i].id == IT_ART_TOME && !(inv[i].flags & IF_CURSED)) {
        auto &item = inv[i];
        add_charge(
            item.magic,
            artifact_rules::recharge(item.id, item.level, item.magic.aux) *
                std::pow(1.175f, accessory_bonus(IT_RING_ENERGY)) * .25f *
                cleric_ranks[cleric_rules::LightReading],
            artifact_rules::capacity(item.id, item.level));
        item.charges = item.magic.aux;
      }
  if (deferred_damage > 0) {
    int amount = std::max(1, int(deferred_damage * .1f));
    deferred_damage = std::max(0.f, deferred_damage - amount);
    take_damage(amount);
  }
  kinetic_damage =
      std::max(0.f, kinetic_damage - std::max(.1f, kinetic_damage * .025f));
  auto decay = [](Effects &e) {
    for (float *value : {&e.burning, &e.chill, &e.poison_delay, &e.blind,
                         &e.paralysis, &e.roots, &e.charm})
      *value = std::max(0.f, *value - 1);
  };
  for (size_t i = 0; i < mobs.size(); ++i) {
    auto &m = mobs[i];
    if (!m.alive)
      continue;
    if (m.effects.burning > 0) {
      if (tiles[m.y][m.x] == T_WATER)
        m.effects.burning = 0;
      else
        damage_mob(i, normal(*this, 1, 3 + floor / 4));
    }
    if (m.effects.poison > 0 && m.effects.poison_delay <= 0) {
      damage_mob(i, int(m.effects.poison / 3) + 1);
      m.effects.poison = std::max(0.f, m.effects.poison - 1);
    }
    if (m.alive && m.effects.bleeding > 0) {
      m.effects.bleeding *= .75f;
      damage_mob(i, int(std::ceil(m.effects.bleeding)));
      if (m.effects.bleeding < .5f)
        m.effects.bleeding = 0;
    }
    decay(m.effects);
    m.lucky = false;
  }
  if (hero_effects.burning > 0 && armor_magic.effect != Brimstone) {
    if (tiles[py][px] == T_WATER)
      hero_effects.burning = 0;
    else
      take_damage(normal(*this, 1, 3 + floor / 4));
  }
  if (hero_effects.poison > 0) {
    take_damage(1 + int(hero_effects.poison / 3));
    hero_effects.poison = std::max(0.f, hero_effects.poison - 1);
  }
  if (hero_effects.bleeding > 0) {
    hero_effects.bleeding *= .75f;
    take_damage(int(std::ceil(hero_effects.bleeding)));
    if (hero_effects.bleeding < .5f)
      hero_effects.bleeding = 0;
  }
  decay(hero_effects);
  if (!boss_alive()) {
    if (weapon_magic.effect == Crystal)
      weapon_magic.durability = std::min(100.f, weapon_magic.durability + .2f);
    if (rose_weapon.magic.effect == Crystal)
      rose_weapon.magic.durability =
          std::min(100.f, rose_weapon.magic.durability + .2f);
    for (int i = 0; i < inv_n; ++i)
      if ((item_def(inv[i].id).kind == K_WEAPON ||
           item_def(inv[i].id).kind == K_MISSILE) &&
          inv[i].magic.effect == Crystal)
        inv[i].magic.durability = std::min(
            100.f, inv[i].magic.durability +
                       (item_def(inv[i].id).kind == K_MISSILE ? .1f : .2f));
  }
  for (int slot : {-3, -4}) {
    auto item = selection_item(slot);
    if (!artifact_rules::valid(item.id))
      continue;
    auto &state = slot == -3 ? ring1_magic : ring2_magic;
    auto &lvl = slot == -3 ? ring1_level : ring2_level;
    if (item.id == IT_ART_ROSE && rose_ghost >= 0 &&
        rose_ghost < int(mobs.size()) && mobs[rose_ghost].alive)
      continue;
    if (item.flags & IF_CURSED) {
      if (item.id == IT_ART_FORESIGHT) state.consumed = 0;
      if (item.id == IT_ART_CHAINS && integer(*this, 0, 99) == 0)
        debuffs[status_rules::Cripple] = 10;
      if (item.id == IT_ART_HORN && hunger < 450)
        ++hunger;
      continue;
    }
    if (item.id == IT_ART_TOOLKIT && state.cooldown > 0)
      state.cooldown = std::max(0.f, state.cooldown - 1);
    if (spell_effects.magic_immunity > 0) {
      if (item.id == IT_ART_FORESIGHT) state.consumed = 0;
      continue;
    }
    if (item.id == IT_ART_CLOAK && state.cooldown > 0) {
      invis_t = 3;
      if (--state.cooldown <= 0) {
        if (state.aux > 0) {
          --state.aux;
          state.cooldown = 4;
          const int diff = level - (1 + 2 * lvl) - (lvl >= 7 ? lvl - 6 : 0);
          state.experience += std::lround(
              10 * (diff >= 0 ? std::pow(1.1f, diff) : std::pow(.75f, -diff)));
          if (state.experience >= (lvl + 1) * 50 && lvl < 10) {
            state.experience -= (lvl + 1) * 50;
            ++lvl;
          }
        } else
          invis_t = 0;
      }
    } else if (!boss_alive() &&
               state.aux < artifact_rules::capacity(item.id, lvl))
      add_charge(
          state,
          artifact_rules::recharge(item.id, lvl, state.aux) *
              std::pow(1.175f,
                       std::clamp(accessory_bonus(IT_RING_ENERGY), -4, 30)),
          artifact_rules::capacity(item.id, lvl));
    if (item.id == IT_ART_FORESIGHT) {
      bool warn = false;
      for (const auto &f : features)
        if (f.type == F_TRAP && vis[f.y][f.x] && distance(px, py, f.x, f.y) <= 3)
          warn = true;
      if (warn && !state.consumed) {
        message("先见护符警示附近存在隐藏危险。");
        clear_path();
      }
      state.consumed = warn;
    }
  }
}
