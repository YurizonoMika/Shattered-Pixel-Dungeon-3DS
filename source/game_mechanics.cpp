// Native ports of v4.0.2 plants, MissileWeapon and artifact interactions.
// GPL-3.0-or-later. Copyright (C) 2014-2026 Evan Debenham.
#include "artifact_rules.h"
#include "game.h"
#include "hero_rules.h"
#include "projectile_rules.h"
#include "seed_rules.h"
#include "time_bubble_rules.h"
#include "frost_rules.h"
#include <algorithm>
#include <cmath>
namespace {
uint32_t draw(Game &g) {
  g.rng ^= g.rng << 13;
  g.rng ^= g.rng >> 17;
  g.rng ^= g.rng << 5;
  return g.rng;
}
float roll(Game &g) { return (draw(g) >> 8) / 16777216.f; }
int normal(Game &g, int lo, int hi) {
  return lo + int((roll(g) + roll(g)) * (hi - lo + 1) * .5f);
}
int dist(int x, int y, int tx, int ty) {
  return std::max(std::abs(x - tx), std::abs(y - ty));
}
bool npc(const Mob &m) { return m.type >= M_NPC_GHOST && m.type <= M_NPC_IMP; }
} // namespace
mechanics_rules::Actor &Game::actor_effects(int index) {
  if (index < 0)
    return spell_effects;
  if (mob_effects.size() < mobs.size())
    mob_effects.resize(mobs.size());
  return mob_effects.at(index);
}
void Game::freeze_actor(int index) {
  if(index<0 && time_stasis)return;
  auto &effect=actor_effects(index);
  auto &status=index<0 ? hero_effects : mobs[index].effects;
  const int x=index<0 ? px : mobs[index].x,y=index<0 ? py : mobs[index].y;
  status.burning=status.chill=0;
  effect.frost=std::max(effect.frost,frost_rules::duration(tiles[y][x]==T_WATER));
  if(index<0)clear_path();
}
void Game::thaw_actor(int index) {
  auto &effect=actor_effects(index);
  auto &status=index<0 ? hero_effects : mobs[index].effects;
  const int x=index<0 ? px : mobs[index].x,y=index<0 ? py : mobs[index].y;
  frost_rules::detach(effect.frost,status.chill,tiles[y][x]==T_WATER);
}
void Game::seed_effect(uint8_t id, int cell) {
  if (seed_rules::index(id) < 0 || !in_map(cell % MW, cell / MW))
    return;
  const int x = cell % MW, y = cell / MW, index = mob_at(x, y);
  const bool hero_here = px == x && py == y;
  const bool occupied = hero_here || index >= 0;
  mechanics_rules::Actor empty;
  auto &effect = occupied ? actor_effects(hero_here ? -1 : index) : empty;
  auto &status = hero_here    ? hero_effects
                 : index >= 0 ? mobs[index].effects
                              : hero_effects;
  switch (id) {
  case IT_SEED_FIREBLOOM:
    // Fire also affects adjacent high grass. Plants activate on empty cells.
    if (occupied)
      status.burning = 8;
    if (tiles[y][x] == T_HIGH_GRASS || tiles[y][x] == T_GRASS)
      tiles[y][x] = T_FLOOR;
    break;
  case IT_SEED_SWIFTTHISTLE:
    if (occupied) {
      effect.time_bubble = time_bubble_rules::reset();
      effect.bubble_active = true;
    }
    break;
  case IT_SEED_SUNGRASS:
    if (occupied) {
      effect.healing += hero_here ? maxhp : mobs[index].maxhp;
      effect.anchor = cell;
    }
    break;
  case IT_SEED_EARTHROOT:
    if (occupied) {
      effect.earth =
          std::max(effect.earth, float(hero_here ? maxhp : mobs[index].maxhp));
      effect.earth_anchor = cell;
    }
    break;
  case IT_SEED_ICECAP:
    for (int cy = y - 1; cy <= y + 1; ++cy)
      for (int cx = x - 1; cx <= x + 1; ++cx) {
        if (!in_map(cx, cy) || blocks_sight(cx, cy))
          continue;
        const int victim = mob_at(cx, cy);
        if (victim >= 0) freeze_actor(victim);
        if (cx == px && cy == py) freeze_actor(-1);
      }
    break;
  case IT_SEED_STORMVINE:
    if (occupied)
      effect.vertigo = 10;
    break;
  case IT_SEED_SORROWMOSS:
    if (occupied)
      status.poison = std::max(status.poison, float(5 + std::lround(2 * floor / 3.f)));
    break;
  case IT_SEED_MAGEROYAL:
    if (occupied) {
      seed_rules::cure(status, effect);
      if (hero_here) {
        for(int type:{status_rules::Poison,status_rules::Cripple,status_rules::Weakness,status_rules::Vulnerable})
          debuffs[type]=0;
      }
    }
    break;
  case IT_SEED_STARFLOWER:
    if (occupied)
      effect.bless = std::max(effect.bless, seed_rules::blessing_duration);
    break;
  case IT_SEED_FADELEAF:
    if (index >= 0)
      teleport_mob(index);
    else if (hero_here && !boss_alive())
      for (int tries = 0; tries < 500; ++tries) {
        int cx = 1 + draw(*this) % (map_width - 2),
            cy = 1 + draw(*this) % (map_height - 2);
        if (walkable(cx, cy) && mob_at(cx, cy) < 0) {
          px = pvx = cx;
          py = pvy = cy;
          clear_path();
          break;
        }
      }
    break;
  case IT_SEED_BLINDWEED:
    if (occupied) {
      status.blind = std::max(status.blind, seed_rules::blindness_duration);
      if (hero_here)
        debuffs[status_rules::Cripple] = std::max(int(debuffs[status_rules::Cripple]), int(seed_rules::cripple_duration));
      else {
        effect.cripple = std::max(effect.cripple, seed_rules::cripple_duration);
        mobs[index].ai = Mob::Wandering;
        mobs[index].target = -1;
      }
    }
    break;
  case IT_SEED_ROTBERRY:
    toxic_gas.seed(cell, 100);
    items.push_back({x, y, id});
    break;
  }
  recompute_fov();
}
void Game::plant_seed(int index, int cell) {
  if (index < 0 || index >= inv_n || seed_rules::index(inv[index].id) < 0)
    return;
  const int x = cell % MW, y = cell / MW;
  if (!in_map(x, y) || !walkable(x, y) || feature_at(x, y) >= 0)
    return;
  Feature plant;
  plant.x = x;
  plant.y = y;
  plant.type = F_PLANT;
  plant.aux = inv[index].id;
  features.push_back(plant);
  remove_item(index);
  spend_action(1);
}
bool Game::artifact_feedable(int slot, int index) const {
  if (index < 0 || index >= inv_n)
    return false;
  const Item art = selection_item(slot), item = inv[index];
  if (art.flags & IF_CURSED)
    return false;
  if (art.id == IT_ART_SANDALS) {
    const int seed = seed_rules::index(item.id);
    return seed >= 0 && !(art.magic.consumed & (1u << seed)) &&
           (art.level < 3 || art.magic.aux2 != item.id);
  }
  if (art.level >= artifact_rules::level_cap(art.id))
    return false;
  return (art.id == IT_ART_HORN && item_def(item.id).kind == K_FOOD) ||
         (art.id == IT_ART_ROSE && item.id == IT_ROSE_PETAL) ||
         (art.id == IT_ART_HOURGLASS && item.id == IT_HOURGLASS_SAND) ||
         (art.id == IT_ART_SPELLBOOK && art.level < 10 &&
          (item.id == spellbook_order[art.level] ||
           (art.level < 9 && item.id == spellbook_order[art.level + 1])));
}
void Game::artifact_feed(int slot, int index) {
  if ((slot != -3 && slot != -4) || !artifact_feedable(slot, index))
    return;
  const uint8_t id = slot == -3 ? ring1 : ring2;
  auto &state = slot == -3 ? ring1_magic : ring2_magic;
  auto &lvl = slot == -3 ? ring1_level : ring2_level;
  const uint8_t material = inv[index].id;
  if (id == IT_ART_SANDALS) {
    state.aux2 = material;
    if (lvl < 3) {
      state.consumed |= 1u << seed_rules::index(material);
      if (++state.experience >= 3 + 3 * lvl) {
        ++lvl;
        state.consumed = 0;
        state.experience = 0;
      }
    }
  } else if (id == IT_ART_HORN) {
    state.experience += hero_rules::food_energy(material);
    while (state.experience >= 120 && lvl < 10) {
      state.experience -= 120;
      ++lvl;
    }
  } else {
    if (id == IT_ART_SPELLBOOK && material != spellbook_order[lvl] && lvl < 9)
      std::swap(spellbook_order[lvl], spellbook_order[lvl + 1]);
    ++lvl;
    state.consumed |= 1u << (material % 32);
    if (id == IT_ART_HOURGLASS)
      state.aux = std::min(state.aux + 1, artifact_rules::capacity(id, lvl));
  }
  remove_item(index);
  transmute_open = false;
  artifact_feed_slot = 0;
  message("神器吸收了选中的材料。");
  spend_action(1);
}
float Game::shop_steal_chance(int index, int &charges) const {
  charges = 0;
  if (index < 0 || index >= shop_n || shop_sold[index] ||
      !has_artifact(IT_ART_ARMBAND))
    return 0;
  const auto item = selection_item(ring1 == IT_ART_ARMBAND ? -3 : -4);
  const int value = std::max(1, item_value(shop_items[index].id) *
                                        int(shop_items[index].count) +
                                    10 * shop_items[index].level);
  const float each = 10 + item.level / 2.f;
  charges = std::min(item.magic.aux, int(std::ceil(value / each)));
  return std::min(1.f, charges * each / value);
}
void Game::shop_steal(int index) {
  if (!shop_open)
    return;
  int charges = 0;
  const float chance = shop_steal_chance(index, charges);
  if (charges == 0) {
    message("需要佩戴有充能的神偷袖章。");
    return;
  }
  const auto item = shop_items[index];
  bool room = inv_n < bag_capacity();
  for (int i = 0; i < inv_n; ++i)
    if (item_is_stackable(item.id) && inv[i].id == item.id &&
        inv[i].count + item.count <= 99)
      room = true;
  if (!room) {
    message("背包满了！");
    return;
  }
  if (roll(*this) > chance) {
    shop_warned[floor] = true;
    harm_shopkeeper();
    message("偷窃失败，店主带走了货物。");
    return;
  }
  auto &state = ring1 == IT_ART_ARMBAND ? ring1_magic : ring2_magic;
  auto &lvl = ring1 == IT_ART_ARMBAND ? ring1_level : ring2_level;
  state.aux -= charges;
  state.experience += 4 * charges;
  while (lvl < 10 && state.experience >= 10 + std::lround(3.33f * lvl)) {
    state.experience -= 10 + std::lround(3.33f * lvl);
    ++lvl;
  }
  const int before = inv_n;
  add_item(item.id, item.count, item.level, item.flags);
  if (inv_n > before)
    inv[before] = item;
  shop_sold[index] = true;
  message("你偷到了选中的商品。");
}
bool Game::missile_similar(const Item &a, const Item &b) const {
  return a.id == b.id && a.level == b.level && a.magic.aux2 == b.magic.aux2 &&
         a.magic.effect == b.magic.effect;
}
void Game::remember_inscription(uint8_t id) {
  if (inscription_replay || hero != 5 ||
      !cleric_ranks[cleric_rules::RecallInscription])
    return;
  if (item_def(id).cat != CAT_SCROLL && id != IT_STONE_AUG &&
      id != IT_STONE_ENCHANT)
    return;
  recalled_item = id;
  recall_left = cleric_ranks[cleric_rules::RecallInscription] == 2 ? 300 : 10;
}
void Game::cleric_form(int spell, int index) {
  if (index < 0 || index >= inv_n || !cleric_unlocked(spell) ||
      tome_slot() == -99)
    return;
  auto &tome = tome_state();
  int cost = cleric_rules::cost(spell, std::max(1, int(cleric_ranks[spell])));
  if (tome.aux < cost)
    return;
  trinity_spell = spell;
  trinity_imbue(inv[index]);
}
void Game::discover_item(const Item &item) {
  if (item.id == IT_NONE || item.id >= IT_COUNT ||
      !(item.flags & IF_IDENTIFIED))
    return;
  discovered_items[item.id] = true;
  if (item.magic.effect) {
    if (item_def(item.id).kind == K_ARMOR)
      discovered_glyphs |= 1u << item.magic.effect;
    else if (item_def(item.id).kind == K_WEAPON ||
             item_def(item.id).kind == K_MISSILE)
      discovered_enchants |= 1u << item.magic.effect;
  }
}
void Game::trinity_menu(int spell) {
  if (spell < cleric_rules::BodyForm || spell > cleric_rules::SpiritForm ||
      !cleric_unlocked(spell))
    return;
  for (int i = -4; i < inv_n; ++i)
    discover_item(selection_item(i));
  trinity_spell = spell;
  trinity_options.clear();
  if (spell == cleric_rules::BodyForm) {
    for (int effect = 1; effect < magic_rules::Annoying; ++effect)
      if (discovered_enchants & (1u << effect)) {
        Item option;
        option.id = IT_WORN_SHORTSWORD;
        option.magic.effect = effect;
        option.flags = IF_KNOWN;
        trinity_options.push_back(option);
      }
    for (int effect = 1; effect < magic_rules::AntiEntropy; ++effect)
      if (discovered_glyphs & (1u << effect)) {
        Item option;
        option.id = IT_CLOTH_ARMOR;
        option.magic.effect = effect;
        option.flags = IF_KNOWN;
        trinity_options.push_back(option);
      }
  } else
    for (int id = 1; id < IT_COUNT; ++id)
      if (discovered_items[id]) {
        const auto kind = item_def(id).kind;
        if ((spell == cleric_rules::MindForm &&
             (kind == K_WAND || kind == K_MISSILE)) ||
            (spell == cleric_rules::SpiritForm &&
             (kind == K_RING || (kind == K_ARTIFACT && id != IT_ART_TOME)))) {
          Item option;
          option.id = id;
          option.flags = IF_KNOWN;
          trinity_options.push_back(option);
        }
      }
  const int pages = std::max(1, (int(trinity_options.size()) + 2) / 3);
  cleric_page %= pages;
  char labels[3][48]{};
  for (int i = 0; i < 3; ++i) {
    const int index = cleric_page * 3 + i;
    const char *label = "—";
    if (index < int(trinity_options.size())) {
      const auto &option = trinity_options[index];
      label = spell == cleric_rules::BodyForm
                  ? (item_def(option.id).kind == K_ARMOR
                         ? magic_rules::glyph_name(option.magic.effect)
                         : magic_rules::enchant_name(option.magic.effect))
                  : item_name(option.id);
    }
    snprintf(labels[i], sizeof(labels[i]), "%s", label);
  }
  char text[240];
  snprintf(text, sizeof(text),
           "从本局已发现的种类中灌注。第%d/%d页，L/"
           "R翻页。消耗%d点法典充能；发动形态另耗护甲充能。",
           cleric_page + 1, pages, cleric_rules::cost(spell));
  open_dialog(35, cleric_rules::names[spell], text, labels[0], labels[1],
              labels[2], "取消");
}
void Game::trinity_imbue(const Item &item) {
  if (tome_slot() == -99 || !cleric_unlocked(trinity_spell))
    return;
  const int cost = cleric_rules::cost(trinity_spell);
  auto &tome = tome_state();
  if (tome.aux < cost)
    return;
  const auto kind = item_def(item.id).kind;
  if (trinity_spell == cleric_rules::BodyForm &&
      (kind == K_WEAPON || kind == K_ARMOR))
    trinity_body = item;
  else if (trinity_spell == cleric_rules::MindForm &&
           (kind == K_WAND || kind == K_MISSILE))
    trinity_mind = item;
  else if (trinity_spell == cleric_rules::SpiritForm &&
           (kind == K_RING || (kind == K_ARTIFACT && item.id != IT_ART_TOME)))
    trinity_spirit = item;
  else
    return;
  finish_tome_cast(trinity_spell, cost);
  transmute_open = false;
  artifact_feed_slot = 0;
  message("三位一体已记住这个种类。");
}
void Game::trinity_use(int form, int target) {
  if (hero != 5 || cleric_ability != 2 || form < 0 || form > 2 ||
      spell_effects.magic_immunity > 0)
    return;
  Item item = form == 0   ? trinity_body
              : form == 1 ? trinity_mind
                          : trinity_spirit;
  if (item.id == IT_NONE) {
    message("需要先用法典灌注这个形态。");
    return;
  }
  const int spell = cleric_rules::BodyForm + form,
            rank = std::max(1, int(cleric_ranks[spell]));
  float cost = 25;
  if (form == 0 && (item_def(item.id).kind == K_ARMOR
                        ? item.magic.effect >= magic_rules::Affection
                        : item.magic.effect >= magic_rules::Corrupting))
    cost = 50;
  if (form == 1 && (item.id == IT_WAND_FIRE || item.id == IT_WAND_REGROWTH))
    cost = 50;
  if (form == 2) {
    if (item.id == IT_ART_ROSE || item.id == IT_ART_SPELLBOOK ||
        item.id == IT_ART_KEY)
      cost = 50;
    if (item.id == IT_ART_CHAINS || item.id == IT_ART_FORESIGHT ||
        item.id == IT_ART_HOURGLASS)
      cost = 35;
  }
  constexpr float armor_factors[] = {1.f, .88f, .77f, .68f, .6f};
  cost *= armor_factors[cleric_ranks[cleric_rules::HeroicEnergy]];
  if (armor_charge < cost) {
    message("护甲充能不足。");
    return;
  }
  if (form == 0) {
    const bool armor_form = item_def(item.id).kind == K_ARMOR;
    if (item.magic.effect ==
        (armor_form ? armor_magic.effect : weapon_magic.effect)) {
      message("不能叠加相同的魔法效果。");
      return;
    }
    spell_effects.body_effect = item.magic.effect;
    spell_effects.body_armor = armor_form;
    spell_effects.body_left = std::lround(13.33f + 6.67f * rank);
    spell_effects.body_durability =
        spell_effects.body_effect == magic_rules::Crystal
            ? spell_effects.body_left * 1.5f
            : 100;
  } else if (form == 1) {
    if (target < 0) {
      magic_target = -3;
      target_item = -1;
      artifact_targeting = targeting = true;
      backpack = false;
      return;
    }
    if (!in_map(target % MW, target / MW) || target == py * MW + px ||
        inv_n >= BAG_MAX)
      return;
    const bool missile = item_def(item.id).kind == K_MISSILE;
    if (!missile &&
        (std::abs(target % MW - px) + std::abs(target / MW - py) > 6 ||
         !los(px, py, target % MW, target / MW)))
      return;
    const int collision =
        cast_ray(py * MW + px, target,
                 missile ? ballistica::projectile : ballistica::magic_bolt)
            .collision;
    const int victim = mob_at(collision % MW, collision / MW);
    if (collision == py * MW + px ||
        (victim >= 0 && (npc(mobs[victim]) || mobs[victim].effects.ally)))
      return;
    item.level = 2 + rank;
    item.count = 1;
    item.flags = IF_KNOWN;
    item.magic = {};
    item.charges = wand_capacity(item);
    const int index = inv_n;
    inv[inv_n++] = item;
    virtual_projectile = true;
    armor_charge -= cost;
    artifact_targeting = targeting = false;
    if (missile)
      throw_projectile(index, target % MW, target / MW);
    else
      cast_wand(index, target % MW, target / MW);
    virtual_projectile = false;
    if (inv_n > index)
      remove_item(index, 99);
    return;
  } else {
    if (ring1 == item.id || ring2 == item.id) {
      message("不能叠加已佩戴的饰品。");
      return;
    }
    if (item_def(item.id).kind != K_RING && item.id != IT_ART_CHALICE) {
      const int artifact_level = 2 + 2 * rank;
      if (item.id == IT_ART_HOURGLASS) {
        armor_charge -= cost;
        dispel_action();
        spell_effects.time_bubble = time_bubble_rules::reset(artifact_level);
        spell_effects.bubble_active = true;
        spend_action(1);
        return;
      }
      if (item.id == IT_ART_ROSE) {
        for (int y = py - 1; y <= py + 1; ++y)
          for (int x = px - 1; x <= px + 1; ++x)
            if (walkable(x, y) && mob_at(x, y) < 0 && !(x == px && y == py)) {
              Mob ally;
              ally.type = M_WRAITH;
              ally.x = ally.vx = x;
              ally.y = ally.vy = y;
              ally.hp = ally.maxhp = 20 + 8 * artifact_level;
              ally.effects.ally = true;
              ally.next_action = actor_time + 1;
              mobs.push_back(ally);
              armor_charge -= cost;
              invis_t = 0;
              spend_action(1);
              return;
            }
        return;
      }
      if (item.id == IT_ART_HORN) {
        hunger = std::max(0, hunger - 90);
        cleric_food();
        armor_charge -= cost;
        invis_t = 0;
        spend_action(3);
        return;
      }
      if (item.id == IT_ART_TOOLKIT) {
        artifact_brewing = alchemy_open = true;
        alchemy_sel = 0;
        armor_charge -= cost;
        return;
      }
      if (item.id == IT_ART_SPELLBOOK) {
        constexpr uint8_t scrolls[] = {
            IT_SCROLL_IDENTIFY, IT_SCROLL_CURSE, IT_SCROLL_MIRROR,
            IT_SCROLL_RECHARGE, IT_SCROLL_TELE,  IT_SCROLL_LULLABY,
            IT_SCROLL_MAP,      IT_SCROLL_RAGE,  IT_SCROLL_RETRIBUTION,
            IT_SCROLL_TERROR};
        const uint8_t scroll = scrolls[draw(*this) % 10];
        if (scroll == IT_SCROLL_IDENTIFY || scroll == IT_SCROLL_CURSE) {
          book_scroll = selection_scroll = scroll;
          book_empowered = true;
          transmute_open = true;
          transmute_sel = transmute_page = 0;
        } else {
          scroll_effect(scroll);
          spend_action(1);
        }
        armor_charge -= cost;
        invis_t = 0;
        return;
      }
      // Directed effects keep their temporary artifact state until the target
      // is committed, so cancellation spends neither armor charge nor a turn.
      if (target < 0) {
        magic_target = -4;
        artifact_targeting = targeting = true;
        target_item = -1;
        backpack = false;
        return;
      }
      if (!in_map(target % MW, target / MW) || !seen[target / MW][target % MW])
        return;
      const int x = target % MW, y = target / MW, victim = mob_at(x, y);
      if (item.id == IT_ART_SANDALS) {
        if (!vis[y][x] || dist(px, py, x, y) > 3)
          return;
        constexpr uint8_t seeds[] = {IT_SEED_BLINDWEED,  IT_SEED_FADELEAF,
                                     IT_SEED_FIREBLOOM,  IT_SEED_ICECAP,
                                     IT_SEED_SORROWMOSS, IT_SEED_STORMVINE};
        seed_effect(seeds[draw(*this) % 6], target);
      } else if (item.id == IT_ART_CHAINS) {
        if (!vis[y][x] ||
            dist(px, py, x, y) > 5 + 2 * std::lround(artifact_level * .5f))
          return;
        if (victim >= 0) {
          if (npc(mobs[victim]) || mobs[victim].effects.ally ||
              mob_def(mobs[victim].type).boss)
            return;
          const auto ray =
              cast_ray(py * MW + px, target, ballistica::stop_target);
          int destination = -1;
          for (int i = 1; i < ray.distance; ++i) {
            int p = ray.path[i];
            if (walkable(p % MW, p / MW) && mob_at(p % MW, p / MW) < 0) {
              destination = p;
              break;
            }
          }
          if (destination < 0)
            return;
          mobs[victim].x = mobs[victim].vx = destination % MW;
          mobs[victim].y = mobs[victim].vy = destination / MW;
        } else {
          if (!walkable(x, y) || target == py * MW + px)
            return;
          px = pvx = x;
          py = pvy = y;
          trigger_tile_features();
        }
      } else if (item.id == IT_ART_FORESIGHT) {
        const int radius = 5 + 2 * artifact_level;
        for (int cy = std::max(0, y - radius);
             cy <= std::min(map_height - 1, y + radius); ++cy)
          for (int cx = std::max(0, x - radius);
               cx <= std::min(map_width - 1, x + radius); ++cx) {
            seen[cy][cx] |= 2;
            if (tiles[cy][cx] == T_SECRET)
              tiles[cy][cx] = T_DOOR;
          }
        for (auto &feature : features)
          if (feature.type == F_TRAP &&
              dist(x, y, feature.x, feature.y) <= radius)
            feature.type = F_TRAP_FOUND;
      } else if (item.id == IT_ART_ARMBAND) {
        if (victim < 0 || npc(mobs[victim]) || mobs[victim].effects.ally ||
            dist(px, py, x, y) > 1)
          return;
        mobs[victim].effects.blind = mobs[victim].effects.chill =
            3 + artifact_level / 2;
      } else if (item.id == IT_ART_KEY) {
        const int feature = feature_at(x, y);
        if (dist(px, py, x, y) != 1)
          return;
        if (tiles[y][x] == T_DOOR_LOCKED && !boss_alive())
          tiles[y][x] = T_DOOR;
        else if (feature >= 0 && (features[feature].type == F_CHEST_LOCKED ||
                                  features[feature].type == F_CHEST_CRYSTAL))
          features[feature].type = F_CHEST;
        else
          return;
      } else {
        message("这个神器暂未支持灵体发动。");
        return;
      }
      armor_charge -= cost;
      artifact_targeting = targeting = false;
      invis_t = 0;
      spend_action(1);
      return;
    }
    spell_effects.spirit_ring = item.id;
    spell_effects.spirit_left = 20;
    update_maxhp();
  }
  armor_charge -= cost;
  invis_t = 0;
  spend_action(1);
}
void Game::tome_menu() {
  if (hero == 5 && boss_dead[1] && !cleric_subclass) {
    open_dialog(33, "牧师转职", "选择后解锁对应的三阶法术和天赋。", "祭司",
                "圣骑士", "取消");
    return;
  }
  if (hero == 5 && boss_dead[3] && !cleric_ability) {
    open_dialog(34, "护甲能力", "选择护甲能力及其四阶法术。", "升华形态",
                "三位一体", "众生之力", "取消");
    return;
  }
  cleric_page = 0;
  cleric_menu();
}
bool Game::cleric_unlocked(int spell) const {
  if (spell < 0 || spell >= cleric_rules::Count)
    return false;
  if (cleric_rules::branch(spell) &&
      cleric_rules::branch(spell) != cleric_subclass)
    return false;
  if (cleric_rules::ability(spell) &&
      cleric_rules::ability(spell) != cleric_ability)
    return false;
  return cleric_rules::innate(spell) || cleric_ranks[spell] > 0;
}
bool Game::cleric_learn(int spell) {
  if (hero != 5 || spell < cleric_rules::HolyIntuition ||
      spell >= cleric_rules::Count || cleric_rules::innate(spell))
    return false;
  const int tier = cleric_rules::tier(spell);
  if ((tier == 3 && !cleric_subclass) || (tier == 4 && !cleric_ability))
    return false;
  if (cleric_rules::branch(spell) &&
      cleric_rules::branch(spell) != cleric_subclass)
    return false;
  if (cleric_rules::ability(spell) &&
      cleric_rules::ability(spell) != cleric_ability)
    return false;
  if (cleric_ranks[spell] >= cleric_rules::rank_cap(spell))
    return false;
  int spent = 0;
  for (int i = 0; i < cleric_rules::Count; ++i)
    if (cleric_rules::tier(i) == tier)
      spent += cleric_ranks[i];
  if (cleric_rules::available(tier, level, spent) <= 0)
    return false;
  ++cleric_ranks[spell];
  return true;
}
void Game::cleric_menu(int mode) {
  cleric_menu_mode = mode;
  std::vector<int> list;
  for (int i = 0; i < cleric_rules::Count; ++i) {
    if (mode == 0 && i < cleric_rules::SatiatedSpells && cleric_unlocked(i))
      list.push_back(i);
    if (mode == 1 && !cleric_rules::innate(i) &&
        (!cleric_rules::branch(i) ||
         cleric_rules::branch(i) == cleric_subclass) &&
        (!cleric_rules::ability(i) ||
         cleric_rules::ability(i) == cleric_ability))
      list.push_back(i);
  }
  int pages = std::max(1, (int(list.size()) + 2) / 3);
  cleric_page %= pages;
  char labels[3][40]{};
  for (int n = 0; n < 3; ++n) {
    int index = cleric_page * 3 + n;
    cleric_choices[n] = index < int(list.size()) ? list[index] : -1;
    if (cleric_choices[n] >= 0)
      snprintf(labels[n], sizeof(labels[n]), mode ? "%s %d/%d" : "%s (%d)",
               cleric_rules::names[cleric_choices[n]],
               mode ? cleric_ranks[cleric_choices[n]]
                    : cleric_rules::cost(
                          cleric_choices[n],
                          std::max(1, int(cleric_ranks[cleric_choices[n]]))),
               cleric_rules::rank_cap(cleric_choices[n]));
    else
      snprintf(labels[n], sizeof(labels[n]), "—");
  }
  char text[240];
  snprintf(text, sizeof(text), "第%d/%d页。L/R翻页，Y返回。%s", cleric_page + 1,
           pages,
           mode ? "按按钮投入一个天赋点。等级2/7/13/21开始获得对应层级点数。"
                : "括号内为充能消耗。X栏为空时不会施法。按SELECT查看天赋。");
  open_dialog(mode ? 32 : 28, mode ? "牧师天赋" : "神圣法典", text, labels[0],
              labels[1], labels[2], "返回");
}
void Game::tome_cast(int spell, int target) {
  if (spell >= cleric_rules::SatiatedSpells || tome_slot() == -99 ||
      !cleric_unlocked(spell) || spell_effects.magic_immunity > 0)
    return;
  auto &tome = tome_state();
  const int rank = std::max(1, int(cleric_ranks[spell]));
  int cost = spell == cleric_rules::GuidingLight && cleric_subclass == 1 &&
                     cleric_priest_cooldown <= 0
                 ? 0
                 : cleric_rules::cost(spell, rank);
  if (spell == cleric_rules::Stasis && stasis_ally >= 0) {
    finish_tome_cast(spell, 0);
    release_stasis();
    return;
  }
  if (cleric_rules::ability(spell) == 1) {
    if (ascended_left <= 0) {
      message("需要先发动升华形态。");
      return;
    }
    if (spell == cleric_rules::DivineIntervention && divine_used)
      return;
    if (spell == cleric_rules::Flash)
      cost += ascended_flashes;
  }
  if (tome.aux < cost) {
    message("法典充能不足。");
    return;
  }
  if (spell >= cleric_rules::BodyForm && spell <= cleric_rules::SpiritForm) {
    cleric_page = 0;
    trinity_menu(spell);
    return;
  }
  if (spell == cleric_rules::RecallInscription) {
    if (recalled_item == IT_NONE || recall_left <= 0)
      return;
    const uint8_t remembered = recalled_item;
    int use = item_is_exotic(remembered)
                  ? (remembered == IT_SCROLL_ENCHANT ||
                             remembered == IT_SCROLL_METAMORPHOSIS
                         ? 8
                         : 4)
              : remembered == IT_SCROLL_TRANSMUTE      ? 6
              : item_def(remembered).cat == CAT_SCROLL ? 3
                                                       : 4;
    if (tome.aux < use)
      return;
    const bool selector =
        remembered == IT_SCROLL_IDENTIFY || remembered == IT_SCROLL_CURSE ||
        remembered == IT_SCROLL_UP || remembered == IT_SCROLL_TRANSMUTE ||
        remembered == IT_SCROLL_ENCHANT || remembered == IT_STONE_ENCHANT ||
        remembered == IT_STONE_AUG;
    if (selector) {
      selection_scroll = remembered;
      bool eligible = false;
      for (int i = -4; i < inv_n; ++i)
        eligible |= can_select_item(i);
      if (!eligible)
        return;
      book_scroll = remembered;
      book_empowered = inscription_replay = true;
      transmute_open = true;
      transmute_sel = transmute_page = 0;
    } else if (remembered == IT_SCROLL_SIREN) {
      magic_target = -1;
      book_scroll = remembered;
      book_empowered = inscription_replay = true;
      artifact_targeting = targeting = true;
      backpack = false;
    } else {
      if (!scroll_effect(remembered))
        return;
    }
    finish_tome_cast(spell, use);
    recalled_item = IT_NONE;
    recall_left = 0;
    if (!selector && remembered != IT_SCROLL_SIREN)
      spend_action(1);
    return;
  }
  if (spell == cleric_rules::HolyIntuition) {
    if (target < 0) {
      artifact_feed_slot = 101;
      selection_scroll = IT_ART_TOME;
      magic_target = spell;
      transmute_open = true;
      transmute_sel = transmute_page = 0;
      return;
    }
    const int index = target - 1;
    if (index < 0 || index >= inv_n || (inv[index].flags & IF_CURSE_KNOWN))
      return;
    inv[index].flags |= IF_CURSE_KNOWN;
  } else if (spell == cleric_rules::Cleanse) {
    hero_effects = {};
    std::fill_n(debuffs, status_rules::Count, 0);
    deferred_damage = 0;
    shield = std::max(shield, 10 * rank);
    spell_effects.cleanse = 2 * (rank - 1);
    for (size_t i = 0; i < mobs.size(); ++i)
      if (mobs[i].alive && mobs[i].effects.ally &&
          (vis[mobs[i].y][mobs[i].x] ||
           (int(i) == linked_ally && link_left > 0))) {
        mobs[i].effects = {};
        mobs[i].effects.ally = true;
        mobs[i].sleep_t = mobs[i].terror_t = 0;
        auto &effects = actor_effects(i);
        effects.vertigo = effects.deferred = 0;
        effects.cleanse = 2 * (rank - 1);
        effects.shield = std::max(effects.shield, float(10 * rank));
      }
  } else if (spell == 0 || spell == cleric_rules::Sunray ||
             spell == cleric_rules::HolyLance ||
             spell == cleric_rules::ShieldOfLight ||
             spell == cleric_rules::Smite || spell == cleric_rules::Flash ||
             spell == cleric_rules::LayOnHands ||
             spell == cleric_rules::Cleanse || spell == cleric_rules::Bless ||
             spell == cleric_rules::MnemonicPrayer ||
             spell == cleric_rules::WallOfLight ||
             spell == cleric_rules::HallowedGround ||
             spell == cleric_rules::BeamingRay) {
    if (spell == cleric_rules::HolyLance && cleric_lance_cooldown > 0) {
      message("神圣长枪尚未冷却。");
      return;
    }
    if (target < 0) {
      magic_target = spell;
      artifact_slot = tome_slot();
      targeting = artifact_targeting = true;
      target_item = -1;
      backpack = false;
      return;
    }
    if (!in_map(target % MW, target / MW))
      return;
    const bool self = target == py * MW + px;
    if (self &&
        (spell == cleric_rules::GuidingLight || spell == cleric_rules::Sunray ||
         spell == cleric_rules::HolyLance ||
         spell == cleric_rules::ShieldOfLight || spell == cleric_rules::Smite ||
         spell == cleric_rules::WallOfLight))
      return;
    const int direct = mob_at(target % MW, target / MW);
    if (spell == cleric_rules::BeamingRay) {
      const int ally = powered_ally >= 0 ? powered_ally : stasis_ally;
      if (ally < 0 || ally >= int(mobs.size()))
        return;
      auto &friend_mob = mobs[ally];
      if (!vis[target / MW][target % MW] ||
          !walkable(target % MW, target / MW) || direct >= 0 ||
          dist(friend_mob.x, friend_mob.y, target % MW, target / MW) > 4 * rank)
        return;
      friend_mob.x = friend_mob.vx = target % MW;
      friend_mob.y = friend_mob.vy = target / MW;
      friend_mob.alive = true;
      if (friend_mob.rose_ally)
        rose_ghost = ally;
      powered_ally = ally;
      stasis_ally = -1;
      stasis_left = 0;
      actor_effects(ally).haste = 10;
    } else if (spell == cleric_rules::Flash) {
      if (dist(px, py, target % MW, target / MW) > 2 + rank ||
          !vis[target / MW][target % MW] ||
          !walkable(target % MW, target / MW) || direct >= 0)
        return;
      px = pvx = target % MW;
      py = pvy = target / MW;
      trigger_tile_features();
    } else if (spell == cleric_rules::Smite) {
      if (direct < 0 || !melee_reaches(target % MW, target / MW) ||
          npc(mobs[direct]) || mobs[direct].effects.ally)
        return;
      attack(direct, false, normal(*this, 5 + level / 2, 10 + level));
    } else if (spell == cleric_rules::ShieldOfLight) {
      if (direct < 0 || !vis[target / MW][target % MW] || npc(mobs[direct]) ||
          mobs[direct].effects.ally)
        return;
      spell_effects.light_shield = 4;
      spell_effects.shield_target = direct;
      spell_effects.shield_rank = rank;
    } else if (spell == cleric_rules::Cleanse || spell == cleric_rules::Bless ||
               spell == cleric_rules::LayOnHands ||
               spell == cleric_rules::MnemonicPrayer) {
      if (!vis[target / MW][target % MW] || (!self && direct < 0))
        return;
      if (self) {
        if (spell == cleric_rules::Cleanse) {
          hero_effects = {};
          std::fill_n(debuffs, status_rules::Count, 0);
          shield = std::max(shield, 10 * rank);
          spell_effects.cleanse = 2 * (rank - 1);
        }
        if (spell == cleric_rules::Bless) {
          spell_effects.bless =
              std::max(spell_effects.bless, float(2 + 4 * rank));
          shield = std::max(shield, 5 + 5 * rank);
        }
        if (spell == cleric_rules::LayOnHands) {
          const int amount = 10 + 5 * rank;
          shield = std::min(3 * amount, shield + amount);
        }
        if (spell == cleric_rules::MnemonicPrayer) {
          for (float *value : {&spell_effects.bless, &spell_effects.holy_weapon,
                               &spell_effects.holy_ward, &spell_effects.aura})
            if (*value > 0)
              *value += 2 + rank;
        }
      } else {
        auto &recipient = mobs[direct];
        auto &effects = actor_effects(direct);
        if (spell == cleric_rules::Cleanse) {
          recipient.effects.poison = recipient.effects.bleeding =
              recipient.effects.burning = 0;
          recipient.effects.paralysis = recipient.effects.blind = 0;
        }
        if (spell == cleric_rules::Bless) {
          const int heal = 5 + 5 * rank;
          effects.bless = std::max(effects.bless, float(heal));
          effects.shield = std::max(
              effects.shield,
              float(std::max(0, heal - (recipient.maxhp - recipient.hp))));
          recipient.hp = std::min(recipient.maxhp, recipient.hp + heal);
        }
        if (spell == cleric_rules::LayOnHands) {
          if (dist(px, py, recipient.x, recipient.y) > 1)
            return;
          const int heal = 10 + 5 * rank;
          effects.shield = std::min(
              float(3 * heal),
              effects.shield +
                  std::max(0, heal - (recipient.maxhp - recipient.hp)));
          recipient.hp = std::min(recipient.maxhp, recipient.hp + heal);
        }
        if (spell == cleric_rules::MnemonicPrayer) {
          float extension = 2 + rank;
          recipient.effects.blind +=
              recipient.effects.blind > 0 ? extension : 0;
          recipient.effects.roots +=
              recipient.effects.roots > 0 ? extension : 0;
          effects.bless += effects.bless > 0 ? extension : 0;
        }
      }
    } else if (spell == cleric_rules::WallOfLight ||
               spell == cleric_rules::HallowedGround) {
      const int x = target % MW, y = target / MW;
      if (!vis[y][x] || !walkable(x, y))
        return;
      if (spell == cleric_rules::HallowedGround) {
        spell_effects.hallowed = 20;
        spell_effects.hallowed_cell = target;
        spell_effects.hallowed_rank = rank;
        for (int cy = y - rank; cy <= y + rank; ++cy)
          for (int cx = x - rank; cx <= x + rank; ++cx)
            if (in_map(cx, cy) && walkable(cx, cy)) {
              if (tiles[cy][cx] == T_FLOOR)
                tiles[cy][cx] = T_GRASS;
              int victim = mob_at(cx, cy);
              if (victim >= 0) {
                if (mobs[victim].effects.ally)
                  mobs[victim].hp =
                      std::min(mobs[victim].maxhp, mobs[victim].hp + 15);
                else
                  mobs[victim].effects.roots = 2;
              }
              if (cx == px && cy == py)
                shield = std::min(30, shield + 15);
            }
      } else {
        const int dx = (x > px) - (x < px), dy = (y > py) - (y < py);
        for (int offset = -rank; offset <= rank; ++offset) {
          int cx = px + dx - dy * offset, cy = py + dy + dx * offset;
          if (in_map(cx, cy) && walkable(cx, cy) && mob_at(cx, cy) < 0)
            features.push_back({cx, cy, F_KEY_WALL, 20});
        }
      }
    } else {
      const int cell =
          cast_ray(py * MW + px, target,
                   spell == cleric_rules::HolyLance ? ballistica::projectile
                                                    : ballistica::magic_bolt)
              .collision;
      const int enemy = mob_at(cell % MW, cell / MW);
      if (enemy >= 0 && (npc(mobs[enemy]) || mobs[enemy].effects.ally))
        return;
      if (spell == cleric_rules::HolyLance)
        cleric_lance_cooldown = 30;
      if (enemy >= 0) {
        int lo = 2, hi = 8;
        if (spell == cleric_rules::Sunray) {
          lo = rank == 2 ? 6 : 4;
          hi = lo * 2;
        }
        if (spell == cleric_rules::HolyLance) {
          lo = 15 + 15 * rank;
          hi = std::lround(27.5f + 27.5f * rank);
          cleric_lance_cooldown = 30;
        }
        damage_mob(enemy, normal(*this, lo, hi));
        auto &effect = actor_effects(enemy);
        if (mobs[enemy].alive &&
            (spell == cleric_rules::GuidingLight ||
             spell == cleric_rules::HolyLance || cleric_subclass == 1)) {
          effect.illuminated = true;
          if (spell == cleric_rules::GuidingLight)
            effect.was_illuminated = true;
        }
        if (spell == cleric_rules::Sunray && mobs[enemy].alive) {
          if (mobs[enemy].effects.blind > 0 && effect.sunray_recent > 0) {
            mobs[enemy].effects.paralysis = 2 + 2 * rank;
            effect.sunray_recent = 0;
          } else if (!effect.sunray_used) {
            mobs[enemy].effects.blind = 2 + 2 * rank;
            effect.sunray_recent = 2 + 2 * rank;
            effect.sunray_used = true;
          }
        }
      } else
        trigger_trap(feature_at(cell % MW, cell / MW));
    }
  } else if (spell == 1)
    spell_effects.holy_weapon = 50;
  else if (spell == 2)
    spell_effects.holy_ward = 50;
  else if (spell == cleric_rules::DivineSense) {
    spell_effects.light = 50;
    mind_t = 50;
  } else if (spell == cleric_rules::Radiance) {
    if (fov_radius < 6)
      spell_effects.light = 100;
    for (size_t i = 0; i < mobs.size(); ++i)
      if (mobs[i].alive && !npc(mobs[i]) && !mobs[i].effects.ally &&
          vis[mobs[i].y][mobs[i].x]) {
        if (actor_effects(i).illuminated)
          damage_mob(i, level + 5);
        else
          actor_effects(i).illuminated = actor_effects(i).was_illuminated =
              true;
        if (mobs[i].alive)
          mobs[i].effects.paralysis = 3;
      }
  } else if (spell == cleric_rules::AuraOfProtection) {
    spell_effects.aura = 20;
    spell_effects.aura_rank = rank;
  } else if (spell == cleric_rules::DivineIntervention) {
    spell_effects.divine = 100 + 50 * rank;
    divine_used = true;
    ascended_left += 2 + rank;
    for (size_t i = 0; i < mobs.size(); ++i)
      if (mobs[i].alive && mobs[i].effects.ally)
        actor_effects(i).divine = 100 + 50 * rank;
  } else if (spell == cleric_rules::Judgement) {
    int base = 5 + 5 * rank;
    base += std::lround(base * ascended_casts / 3.f);
    for (size_t i = 0; i < mobs.size(); ++i)
      if (mobs[i].alive && !npc(mobs[i]) && !mobs[i].effects.ally &&
          vis[mobs[i].y][mobs[i].x]) {
        damage_mob(i, normal(*this, base, 2 * base));
        actor_effects(i).illuminated = true;
      }
    ascended_casts = 0;
  } else if (spell == cleric_rules::LifeLink) {
    if (powered_ally < 0 || powered_ally >= int(mobs.size()) ||
        !mobs[powered_ally].alive)
      return;
    linked_ally = powered_ally;
    link_left = std::lround(6.67f + 3.33f * rank);
  } else if (spell == cleric_rules::Stasis) {
    if (stasis_ally >= 0) {
      stasis_left = .001f;
      return;
    }
    if (powered_ally < 0 || powered_ally >= int(mobs.size()) ||
        !mobs[powered_ally].alive)
      return;
    stasis_ally = powered_ally;
    stasis_left = 30 + 30 * rank;
    mobs[stasis_ally].alive = false;
    if (rose_ghost == stasis_ally)
      rose_ghost = -1;
  } else {
    message("这个法术需要尚未移植的职业能力。");
    return;
  }
  finish_tome_cast(spell, cost);
  if (spell != 1 && spell != 2 && spell != cleric_rules::ShieldOfLight &&
      spell != cleric_rules::DivineSense && spell != cleric_rules::LayOnHands)
    spend_action(1);
}
void Game::finish_tome_cast(int spell, int cost) {
  auto &tome = tome_state();
  auto &lvl = tome_level();
  if (spell_effects.satiated_spell &&
      cleric_ranks[cleric_rules::SatiatedSpells]) {
    const int amount = 1 + 2 * cleric_ranks[cleric_rules::SatiatedSpells];
    shield = std::max(shield, amount);
    if (linked_ally >= 0 && linked_ally < int(mobs.size()) && link_left > 0)
      actor_effects(linked_ally).shield =
          std::max(actor_effects(linked_ally).shield, float(amount));
    spell_effects.satiated_spell = false;
  }
  tome.aux -= cost;
  if (tome_slot() >= 0)
    inv[tome_slot()].charges = tome.aux;
  const int diff = level - (1 + 2 * lvl) - (lvl >= 7 ? lvl - 6 : 0);
  tome.experience += std::lround(
      cost * 10 * (diff >= 0 ? std::pow(1.1f, diff) : std::pow(.75f, -diff)));
  while (lvl < 10 && tome.experience >= (lvl + 1) * 50) {
    tome.experience -= (lvl + 1) * 50;
    ++lvl;
  }
  dispel_action();
  if (ascended_left > 0) {
    ++ascended_casts;
    if (spell == cleric_rules::Judgement)
      ascended_casts = 0;
    if (spell == cleric_rules::Flash)
      ++ascended_flashes;
    if (spell != cleric_rules::DivineIntervention)
      spell_effects.divine += 10 * cost;
  }
  if (spell == 0 && cleric_subclass == 1 && cost == 0)
    cleric_priest_cooldown = 50;
  if (cleric_subclass == 2) {
    if (spell != 1 && spell_effects.holy_weapon > 0)
      spell_effects.holy_weapon =
          std::min(100.f, spell_effects.holy_weapon + 10 * cost);
    if (spell != 2 && spell_effects.holy_ward > 0)
      spell_effects.holy_ward =
          std::min(100.f, spell_effects.holy_ward + 10 * cost);
  }
}
int Game::tome_slot() const {
  if (ring1 == IT_ART_TOME && !(ring1_flags & IF_CURSED))
    return -3;
  if (ring2 == IT_ART_TOME && !(ring2_flags & IF_CURSED))
    return -4;
  if (cleric_ranks[cleric_rules::LightReading])
    for (int i = 0; i < inv_n; ++i)
      if (inv[i].id == IT_ART_TOME && !(inv[i].flags & IF_CURSED))
        return i;
  return -99;
}
magic_rules::State &Game::tome_state() {
  const int slot = tome_slot();
  return slot == -3   ? ring1_magic
         : slot == -4 ? ring2_magic
                      : inv[std::max(0, slot)].magic;
}
uint8_t &Game::tome_level() {
  const int slot = tome_slot();
  return slot == -3   ? ring1_level
         : slot == -4 ? ring2_level
                      : inv[std::max(0, slot)].level;
}
void Game::cleric_food() {
  if (hero != 5)
    return;
  spell_effects.satiated_spell = cleric_ranks[cleric_rules::SatiatedSpells] > 0;
  if (cleric_ranks[cleric_rules::EnlighteningMeal]) {
    int slot = tome_slot();
    if (slot == -99)
      for (int i = 0; i < inv_n; ++i)
        if (inv[i].id == IT_ART_TOME && !(inv[i].flags & IF_CURSED)) {
          slot = i;
          break;
        }
    if (slot != -99) {
      auto &state = slot == -3   ? ring1_magic
                    : slot == -4 ? ring2_magic
                                 : inv[slot].magic;
      const int lvl = slot == -3   ? ring1_level
                      : slot == -4 ? ring2_level
                                   : inv[slot].level;
      state.stored += (1 + cleric_ranks[cleric_rules::EnlighteningMeal]) / 3.f;
      const int cap = artifact_rules::capacity(IT_ART_TOME, lvl);
      while (state.stored >= 1 && state.aux < cap) {
        ++state.aux;
        --state.stored;
      }
      if (state.aux >= cap)
        state.stored = 0;
      if (slot >= 0)
        inv[slot].charges = state.aux;
    }
  }
}
void Game::cleric_armor_action(int target) {
  if (hero != 5 || cleric_ability == 0 || !boss_dead[3] ||
      spell_effects.magic_immunity > 0)
    return;
  if (cleric_ability == 1) {
    constexpr float factors[] = {1.f, .88f, .77f, .68f, .6f};
    const float cost = 50 * factors[cleric_ranks[cleric_rules::HeroicEnergy]];
    if (armor_charge < cost) {
      message("护甲充能不足。");
      return;
    }
    armor_charge -= cost;
    ascended_left = 10;
    spell_effects.divine = 30;
    ascended_casts = ascended_flashes = 0;
    divine_used = false;
    spend_action(1);
    return;
  }
  if (cleric_ability == 2) {
    open_dialog(36, "三位一体", "发动已灌注形态；消耗护甲充能。", "躯体形态",
                "心智形态", "灵体形态", "取消");
    return;
  }
  if (stasis_ally >= 0) {
    message("盟友仍在停滞中。");
    return;
  }
  if (powered_ally >= 0 && powered_ally < int(mobs.size()) &&
      mobs[powered_ally].alive) {
    if (target >= 0)
      mobs[powered_ally].target = target;
    return;
  }
  constexpr float factors[] = {1.f, .88f, .77f, .68f, .6f};
  const float cost = 25 * factors[cleric_ranks[cleric_rules::HeroicEnergy]];
  if (armor_charge < cost) {
    message("护甲充能不足。");
    return;
  }
  if (target < 0) {
    magic_target = -2;
    artifact_slot = ring1 == IT_ART_TOME ? -3 : -4;
    targeting = artifact_targeting = true;
    backpack = false;
    return;
  }
  const int x = target % MW, y = target / MW;
  if (!in_map(x, y) || !vis[y][x] || !walkable(x, y))
    return;
  int index = mob_at(x, y);
  if (index >= 0 && !mobs[index].effects.ally)
    return;
  if (index < 0) {
    Mob ally;
    ally.type = M_WRAITH;
    ally.x = ally.vx = x;
    ally.y = ally.vy = y;
    ally.hp = ally.maxhp = 80;
    ally.atk = level;
    ally.effects.ally = true;
    ally.next_action = actor_time + 1;
    index = mobs.size();
    mobs.push_back(ally);
    actor_effects(index).light_ally = true;
  }
  powered_ally = index;
  actor_effects(index).power_left = 100;
  actor_effects(index).shield = std::max(25.f, actor_effects(index).shield);
  armor_charge -= cost;
  spend_action(1);
}
void Game::rose_attack(int target) {
  if (rose_ghost < 0 || rose_ghost >= int(mobs.size()) || target < 0 ||
      target >= int(mobs.size()))
    return;
  auto &ghost = mobs[rose_ghost];
  auto &enemy = mobs[target];
  const int str = 13 + artifact_level(IT_ART_ROSE) / 2;
  float accuracy = (9 + level) * hero_rules::weapon_accuracy(rose_weapon.id);
  float evasion = enemy.sleep_t ? 0.f : mob_def(enemy.type).evasion;
  if (roll(*this) * accuracy < roll(*this) * evasion)
    return;
  auto range = rose_weapon.id == IT_NONE
                   ? hero_rules::DamageRange{1, str - 8}
                   : hero_rules::weapon_range(rose_weapon.id, rose_weapon.level,
                                              str, false);
  int damage = normal(*this, range.min, range.max);
  if (rose_weapon.id != IT_NONE)
    damage += normal(*this, 0,
                     std::max(0, str - hero_rules::strength_requirement(
                                           rose_weapon.id, rose_weapon.level)));
  const int old_px = px, old_py = py, old_hp = hp, old_maxhp = maxhp,
            old_shield = shield;
  const int old_actor = magic_actor;
  const int old_hero_cell = magic_hero_cell;
  if (old_actor < 0)
    magic_hero_cell = py * MW + px;
  magic_actor = rose_ghost;
  const uint8_t old_weapon = weapon, old_level = weapon_level,
                old_augment = weapon_augment;
  auto old_magic = weapon_magic;
  auto old_status = hero_effects;
  auto old_spells = spell_effects;
  const float old_deferred = deferred_damage, old_kinetic = kinetic_damage;
  const int old_invis = invis_t;
  auto &ghost_effect = actor_effects(rose_ghost);
  deferred_damage = ghost_effect.deferred;
  kinetic_damage = ghost_effect.kinetic;
  px = ghost.x;
  py = ghost.y;
  hp = ghost.hp;
  maxhp = ghost.maxhp;
  shield = 0;
  weapon = rose_weapon.id;
  weapon_level = rose_weapon.level;
  weapon_augment = rose_weapon.augment;
  weapon_magic = rose_weapon.magic;
  hero_effects = ghost.effects;
  spell_effects = {};
  int effect = 0;
  damage = weapon_proc(target, damage, effect);
  damage_mob(target, damage, DamageKind::Physical, effect);
  ghost.hp = hp;
  ghost.effects = hero_effects;
  rose_weapon.magic = weapon_magic;
  if (weapon == IT_NONE)
    rose_weapon = {};
  actor_effects(magic_actor).deferred = deferred_damage;
  actor_effects(magic_actor).kinetic = kinetic_damage;
  actor_effects(magic_actor).shield =
      std::max(actor_effects(magic_actor).shield, float(shield));
  if (ghost.hp <= 0) {
    ghost.alive = false;
    rose_ghost = -1;
  }
  px = old_px;
  py = old_py;
  hp = old_hp;
  maxhp = old_maxhp;
  shield = old_shield;
  weapon = old_weapon;
  weapon_level = old_level;
  weapon_augment = old_augment;
  weapon_magic = old_magic;
  hero_effects = old_status;
  spell_effects = old_spells;
  magic_actor = old_actor;
  deferred_damage = old_deferred;
  kinetic_damage = old_kinetic;
  invis_t = old_invis;
  magic_hero_cell = old_hero_cell;
  finish_ally_combat();
}
void Game::rose_defend(int attacker, int &damage) {
  if (rose_ghost < 0 || rose_ghost >= int(mobs.size()) || attacker < 0 ||
      attacker >= int(mobs.size()) || damage <= 0)
    return;
  auto &ghost = mobs[rose_ghost];
  const int old_px = px, old_py = py, old_hp = hp, old_maxhp = maxhp,
            old_shield = shield, old_actor = magic_actor;
  const uint8_t old_armor = armor, old_level = armor_level;
  const auto old_magic = armor_magic;
  const auto old_status = hero_effects;
  const auto old_spells = spell_effects;
  const float old_deferred = deferred_damage;
  deferred_damage = actor_effects(rose_ghost).deferred;
  px = ghost.x;
  py = ghost.y;
  hp = ghost.hp;
  maxhp = ghost.maxhp;
  shield = 0;
  magic_actor = rose_ghost;
  armor = rose_armor.id;
  armor_level = rose_armor.level;
  armor_magic = rose_armor.magic;
  hero_effects = ghost.effects;
  spell_effects = {};
  damage = armor_proc(mobs[attacker], damage, false);
  ghost.hp = hp;
  ghost.effects = hero_effects;
  ghost.x = ghost.vx = px;
  ghost.y = ghost.vy = py;
  rose_armor.magic = armor_magic;
  auto &effects = actor_effects(rose_ghost);
  effects.shield = std::max(effects.shield, float(shield));
  effects.deferred = deferred_damage;
  deferred_damage = old_deferred;
  if (ghost.hp <= 0) {
    ghost.alive = false;
    rose_ghost = -1;
  }
  px = old_px;
  py = old_py;
  hp = old_hp;
  maxhp = old_maxhp;
  shield = old_shield;
  magic_actor = old_actor;
  armor = old_armor;
  armor_level = old_level;
  armor_magic = old_magic;
  hero_effects = old_status;
  spell_effects = old_spells;
}
void Game::carry_allies(bool depart) {
  if (depart) {
    end_time_bubble(true);
    mobs.insert(mobs.end(), pending_swarms.begin(), pending_swarms.end());
    pending_swarms.clear();
    carried_allies.clear();
    for (size_t i = 0; i < mobs.size(); ++i) {
      const auto effect = actor_effects(i);
      const bool held = int(i) == stasis_ally && stasis_left > 0;
      const bool follows =
          mobs[i].rose_ally || effect.light_ally ||
          (effect.prismatic > 0 &&
           (int(i) == powered_ally || dist(px, py, mobs[i].x, mobs[i].y) <= 5));
      if (!held && (!mobs[i].alive || !mobs[i].effects.ally || !follows))
        continue;
      CarriedAlly ally;
      ally.mob = mobs[i];
      ally.effects = effect;
      ally.mob.next_action =
          std::max(0.f, ally.mob.next_action - action_remainder);
      ally.mob.owner = ally.mob.minion = ally.mob.summoning_pos = -1;
      ally.powered = int(i) == powered_ally;
      ally.linked = int(i) == linked_ally;
      ally.stasis = held;
      carried_allies.push_back(ally);
      mobs[i].alive = false;
    }
    rose_ghost = powered_ally = linked_ally = stasis_ally = -1;
    rose_command = rose_enemy = -1;
    spell_effects.light_shield = 0;
    spell_effects.shield_target = -1;
    spell_effects.hallowed = 0;
    spell_effects.hallowed_cell = -1;
    spell_effects.arena = 0;
    spell_effects.arena_cell = -1;
    spell_effects.healing = spell_effects.earth = spell_effects.partial_heal =
        0;
    spell_effects.anchor = spell_effects.earth_anchor = -1;
    return;
  }
  for (const auto &carried : carried_allies) {
    auto ally = carried.mob;
    bool placed = false;
    for (int y = py - 1; y <= py + 1 && !placed; ++y)
      for (int x = px - 1; x <= px + 1 && !placed; ++x)
        if (walkable(x, y) && mob_at(x, y) < 0 && !(x == px && y == py)) {
          ally.x = ally.vx = x;
          ally.y = ally.vy = y;
          placed = true;
        }
    if (!placed) {
      // Preserve an ally even when every arrival neighbour is occupied.
      // Actors can temporarily share the stairs until their next move.
      ally.x = ally.vx = px;
      ally.y = ally.vy = py;
    }
    ally.next_action += action_remainder;
    ally.target = -1;
    ally.ai = Mob::Wandering;
    ally.alive = !carried.stasis;
    const int index = mobs.size();
    mobs.push_back(ally);
    actor_effects(index) = carried.effects;
    if (ally.rose_ally && ally.alive)
      rose_ghost = index;
    if (carried.powered)
      powered_ally = index;
    if (carried.linked)
      linked_ally = index;
    if (carried.stasis)
      stasis_ally = index;
  }
  if (linked_ally < 0)
    link_left = 0;
  if (stasis_ally < 0)
    stasis_left = 0;
  carried_allies.clear();
}
void Game::end_time_bubble(bool disarm) {
  // Clear before triggering: a queued Swiftthistle may create a new bubble.
  if(!disarm) {
    spell_effects.bubble_active = false;
    spell_effects.time_bubble = 0;
  }
  resolve_time_presses(disarm);
}
void Game::end_hourglass() {
  const bool active=spell_effects.hourglass_active;
  spell_effects.hourglass_active = false;
  freeze_left = 0;
  if(active)resolve_time_presses();
}
void Game::resolve_time_presses(bool disarm) {
  auto presses = std::move(bubble_presses);
  bubble_presses.clear();
  for (int cell : presses) {
    const int index = feature_at(cell % MW, cell / MW);
    if (index < 0) continue;
    const auto feature = features[index];
    if (disarm) {
      if (feature.type == F_PLANT && feature.aux != IT_SEED_ROTBERRY)
        features.erase(features.begin() + index);
      else if (trap_feature(feature.type)) features[index].type = F_TRAP_DISARMED;
    } else if (feature.type == F_PLANT) {
      features.erase(features.begin() + index);
      seed_effect(feature.aux, cell);
    } else if (trap_feature(feature.type)) {
      // Trigger at the original cell, allowing an occupant to take the hit.
      const int actor = mob_at(feature.x, feature.y);
      if(actor<0 && (px!=feature.x || py!=feature.y) &&
          trap_kind(feature)!=TR_TOXIC && trap_kind(feature)!=TR_ALARM) {
        features[index].type=F_TRAP_DISARMED;
        continue;
      }
      trigger_trap(index, actor >= 0 ? &mobs[actor] : nullptr);
    }
  }
}
void Game::dispel_action() {
  invis_t = 0;
  spell_effects.hourglass_active=false;
  freeze_left=0;
  end_time_bubble();
}
void Game::cancel_item_selection() {
  transmute_open = false;
  artifact_feed_slot = 0;
  book_scroll = IT_NONE;
  book_empowered = inscription_replay = false;
  selection_scroll = IT_NONE;
}
void Game::rose_menu() {
  open_dialog(29, "幽灵装备",
              "幽灵力量为13加神器等级的一半。只能使用已确认无诅咒且满足力量需求"
              "的近战武器和护甲。",
              "交付装备", "跟随", "归还装备", "取消");
}
void Game::rose_outfit(int index) {
  if (index < 0 || index >= inv_n || !has_artifact(IT_ART_ROSE))
    return;
  const Item item = inv[index];
  const bool weapon_item = item_def(item.id).kind == K_WEAPON;
  if (!weapon_item && item_def(item.id).kind != K_ARMOR)
    return;
  const int str = 13 + artifact_level(IT_ART_ROSE) / 2;
  if (!(item.flags & IF_CURSE_KNOWN) || (item.flags & IF_CURSED) ||
      (!(item.flags & IF_IDENTIFIED) &&
       hero_rules::strength_requirement(item.id, 0) > str) ||
      hero_rules::strength_requirement(item.id, item.level) > str) {
    message("幽灵无法使用这件装备。");
    return;
  }
  auto &equipped = weapon_item ? rose_weapon : rose_armor;
  if (equipped.id == IT_NONE) {
    equipped = item;
    remove_item(index);
  } else {
    inv[index] = equipped;
    equipped = item;
  }
  transmute_open = false;
  artifact_feed_slot = 0;
  message("幽灵更换了装备。");
}
void Game::throw_projectile(int index, int x, int y) {
  if (index < 0 || index >= inv_n ||
      item_def(inv[index].id).kind != K_MISSILE || !in_map(x, y))
    return;
  Item thrown = inv[index];
  thrown.count = 1;
  const int from = py * MW + px, target = y * MW + x;
  if (target == from)
    return;
  const bool projecting =
      thrown.magic.effect == magic_rules::Projecting &&
      dist(px, py, x, y) <= std::lround(4 * enchant_power());
  const auto ray = cast_ray(from, target, ballistica::projectile);
  int cell = returning_projectile || projecting ? target : ray.collision;
  x = cell % MW;
  y = cell / MW;
  if (cell == from)
    return;
  const int victim = mob_at(x, y);
  if (victim >= 0 && (npc(mobs[victim]) || mobs[victim].effects.ally)) {
    message("不能向盟友投掷武器。");
    return;
  }
  combat_strike(hero_combat, px, py, x, y, CombatStyle::Throw, thrown.id);
  const int missing =
      std::max(0, projectile_rules::strength(thrown.id, thrown.level) -
                      effective_strength());
  const float delay = std::pow(1.2f, missing) * (thrown.augment == 1   ? 2.f / 3
                                                 : thrown.augment == 2 ? 5.f / 3
                                                                       : 1.f);
  const bool instant =
      thrown.id == IT_SHURIKEN && spell_effects.shuriken_cooldown <= 0;
  bool hit = false;
  std::vector<int> victims;
  if (thrown.id == IT_FORCE_CUBE && tiles[y][x] != T_CHASM) {
    for (size_t i = 0; i < mobs.size(); ++i)
      if (mobs[i].alive && !npc(mobs[i]) &&
          dist(x, y, mobs[i].x, mobs[i].y) <= 1)
        victims.push_back(i);
    for (int cy = y - 1; cy <= y + 1; ++cy)
      for (int cx = x - 1; cx <= x + 1; ++cx)
        if (in_map(cx, cy))
          trigger_trap(feature_at(cx, cy));
    if (dist(x, y, px, py) <= 1) {
      const auto range = projectile_rules::damage(thrown.id, thrown.level);
      take_damage(normal(*this, range.min, range.max));
    }
    hit = true;
  } else if (victim >= 0)
    victims.push_back(victim);
  for (int victim_index : victims) {
    auto &enemy = mobs[victim_index];
    const bool surprise = enemy.sleep_t || enemy.ai != Mob::Hunting || invis_t;
    const float accuracy =
        (9 + level) * std::pow(1.3f, accessory_bonus(IT_RING_ACCURACY)) /
        std::pow(1.5f, missing) *
        (dist(px, py, x, y) <= 1 && !returning_projectile ? .5f : 1.5f);
    const float evasion = surprise && !missing ? 0.f
                                               : mob_def(enemy.type).evasion *
                                                     mob_modifier(enemy);
    const bool target_hit = roll(*this) * accuracy >= roll(*this) * evasion;
    hit |= target_hit;
    if (target_hit) {
      const int damage_level =
          buffed_level(thrown.level) + accessory_bonus(IT_RING_SHARPSHOOTING);
      const auto range = projectile_rules::damage(
          thrown.id, damage_level, surprise,
          weapon == IT_CROSSBOW ? buffed_level(weapon_level) : -1);
      int damage = normal(*this, range.min, range.max) +
                   normal(*this, 0,
                          std::max(0, effective_strength() -
                                          projectile_rules::strength(
                                              thrown.id, thrown.level)));
      damage = int(std::lround(damage * (thrown.augment == 1   ? .7f
                                         : thrown.augment == 2 ? 1.5f
                                                               : 1.f)));
      auto old_magic = weapon_magic;
      auto old_weapon = weapon, old_level = weapon_level;
      const auto old_augment = weapon_augment;
      const bool crossbow_enchant =
          projectile_rules::dart(thrown.id) && old_weapon == IT_CROSSBOW;
      int crossbow_effect = 0;
      if (crossbow_enchant) {
        projectile_proc = true;
        damage = weapon_proc(victim_index, damage, crossbow_effect);
        projectile_proc = false;
        old_magic = weapon_magic;
        old_weapon = weapon;
        old_level = weapon_level;
      }
      weapon = thrown.id;
      weapon_level = thrown.level;
      weapon_augment = thrown.augment;
      weapon_magic = thrown.magic;
      projectile_proc = true;
      int effect = 0;
      damage = weapon_proc(victim_index, damage, effect);
      damage_mob(victim_index, damage, DamageKind::Physical,
                 effect | crossbow_effect);
      thrown.magic = weapon_magic;
      projectile_proc = false;
      weapon_magic = old_magic;
      weapon = old_weapon;
      weapon_level = old_level;
      weapon_augment = old_augment;
      // Crossbow enchantments are processed before final missile damage, so
      // deferred Grim/Kinetic/Corrupting procs see the same hit.
      if (thrown.id == IT_BOLAS)
        enemy.effects.chill = 5;
      if (thrown.id == IT_TOMAHAWK)
        enemy.effects.bleeding =
            3 + thrown.level / 2.f +
            (roll(*this) + roll(*this)) * .5f * (3 + thrown.level / 2.f);
      if (thrown.id == IT_DART_TIPPED)
        enemy.sleep_t = 8;
      identify_use(thrown.flags, thrown.identification, 10);
      inv[index].flags = thrown.flags;
      inv[index].identification = thrown.identification;
    }
  }
  if (hit) {
    const float cost = projectile_rules::durability_cost(
        thrown.id, thrown.level, accessory_bonus(IT_RING_SHARPSHOOTING),
        thrown.augment, thrown.magic.effect);
    if (inv[index].count > 1) {
      if (inv[index].magic.durability <= cost) {
        thrown.magic.durability = 0;
        inv[index].magic.durability = 100;
      } else
        inv[index].magic.durability -= cost;
    } else
      thrown.magic.durability = std::max(0.f, thrown.magic.durability - cost);
  }
  if (hit && thrown.id == IT_DART_TIPPED)
    thrown.id = IT_DART;
  remove_item(index);
  if (thrown.magic.durability > 0 && !virtual_projectile) {
    FloorItem drop{x, y, thrown.id, thrown.level, thrown.flags};
    drop.magic = thrown.magic;
    drop.augment = thrown.augment;
    drop.identification = thrown.identification;
    // aux stores pin ownership (index+1); boomerangs use a negative return
    // cell.
    drop.magic.aux = hit && victim >= 0 && mobs[victim].alive &&
                             projectile_rules::sticky(thrown.id)
                         ? victim + 1
                         : 0;
    if (thrown.id == IT_BOOMERANG && !returning_projectile) {
      drop.magic.aux = -from - 1;
      drop.magic.cooldown = 5;
    }
    items.push_back(drop);
  }
  if(!returning_projectile)dispel_action();
  targeting = false;
  target_item = -1;
  message(hit ? "投掷武器命中。" : "投掷武器落在了地上。");
  if (instant)
    spell_effects.shuriken_cooldown = 19;
  if (!instant && !returning_projectile)
    spend_action(delay / std::pow(1.09051f, accessory_bonus(IT_RING_FUROR)));
}
void Game::tick_extended_effects() {
  armor_charge = std::min(100.f, armor_charge + .2f);
  if (ascended_left > 0 && --ascended_left <= 0) {
    spell_effects.divine = 0;
    divine_used = false;
    for (auto &effect : mob_effects)
      effect.divine = 0;
  }
  cleric_lance_cooldown = std::max(0.f, cleric_lance_cooldown - 1);
  cleric_priest_cooldown = std::max(0.f, cleric_priest_cooldown - 1);
  recall_left = std::max(0.f, recall_left - 1);
  if (recall_left == 0)
    recalled_item = IT_NONE;
  if (stasis_ally != linked_ally)
    link_left = std::max(0.f, link_left - 1);
  if (link_left == 0)
    linked_ally = -1;
  if (stasis_ally >= 0 && stasis_left > 0 && --stasis_left <= 0)
    release_stasis();
  if (spell_effects.foresight > 0) {
    for (int y = std::max(0, py - 8); y <= std::min(map_height - 1, py + 8);
         ++y)
      for (int x = std::max(0, px - 8); x <= std::min(map_width - 1, px + 8);
           ++x) {
        seen[y][x] |= 2;
        if (tiles[y][x] == T_SECRET)
          tiles[y][x] = T_DOOR;
      }
    for (auto &f : features)
      if (f.type == F_TRAP && dist(px, py, f.x, f.y) <= 8)
        f.type = F_TRAP_FOUND;
  }
  if (spell_effects.cleanse > 0) {
    hero_effects = {};
    deferred_damage = 0;
    spell_effects.vertigo = 0;
    std::fill_n(debuffs, status_rules::Count, 0);
  }
  if (spell_effects.hallowed > 0) {
    const int x = spell_effects.hallowed_cell % MW,
              y = spell_effects.hallowed_cell / MW,
              range = spell_effects.hallowed_rank;
    if (dist(px, py, x, y) <= range && roll(*this) < (10 + 10 * range) / 100.f)
      ++shield;
    for (auto &enemy : mobs)
      if (enemy.alive && !enemy.effects.ally &&
          dist(x, y, enemy.x, enemy.y) <= range)
        enemy.effects.chill = std::max(1.f, enemy.effects.chill);
  }
  if (spell_effects.prismatic_guard > 0) {
    const int maximum = 10 + int(std::floor(level * 2.5f));
    if (!boss_alive())
      spell_effects.prismatic_guard =
          std::min(float(maximum), spell_effects.prismatic_guard + .1f);
    bool nearby = false;
    for (const auto &enemy : mobs)
      nearby |= enemy.alive && !npc(enemy) && !enemy.effects.ally &&
                vis[enemy.y][enemy.x] && dist(px, py, enemy.x, enemy.y) < 5;
    if (nearby)
      for (int y = py - 1; y <= py + 1 && spell_effects.prismatic_guard > 0;
           ++y)
        for (int x = px - 1; x <= px + 1 && spell_effects.prismatic_guard > 0;
             ++x)
          if (walkable(x, y) && mob_at(x, y) < 0 && !(x == px && y == py)) {
            Mob image;
            image.type = M_WRAITH;
            image.x = image.vx = x;
            image.y = image.vy = y;
            image.effects.ally = true;
            image.hp = int(spell_effects.prismatic_guard);
            image.maxhp = maximum;
            image.next_action = actor_time + 1;
            pending_swarms.push_back(image);
            spell_effects.prismatic_guard = 0;
            // Mark by pending actor index when it enters the scheduler.
            if (mob_effects.size() < mobs.size() + pending_swarms.size())
              mob_effects.resize(mobs.size() + pending_swarms.size());
            mob_effects[mobs.size() + pending_swarms.size() - 1].prismatic = 1;
          }
  }
  if (rose_ghost >= 0 && rose_ghost < int(mobs.size()) &&
      mobs[rose_ghost].alive) {
    auto &ghost = mobs[rose_ghost];
    ghost.maxhp = 40 + 10 * std::max(0, artifact_level(IT_ART_ROSE));
    if (!has_artifact(IT_ART_ROSE) || spell_effects.magic_immunity > 0)
      damage_mob(rose_ghost, 1);
    else if (!boss_alive() && ghost.hp < ghost.maxhp) {
      auto &state = ring1 == IT_ART_ROSE ? ring1_magic : ring2_magic;
      state.stored += ghost.maxhp / 500.f *
                      std::pow(1.175f, accessory_bonus(IT_RING_ENERGY));
      while (state.stored > 1 && ghost.hp < ghost.maxhp) {
        ++ghost.hp;
        --state.stored;
      }
      if (ghost.hp == ghost.maxhp)
        state.stored = 0;
    }
  }
  auto tick = [&](mechanics_rules::Actor &e, int index) {
    const bool hero = index < 0;
    if (!hero && (index >= int(mobs.size()) || !mobs[index].alive))
      return;
    const int cell = hero ? py * MW + px : mobs[index].y * MW + mobs[index].x;
    int &health = hero ? hp : mobs[index].hp;
    const int maximum = hero ? maxhp : mobs[index].maxhp;
    if(e.frost>0) {
      auto &cold=hero ? hero_effects : mobs[index].effects;
      cold.chill=0;
      if(e.frost<=1)thaw_actor(index);
      else --e.frost;
    }
    if (e.drowsy > 0) {
      e.drowsy = std::max(0.f, e.drowsy - 1);
      if (e.drowsy == 0 &&
          !((hero || mobs[index].effects.ally) && health >= maximum)) {
        e.magical_sleep = true;
        if (!hero)
          mobs[index].ai = Mob::Sleeping;
      }
    }
    if (e.magical_sleep && (hero || mobs[index].effects.ally)) {
      health = std::min(maximum, health + 1);
      if (health == maximum) {
        e.magical_sleep = false;
        if (!hero)
          mobs[index].ai = Mob::Wandering;
      }
    }
    if (!hero && e.cleanse > 0) {
      auto &status = mobs[index].effects;
      status.poison = status.bleeding = status.burning = status.paralysis =
          status.blind = status.chill = status.roots = status.charm = 0;
      mobs[index].sleep_t = mobs[index].terror_t = 0;
      e.vertigo = e.deferred = 0;
    }
    if (!hero && e.deferred > 0) {
      const int amount = std::max(1, int(e.deferred * .1f));
      e.deferred = std::max(0.f, e.deferred - amount);
      damage_mob(index, amount);
      if (!mobs[index].alive)
        return;
    }
    e.kinetic = std::max(0.f, e.kinetic - std::max(.1f, e.kinetic * .025f));
    if (!hero && e.power_left > 0 && !(index == linked_ally && link_left > 0) &&
        e.haste <= 0) {
      e.power_left = std::max(0.f, e.power_left - 1);
      if (e.power_left == 0) {
        if (index == powered_ally)
          powered_ally = -1;
        if (e.light_ally) {
          mobs[index].alive = false;
          return;
        }
      }
    }
    if (hero && e.arena > 0 &&
        dist(px, py, e.arena_cell % MW, e.arena_cell / MW) > 2)
      e.arena = 0;
    if (!hero && e.dread > 0 && !vis[mobs[index].y][mobs[index].x]) {
      mobs[index].alive = false;
      e.dread = 0;
      return;
    }
    if (e.anchor >= 0 && e.anchor != cell) {
      e.healing = e.partial_heal = 0;
      e.anchor = -1;
    }
    if (e.earth_anchor >= 0 && e.earth_anchor != cell) {
      e.earth = 0;
      e.earth_anchor = -1;
    }
    if (e.healing > 0) {
      const int amount = seed_rules::heal_tick(e.healing, e.partial_heal, maximum);
      health = std::min(maximum, health + amount);
    }
    if (e.artifact_recharge > 0 && hero)
      artifact_charge(1);
    const bool spirit_might =
        hero && e.spirit_left > 0 && e.spirit_ring == IT_RING_MIGHT;
    for (float *timer : {&e.vertigo,
                         &e.bless,
                         &e.haste,
                         &e.levitation,
                         &e.magic_immunity,
                         &e.foresight,
                         &e.scry,
                         &e.cripple,
                         &e.slow,
                         &e.weakness,
                         &e.vulnerable,
                         &e.artifact_recharge,
                         &e.holy_weapon,
                         &e.holy_ward,
                         &e.light_shield,
                         &e.light,
                         &e.sunray_recent,
                         &e.arena,
                         &e.aura,
                         &e.wayward,
                         &e.shuriken_cooldown,
                         &e.body_left,
                         &e.spirit_left,
                         &e.lifelink,
                         &e.cleanse,
                         &e.hallowed})
      *timer = std::max(0.f, *timer - 1);
    e.amok = std::max(0.f, e.amok - 1);
    if (spirit_might && e.spirit_left == 0)
      update_maxhp();
  };
  tick(spell_effects, -1);
  for (size_t i = 0; i < std::min(mob_effects.size(), mobs.size()); ++i)
    tick(mob_effects[i], i);
  // Awareness belongs to the hero and expires even while its actor is off-floor.
  for(auto &state:visited_floors)if(state.depth!=floor)
    for(auto &effect:state.effects)effect.scry=std::max(0.f,effect.scry-1);
  for(auto &heap:scry_heaps)heap.left=std::max(0.f,heap.left-1);
  scry_heaps.erase(std::remove_if(scry_heaps.begin(),scry_heaps.end(),
      [](const mechanics_rules::HeapAwareness &heap){return heap.left<=0;}),scry_heaps.end());
  std::vector<FloorItem> returns;
  for (size_t index = 0; index < items.size();) {
    auto &item = items[index];
    if (item_def(item.id).kind != K_MISSILE) {
      ++index;
      continue;
    }
    if (item.magic.aux > 0) {
      const int owner = item.magic.aux - 1;
      if (owner >= int(mobs.size()) || !mobs[owner].alive)
        item.magic.aux = 0;
      else {
        item.x = mobs[owner].x;
        item.y = mobs[owner].y;
      }
    } else if (item.magic.aux < 0 && item.magic.cooldown > 0 &&
               --item.magic.cooldown <= 0) {
      const int cell = -item.magic.aux - 1;
      item.x = cell % MW;
      item.y = cell / MW;
      item.magic.aux = 0;
      returns.push_back(item);
      items.erase(items.begin() + index);
      continue;
    }
    ++index;
  }
  for (auto item : returns) {
    const int victim = mob_at(item.x, item.y);
    if (victim >= 0 && !npc(mobs[victim]) && !mobs[victim].effects.ally &&
        inv_n < BAG_MAX) {
      Item projectile;
      projectile.id = item.id;
      projectile.flags = item.flags;
      projectile.level = item.level;
      projectile.magic = item.magic;
      projectile.augment = item.augment;
      projectile.identification = item.identification;
      const int index = inv_n;
      inv[inv_n++] = projectile;
      const bool old_targeting = targeting, old_artifact = artifact_targeting;
      const int old_target = target_item;
      returning_projectile = true;
      throw_projectile(index, item.x, item.y);
      returning_projectile = false;
      targeting = old_targeting;
      artifact_targeting = old_artifact;
      target_item = old_target;
    } else {
      items.push_back(item);
      if (item.x == px && item.y == py)
        pick_item(items.size() - 1);
    }
  }
}
void Game::release_stasis() {
  if (stasis_ally < 0 || stasis_ally >= int(mobs.size()))
    return;
  auto &ally = mobs[stasis_ally];
  for (int y = py - 1; y <= py + 1; ++y)
    for (int x = px - 1; x <= px + 1; ++x)
      if (walkable(x, y) && mob_at(x, y) < 0 && !(x == px && y == py)) {
        ally.x = ally.vx = x;
        ally.y = ally.vy = y;
        ally.alive = true;
        ally.next_action = actor_time + 1;
        if (ally.rose_ally)
          rose_ghost = stasis_ally;
        powered_ally = stasis_ally;
        stasis_ally = -1;
        stasis_left = 0;
        return;
      }
  stasis_left = 1;
}
