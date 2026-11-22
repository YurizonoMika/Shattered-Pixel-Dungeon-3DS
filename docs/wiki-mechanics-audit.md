# Wiki mechanics audit / Wiki 机制复盘

Reference checked 2026-11-22: <https://pixeldungeon.wiki/>. The main namespace
documents **Shattered Pixel Dungeon**; `OriginalPD:` documents the original game.
This project targets Shattered v4.0.2, upstream commit
`57a4e06a4caf162446d1c28caa7983f0493fecf0`.

以主命名空间的《破碎的像素地牢》为目标，不混入 `OriginalPD:` 规则。
Wiki 可能混有版本差异和笔误：例如 Dungeon 页仍写固定 32×32 地图，
Enemies 表的部分数值与 v4.0.2 不一致，不能机械地照抄。发生冲突时记录冲突，
用固定 v4.0.2 源码复核；不把“跑完 25 层”标成机制一致。

## Evidence / 依据

These are independent summaries of rules, not copies of Wiki article text.
Wiki article text is CC BY-SA 4.0; see each linked page's attribution/history.

| Page | Revision | Rules used / 核对内容 |
| --- | --- | --- |
| [Hero](https://pixeldungeon.wiki/Hero?oldid=3199) | 3199 | 20 starting HP; +5 HP per level; next XP `5+5×level`; level cap 30; classes/talents |
| [Food](https://pixeldungeon.wiki/Food?oldid=3427) | 3427 | Ration 300, cooked meat 150, pasty 450 satiety; 3 turns to eat |
| [Equipment](https://pixeldungeon.wiki/Equipment?oldid=1681) | 1681 | Tiers, strength requirement and triangular upgrade thresholds |
| [Armor](https://pixeldungeon.wiki/Armor?oldid=3433) | 3433 | DR bounds, strength penalties, identification, glyphs |
| [Melee Weapons](https://pixeldungeon.wiki/Melee_Weapons?oldid=3488) | 3488 | Damage, accuracy, reach, delay, requirements, curses and identification |
| [Potion of Strength](https://pixeldungeon.wiki/Potion_of_Strength?oldid=1224) | 1224 | Permanent +1 STR; two per region |
| [Potion of Experience](https://pixeldungeon.wiki/Potion_of_Experience?oldid=1268) | 1268 | One level; Bless at cap |
| [Potion of Healing](https://pixeldungeon.wiki/Potion_of_Healing?oldid=3099) | 3099 | Healing over time: `0.8×HT+14`; cleanses debuffs |
| [Trap](https://pixeldungeon.wiki/Trap?oldid=3577) | 3577 | Generation, discovery, avoidance, activation, full list |
| [Dungeon](https://pixeldungeon.wiki/Dungeon?oldid=2846) | 2846 | Regions, 26 floors; fixed-size claim conflicts with pinned code |
| [Enemies](https://pixeldungeon.wiki/Enemies?oldid=2992) | 2992 | Stats, behavior and properties; some entries require code verification |
| [Rings](https://pixeldungeon.wiki/Rings?oldid=3490) | 3490 | 12 rings, identities, modifiers and generation |
| [Wands](https://pixeldungeon.wiki/Wands?oldid=3533) | 3533 | Zaps, recharging, curses and generation |
| [Scrolls](https://pixeldungeon.wiki/Scrolls?oldid=3496) | 3496 | 12 basic and 12 exotic types; category weights |
| [Alchemy](https://pixeldungeon.wiki/Alchemy?oldid=3657) | 3657 | Energy, recipes, three-seed brewing |
| [Shop](https://pixeldungeon.wiki/Shop?oldid=3129) | 3129 | Floors, stock, prices and shopkeeper |
| [Special Room](https://pixeldungeon.wiki/Special_Room?oldid=2980) | 2980 | Room mechanics and rewards |
| [Status Effects](https://pixeldungeon.wiki/Status_Effects?oldid=3771) | 3771 | Buffs, debuffs, resistance and sources |

## Audit / 差异清单

“Partial” means the stated subsystem remains incomplete even if individual
formulas pass tests. No whole-game or whole-floor parity is claimed.

| System / 系统 | Before this audit / 旧实现 | Current work and remaining gaps / 本轮及后续 |
| --- | --- | --- |
| Hero progression | Different class HP; next XP `level×10`; uncapped; generic automatic talent buffs | Base HP and XP corrected; class starts, unlocks, talent choices, subclasses and armor abilities still incomplete |
| Strength | Absent; potion adds HP | Persistent STR, +1 potion, equipment requirements; Ring of Might HT multiplier, mastery and cursed levels still missing |
| Hunger and food | Food heals directly; unconditional regen every 12 turns | Persistent hunger, satiety, three-turn food, starvation and 10-turn regen; food talents/holiday effects and full hunger modifiers remain |
| Healing potion | Instant +15 HP | Healing pool and per-turn recovery; complete cleansing, challenge and trinket effects remain |
| Physical combat | Fixed weapon power and ad hoc armor upper bound | Base weapon ranges, reach, armor bounds/encumbrance and cumulative action costs corrected; full per-actor scheduler, procs, augmentation, curses and enchantments remain |
| Item catalog | Around 90 entries, including native substitutes | 12 basic/exotic potions and scrolls, complete missiles/seeds/stones, artifacts, trinkets and unique hero items remain missing |
| Identification/curse | Identify/Remove Curse only print success | Need stored identity, known state, curse state, equip restrictions and item selection; do not claim implemented |
| Upgrade/transmutation | Automatically chooses equipped weapon; category reroll | Need selected-target upgrades and complete category decks, equip state, charges and durability |
| RNG/loot | Native run plan, category decks; wrong weapon/armor tier mappings; artifacts become rings | Supported equipment now maps to actual tiers; complete per-category decks, artifact uniqueness, depth seed schedule and exact guaranteed drops remain |
| Normal map | Builders and several masks matched on identical explicit inputs | Complete painters, room constraints, merging, door hiding, terrain vetoes and full-floor fixtures remain |
| Population | Area-based 1–4 cap added to address crowded rooms | Removed this non-upstream cap; restored size-category weights, 25% second-spawn attempt and bounded room sampling. Full eligibility, original AI/scheduler and same-seed population fixtures remain |
| Traps | Six custom types and region weights; radius/turn gas | Need original trap tables, every effect, original gas diffusion, item activation and glyphs. Full-floor alarm and visible-trap avoidance are corrected in this pass |
| Monster combat | Base HP and hit/DR partly present; generic attack strength | Basic melee ranges, kill-XP caps, and normal boss HP/XP now explicit; cooldowns, AI, immunities, statuses, variants, death procs and boss phases remain. Summon-only necromancer and Yog still have native placeholder attacks |
| Terrain/FOV | Native LOS and simplified grass; chasms block walking | Original shadow casting, high grass trampling, fire/water/gas, flying and fall/return transitions remain |
| Shops/quests | UI and simplified quest actions | Exact stock/value/identification rules, quest variants, Old Mine and Imp Vault remain |
| Alchemy | Three fixed seed-to-potion recipes | Energy, seed mixtures, catalysts, bombs, spells, elixirs and exotic recipes remain |
| Run completion | Ends after floor-25 boss | Floor 26, Amulet, ascent, persistent visited floors and ascent modifiers remain |
| Meta/UI/audio | Native 3DS UI and ranking, no gameplay audio | Badges, challenges, daily/seeded runs, journal, localization and audio remain |
| Saves | V9/V10; bounded entity tables | V11 adds survival, action remainder and room weights; per-floor state, future buff/item metadata and table overflow handling remain |

V9/V10 migration retains existing HP and enemies. Missing strength defaults to
10, hunger to 0, and the XP threshold becomes `5+5×level`. The old files cannot
reveal how many strength potions were consumed. A fresh run is recommended for
mechanics comparisons; keep a backup before upgrading. V11 cannot be loaded by
older releases.

旧档保留已有生命和怪物，缺失力量以 10 初始化、饥饿以 0 初始化，升级门槛
采用新公式。无法从旧档准确还原已消耗的力量药水，因此建议新局验证对齐。
V11 不支持降级读取。

The native `IT_HALBERD` entry currently uses the upstream Glaive's base
statistics. `IT_GIANT_HAMMER` and `IT_RUNE_ARMOR` are legacy substitutes;
neither is used to fill a hole in the corrected tier drop tables. Tier tables
still cover only the supported subset, with repeated entries where fewer than
three weapons exist. They are not the original category probabilities.

## Verified in v0.1.11alpha / 本次验证

- Host rule boundaries: XP curve, food values, 155 armor/strength cases,
  weapon ranges, tiers, reach and action delays.
- Pinned Java comparisons: 1,000 builders, 500 patch masks, 100 mazes,
  2,000 room input lists, 1,800 room paintings and 30,000 pathfinder results.
  These use shared explicit inputs, not whole-floor same-seed fixtures.
- Azahar scene 87: 800 floors, zero generation/supply/route failures,
  zero invalid room weights; 429 chasm floors and no ordinary-floor fallback.
- Scenes 99/91/92: combat, death, shields/Ankh, respawn retry/rotation,
  locks, trap state and V11 save round-trip passed.
- Scene 104: strength, three-turn food, hunger boundary, shield-bypassing
  starvation, fractional starvation, regeneration, XP cap, healing (including
  full-health use), action accumulation, V11 round-trip, V10 defaults and
  truncated-trailer rejection passed. The migration fixture changes the
  version of a disposable V11 file; it is not a corpus of historical saves.
- Scene 105: visible-trap avoidance, whole-floor alarm and mapping passed.
- Both release formats built. CIA installed and booted in Azahar; title version
  visually checked and all SMDH titles/descriptions checked as version-free.
  Existing normal save checksums were unchanged and fixture slots removed.

验证范围不包含真机测试，也不代表整层同种子或全游戏机制一致。

## Implementation order / 实施顺序

1. Hero base stats, survival and compatible save state. Test exact boundaries,
   food consumption, strength requirements, healing and migration.
2. Actor timing and complete item state. Implement movement/attack delays before
   claiming haste, encumbrance, fast weapons or poison timing match.
3. Original room inputs/painters and monster/loot placement. Replace ad hoc caps
   only with verified behavior; retain generation reachability regressions.
4. Full trap catalog and blob/status engines; original monster abilities.
5. Complete item catalog, recipes, class talents, quests and boss scripts.
6. Floor returns, Amulet/ascent, challenges, badges, audio and end-to-end fixtures.

测试结束关闭测试模拟器，只保留一个标题菜单的试玩实例。
