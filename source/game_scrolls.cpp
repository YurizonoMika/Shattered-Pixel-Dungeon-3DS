// Shattered v4.0.2 scroll and spellbook dispatch. GPL-3.0-or-later.
#include "game.h"
#include <algorithm>
#include <cmath>
namespace {
uint32_t draw(Game &g) {
  g.rng ^= g.rng << 13;
  g.rng ^= g.rng >> 17;
  g.rng ^= g.rng << 5;
  return g.rng;
}
bool hostile(const Mob &m) {
  return m.alive && !m.effects.ally &&
         !(m.type >= M_NPC_GHOST && m.type <= M_NPC_IMP);
}
bool npc(const Mob &m) { return m.type >= M_NPC_GHOST && m.type <= M_NPC_IMP; }
uint8_t exotic_for(uint8_t scroll) {
  switch (scroll) {
  case IT_SCROLL_IDENTIFY:
    return IT_SCROLL_DIVINATION;
  case IT_SCROLL_CURSE:
    return IT_SCROLL_ANTIMAGIC;
  case IT_SCROLL_MIRROR:
    return IT_SCROLL_PRISMATIC;
  case IT_SCROLL_RECHARGE:
    return IT_SCROLL_MYSTICAL;
  case IT_SCROLL_TELE:
    return IT_SCROLL_PASSAGE;
  case IT_SCROLL_LULLABY:
    return IT_SCROLL_SIREN;
  case IT_SCROLL_MAP:
    return IT_SCROLL_FORESIGHT;
  case IT_SCROLL_TERROR:
    return IT_SCROLL_DREAD;
  case IT_SCROLL_RETRIBUTION:
    return IT_SCROLL_PSIONIC;
  case IT_SCROLL_RAGE:
    return IT_SCROLL_ARENA;
  default:
    return IT_NONE;
  }
}
} // namespace
void Game::init_spellbook() {
  constexpr uint8_t ids[] = {IT_SCROLL_IDENTIFY,    IT_SCROLL_CURSE,
                             IT_SCROLL_MIRROR,      IT_SCROLL_RECHARGE,
                             IT_SCROLL_TELE,        IT_SCROLL_LULLABY,
                             IT_SCROLL_MAP,         IT_SCROLL_RAGE,
                             IT_SCROLL_RETRIBUTION, IT_SCROLL_TERROR};
  constexpr int base[] = {6, 4, 3, 3, 3, 2, 2, 2, 2, 2};
  int weights[10];
  std::copy_n(base, 10, weights);
  for (int n = 0; n < 10; ++n) {
    int total = 0;
    for (int weight : weights)
      total += weight;
    int value = draw(*this) % total, index = 0;
    while (value >= weights[index])
      value -= weights[index++];
    spellbook_order[n] = ids[index];
    weights[index] = 0;
  }
}
void Game::spellbook_read() {
  if (!has_artifact(IT_ART_SPELLBOOK) || hero_effects.blind > 0 ||
      spell_effects.magic_immunity > 0)
    return;
  auto &state = ring1 == IT_ART_SPELLBOOK ? ring1_magic : ring2_magic;
  if (state.aux < 1) {
    message("魔典充能不足。");
    return;
  }
  // First Generator scroll deck, excluding Transmutation. The ID/removal/
  // mapping rejection follows UnstableSpellbook.doReadEffect.
  constexpr uint8_t scrolls[] = {
      IT_SCROLL_IDENTIFY,    IT_SCROLL_IDENTIFY, IT_SCROLL_IDENTIFY,
      IT_SCROLL_CURSE,       IT_SCROLL_CURSE,    IT_SCROLL_MIRROR,
      IT_SCROLL_RECHARGE,    IT_SCROLL_RECHARGE, IT_SCROLL_TELE,
      IT_SCROLL_LULLABY,     IT_SCROLL_MAP,      IT_SCROLL_RAGE,
      IT_SCROLL_RETRIBUTION, IT_SCROLL_TERROR};
  do {
    book_scroll = scrolls[draw(*this) % 14];
  } while ((book_scroll == IT_SCROLL_IDENTIFY ||
            book_scroll == IT_SCROLL_CURSE || book_scroll == IT_SCROLL_MAP) &&
           draw(*this) % 2 == 0);
  --state.aux;
  bool learned = true;
  const int level = ring1 == IT_ART_SPELLBOOK ? ring1_level : ring2_level;
  for (int i = level; i < 10; ++i)
    if (spellbook_order[i] == book_scroll)
      learned = false;
  if (state.aux > 0 && learned)
    open_dialog(30, "无序魔典", "消耗额外1点充能，可选择释放异化卷轴。",
                item_name(book_scroll), item_name(exotic_for(book_scroll)));
  else
    spellbook_resolve(false);
}
void Game::spellbook_resolve(bool exotic) {
  if (book_scroll == IT_NONE || !has_artifact(IT_ART_SPELLBOOK))
    return;
  auto &state = ring1 == IT_ART_SPELLBOOK ? ring1_magic : ring2_magic;
  const uint8_t id = exotic ? exotic_for(book_scroll) : book_scroll;
  if (exotic) {
    if (state.aux < 1 || id == IT_NONE)
      return;
    --state.aux;
  }
  book_scroll = IT_NONE;
  if (id == IT_SCROLL_IDENTIFY || id == IT_SCROLL_CURSE) {
    book_scroll = id;
    book_empowered = true;
    selection_scroll = id;
    transmute_open = true;
    transmute_sel = transmute_page = 0;
    return;
  }
  if (id == IT_SCROLL_SIREN) {
    magic_target = -1;
    book_scroll = id;
    book_empowered = true;
    artifact_targeting = targeting = true;
    artifact_slot = ring1 == IT_ART_SPELLBOOK ? -3 : -4;
    backpack = false;
    return;
  }
  scroll_effect(id);
  dispel_action();
  spend_action(1);
}
bool Game::scroll_effect(uint8_t id, int target) {
  auto spawn_ally = [&](int health, bool prismatic) {
    for (int y = py - 1; y <= py + 1; ++y)
      for (int x = px - 1; x <= px + 1; ++x) {
        if (!walkable(x, y) || mob_at(x, y) >= 0 || (x == px && y == py))
          continue;
        Mob image;
        image.x = image.vx = x;
        image.y = image.vy = y;
        image.type = M_WRAITH;
        image.hp = image.maxhp = health;
        image.effects.ally = true;
        image.atk = std::max(1, level);
        image.next_action = actor_time + 1;
        const int index = mobs.size();
        mobs.push_back(image);
        if (prismatic)
          actor_effects(index).prismatic = 1;
        else
          actor_effects(index).mirror = 1;
        return true;
      }
    return false;
  };
  switch (id) {
  case IT_SCROLL_MAP:
    for (int y = 0; y < map_height; ++y)
      for (int x = 0; x < map_width; ++x) {
        seen[y][x] |= 2;
        if (tiles[y][x] == T_SECRET)
          tiles[y][x] = T_DOOR;
      }
    for (auto &f : features)
      if (f.type == F_TRAP)
        f.type = F_TRAP_FOUND;
    break;
  case IT_SCROLL_FORESIGHT:
    spell_effects.foresight = 400;
    break;
  case IT_SCROLL_DIVINATION: {
    int weights[3] = {3, 3, 3};
    for (int n = 0; n < 4; ++n) {
      std::vector<uint8_t> unknown[3];
      for (int i = 1; i < IT_COUNT; ++i)
        if (item_random_identity(i) && !identity_known(i)) {
          const int category = item_def(i).kind == K_RING      ? 2
                               : item_def(i).cat == CAT_POTION ? 0
                                                               : 1;
          unknown[category].push_back(i);
        }
      int total = 0;
      for (int i = 0; i < 3; ++i) {
        if (unknown[i].empty())
          weights[i] = 0;
        total += weights[i];
      }
      if (!total) {
        for (int i = 0; i < 3; ++i)
          weights[i] = unknown[i].empty() ? 0 : 3;
        for (int weight : weights)
          total += weight;
      }
      if (!total)
        break;
      int category = 0, value = draw(*this) % total;
      while (value >= weights[category])
        value -= weights[category++];
      const auto &list = unknown[category];
      identify_type(list[draw(*this) % list.size()]);
      --weights[category];
    }
    break;
  }
  case IT_SCROLL_RECHARGE:
    spell_effects.wand_recharge = 30;
    break;
  case IT_SCROLL_MYSTICAL:
    spell_effects.artifact_recharge = 30;
    break;
  case IT_SCROLL_ANTIMAGIC:
    spell_effects.magic_immunity = 20;
    break;
  case IT_SCROLL_TERROR:
  case IT_SCROLL_DREAD:
    for (size_t i = 0; i < mobs.size(); ++i)
      if (hostile(mobs[i]) && vis[mobs[i].y][mobs[i].x]) {
        mobs[i].terror_t = 20;
        if (id == IT_SCROLL_DREAD && !mob_def(mobs[i].type).boss)
          actor_effects(i).dread = 1;
      }
    break;
  case IT_SCROLL_LULLABY:
    spell_effects.drowsy = 5;
    for (size_t i = 0; i < mobs.size(); ++i)
      if (mobs[i].alive && !npc(mobs[i]) && vis[mobs[i].y][mobs[i].x])
        actor_effects(i).drowsy = 5;
    break;
  case IT_SCROLL_METAMORPHOSIS:
    message("蜕变依赖其他职业的完整天赋树，当前无法使用。");
    return false;
  case IT_SCROLL_SIREN: {
    if (target < 0 || !in_map(target % MW, target / MW))
      return false;
    int selected = mob_at(target % MW, target / MW);
    if (selected < 0 || !hostile(mobs[selected]) ||
        !vis[mobs[selected].y][mobs[selected].x])
      return false;
    for (auto &m : mobs)
      if (hostile(m) && vis[m.y][m.x])
        m.effects.charm = 20;
    if (!mob_def(mobs[selected].type).boss) {
      mobs[selected].effects.ally = true;
      mobs[selected].effects.charm = 0;
    }
    break;
  }
  case IT_SCROLL_RETRIBUTION:
  case IT_SCROLL_PSIONIC: {
    std::vector<int> targets;
    for (size_t i = 0; i < mobs.size(); ++i)
      if (hostile(mobs[i]) && vis[mobs[i].y][mobs[i].x])
        targets.push_back(i);
    for (int i : targets) {
      const float power = std::min(4.f, 4.45f * float(maxhp - hp) / maxhp);
      int damage = id == IT_SCROLL_PSIONIC
                       ? int(std::lround((mobs[i].maxhp + mobs[i].hp) / 2.f))
                       : int(std::lround(mobs[i].maxhp / 10.f +
                                         mobs[i].hp * power * .225f));
      damage_mob(i, damage);
      if (mobs[i].alive)
        mobs[i].effects.blind = 10;
    }
    if (id == IT_SCROLL_PSIONIC) {
      take_damage(
          int(std::lround(maxhp * .5f * std::pow(.9f, targets.size()))));
      if (!dead) {
        hero_effects.blind = 10;
        debuffs[status_rules::Weakness] = 100;
      }
    } else {
      hero_effects.blind = 10;
      debuffs[status_rules::Weakness] = 20;
    }
    break;
  }
  case IT_SCROLL_RAGE:
    for (size_t i = 0; i < mobs.size(); ++i)
      if (mobs[i].alive && !npc(mobs[i]) && !mobs[i].effects.ally) {
        mobs[i].target = py * MW + px;
        mobs[i].ai = Mob::Hunting;
        if (vis[mobs[i].y][mobs[i].x])
          actor_effects(i).amok = 5;
      }
    break;
  case IT_SCROLL_ARENA:
    spell_effects.arena = 100;
    spell_effects.arena_cell = py * MW + px;
    break;
  case IT_SCROLL_MIRROR:
    spawn_ally(1, false);
    spawn_ally(1, false);
    spawn_ally(1, false);
    break;
  case IT_SCROLL_PRISMATIC:
    spell_effects.prismatic_guard = 10 + int(std::floor(level * 2.5f));
    break;
  case IT_SCROLL_PASSAGE: {
    if (boss_alive() || floor <= 1)
      return false;
    const int depth = std::max(1, floor - 1 - (floor - 2) % 5);
    const auto found =
        std::find_if(visited_floors.begin(), visited_floors.end(),
                     [&](const FloorState &f) { return f.depth == depth; });
    if (found == visited_floors.end())
      return false;
    carry_allies(true);
    remember_floor();
    restore_floor(depth, false);
    carry_allies(false);
    break;
  }
  case IT_SCROLL_TELE:
    if (boss_alive())
      return false;
    for (int n = 0; n < 500; ++n) {
      int x = 1 + draw(*this) % (map_width - 2),
          y = 1 + draw(*this) % (map_height - 2);
      if (walkable(x, y) && mob_at(x, y) < 0) {
        px = pvx = x;
        py = pvy = y;
        clear_path();
        break;
      }
    }
    break;
  default:
    return false;
  }
  recompute_fov();
  return true;
}
