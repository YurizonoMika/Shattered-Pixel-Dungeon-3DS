# Wiki mechanics audit / Wiki 机制复盘

Reference checked 2026-11-22: <https://pixeldungeon.wiki/>. The main namespace
documents **Shattered Pixel Dungeon**; `OriginalPD:` documents the original game.
This project targets Shattered v4.0.2, upstream commit
`57a4e06a4caf162446d1c28caa7983f0493fecf0`.

以主命名空间的《破碎的像素地牢》为目标，不混入 `OriginalPD:` 规则。
Wiki 可能混有版本差异和笔误：例如 Dungeon 页仍写固定 32×32 地图，
Enemies 表的部分数值与 v4.0.2 不一致，不能机械地照抄。发生冲突时记录冲突，
用固定 v4.0.2 源码复核；不把“跑完 25 层”标成机制一致。

Current scope: align in-run features and mechanics. Meta systems such as badges,
daily runs and persistent achievement progression are excluded by user request.
后续对齐范围为局内功能与机制；局外成就、每日挑战等不作为对齐目标。

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
| Strength | Absent; potion adds HP | Persistent STR, +1 potion, equipment requirements, Might HT multiplier and cursed ring penalties; mastery and negative item levels remain |
| Hunger and food | Food heals directly; unconditional regen every 12 turns | Persistent hunger, satiety, three-turn food, starvation and 10-turn regen; food talents/holiday effects and full hunger modifiers remain |
| Healing potion | Instant +15 HP | Healing pool and per-turn recovery; complete cleansing, challenge and trinket effects remain |
| Physical combat | Fixed weapon power and ad hoc armor upper bound | Individual actor clocks, base weapon ranges, reach, DR, encumbrance and augmentation. The native enchantment and glyph engine shares the buff clock. Individual effect timing and interactions still need comparisons. |
| Item catalog | Around 90 entries, including native substitutes | 31 normally generated melee weapons, 12 rings, 13 artifacts, 12 seeds and 15 ordinary thrown weapons. Missile hit durability, pinning and group recovery are implemented. Tipped darts, more consumables, stones and trinkets remain incomplete. See the v0.1.15alpha development notes for effect coverage. |
| Foresight artifact | Square scan around the selected target, rounded whole-charge cost | Pinned cone rays, distance clamp, fractional charge/debt, discovery XP, timed enemy/heap awareness and visible-trap warning transitions. 12,072 Java cases and host runtime tests pass. Native heap identity, shared buff timing and other-class talent hooks remain partial. |
| Identification/curse | Identify/Remove Curse only print success | Stored known/curse state, equip locks, selected scroll targets and default use/XP identification. Per-run potion/scroll/ring appearances and global type knowledge now stored; full appearance catalogs, complete curse effects, class/trinket ID modifiers and negative levels remain |
| Upgrade/transmutation | Automatically chooses equipped weapon; category reroll | Selected upgrades/removal/identification, augmentation picker, wand charge preservation and no armor transmutation. Projectile durability and group identity persist. Full category decks, item metadata, Metamorphosis and several exotic effects remain incomplete. |
| RNG/loot | Native run plan, category decks, incomplete equipment tier mappings | Equipment tier tables and run-wide artifact uniqueness are implemented. Per-category decks, depth seed schedules and exact guaranteed drops remain outstanding. |
| Normal map | Builders and several masks matched on identical explicit inputs | Complete painters, room constraints, merging, door hiding, terrain vetoes and full-floor fixtures remain |
| Population | Area-based 1–4 cap added to address crowded rooms | Removed this non-upstream cap; restored size-category weights, 25% second-spawn attempt and bounded room sampling. Full eligibility, original AI/scheduler and same-seed population fixtures remain |
| Traps | Six custom types and region weights; radius/turn gas | ToxicGas/Blob diffusion and depth damage now ported; alarm/avoidance corrected. Full trap tables/effects, fire/freezing/electricity, item activation and glyphs remain |
| Monster combat | Base HP and hit/DR partly present; generic attack strength | Added base movement/attack delays; magic bolts/debuffs; Scorpio retreat; Eye charge; necromancer summon/heal; guard chains; monk focus; brute rage; bat healing; skeleton explosion; Swarm split/generations; slime reduction; Goo pumping/water heal. Variant scripts, full AI/immunities/loot, ghoul revival, webs, leaps and later boss phases remain |
| Terrain/FOV | Native LOS and simplified grass; chasms block walking | ShadowCaster and Ballistica ported with Java fixtures; mind vision corrected. Base high grass/dew/waterskin implemented; Huntress grass, artifact/trinket modifiers, fire/water/status interactions, falling, special sight modifiers and complete terrain flags remain |
| Shops/quests | UI and simplified quest actions | Exact stock/value/identification rules, quest variants, Old Mine and Imp Vault remain |
| Alchemy | Three fixed seed-to-potion recipes | Energy, seed mixtures, catalysts, bombs, spells, elixirs and exotic recipes remain |
| Run completion | Ends after floor-25 boss | Persistent visited floors, stairs up, LastLevel walkable geometry, Amulet choice/surface victory and base ascent stacks/modifiers implemented. Exact transition timing, reset exceptions, original panoramas and full ascent interactions remain |
| Meta/UI/audio | Native 3DS UI and ranking, no gameplay audio | Meta progression, badges and daily runs are outside the requested parity scope. In-run challenge modifiers, journal information, localization and audio remain incomplete. |
| Saves | V9/V10 and bounded entity tables | V15 retains V14 magic/entity prefixes and appends plant/spell actor state, ghost gear, Cleric talents, Trinity imprints and spellbook order. V9–V14 import paths remain; independent historical runtime fixtures cover V12/V13. Atomic replacement preserves the previous file on failure. |

V9/V10 migration retains existing HP and enemies. Missing strength defaults to
10, hunger to 0, and the XP threshold becomes `5+5×level`. The old files cannot
reveal how many strength potions were consumed. A fresh run is recommended for
mechanics comparisons; keep a backup before upgrading. V12 cannot be loaded by
older releases.

旧档保留已有生命和怪物，缺失力量以 10 初始化、饥饿以 0 初始化，升级门槛
采用新公式。无法从旧档准确还原已消耗的力量药水，因此建议新局验证对齐。
V12 不支持降级读取。

The native `IT_HALBERD` entry uses upstream Glaive base statistics.
`IT_GIANT_HAMMER` and `IT_RUNE_ARMOR` are legacy substitutes excluded from
the corrected tier tables. All 31 normally generated melee weapons are now
represented, but category depletion decks and exact RNG consumption still differ.

## Current verification / 本轮验证

Frost follow-up: Icecap applies independent saved Frost, damage detaches it and
water thaw leaves Chill. 10,001 speed-factor cases match pinned Java. Host
tests cover paralysis coexistence, zero-damage thaw, expiry, movement blocking
and saving. Inventory freezing and full immunity/resistance tables remain open.

Plant follow-up: 26,650 Earthroot absorption cases, 18,000 Sungrass healing ticks,
nine cure entries and three buff durations match pinned Java. Runtime tests cover
independent anchors, shared potion/Mageroyal cleansing, movement-only Blindweed
Cripple, duration preservation and V15 saving. Warden, immunities,
full Frost and elemental terrain/item behavior remain incomplete. Swiftthistle
now has independent TimeBubble consumption, queued presses/save state and
transition disarming; 93,600 states match Java reset/processTime. Independent
VFX scheduling and complete offensive dispel coverage remain partial. Azahar
scene 122 passes bubble boundary, queue deduplication, saving/loading and expiry.
Hourglass freeze now debits charges incrementally and retains unused charges
when cancelled. 44,000 charge/timing/expiry states match pinned Java; host
tests cover pending presses, fractional save state, bubble priority and removal.
Azahar scene 123 passes charge, save/load and cancellation checks.
Stasis compensates non-starving hunger and advances existing healing and status
clocks while protecting the hero. 4,510 charge/duration/hunger cases match Java;
independent buff ordering and complete artifact/talent hooks remain partial.

For the unpublished v0.1.15alpha tree, the runtime ASan/UBSan suite and
30,690 pinned Java projectile formula cases and 12,072 Foresight cone/charge
cases pass. Scene 120 passes mapping, awareness, charge, save/load and expiry
on the latest rebuilt ARM binary. Azahar scenes 117/118/119
cover cross-floor ghost/link/save state, Trinity selection and Cleric talent
menus. Scenes 115/116/109/106/108/105 and scene 87 were rerun on the rebuilt
development binary: 800 floors, zero generation/rule/room-weight failures,
341 Loop, 299 FigureEight and zero ordinary-floor fallbacks. The real slot0
SHA256 remains `783b26d7473c32b4515f056532095d34531a12f3ed44846f7c8d1dccf38af3c9`.
Disposable fixture slots were removed. This does not establish complete
artifact, spell, scroll or terrain parity; see the
[development notes](releases/v0.1.15alpha.md).

- `bash tools/check-game-rules.sh`: actual runtime built on host with only
  platform stubs, AddressSanitizer and UndefinedBehaviorSanitizer. Covers actor
  ties/fractions, save continuation, item locks/scroll targets/augmentation/ID,
  ranged/debuff/ability behavior, gas, floor returns, >48 items, Amulet/ascent,
  wand recharge, V11 migration and failed-save retention. Saves live only in a
  temporary test directory.
- `check-equipment.sh`: 961 cases across 31 weapons, upgrades +0..30,
  tier/damage/STR/accuracy/delay/reach match extracted pinned Java methods;
  465 armor cases match DR min/max and evasion across all augmentation choices.
- `check-ballistica.sh`: 40,000 full paths/collision points/distances match.
- `check-shadowcaster.sh`: 10,000 masks at radii 1..20 match.
- `check-blob.sh`: 17,500 diffusion steps/volumes match the extracted original
  evolve method, including changing area bounds during scanning.
- Azahar scenes 109/106/108/104/105/99/91/92/102/103 passed floor returns,
  LastLevel/save, actor ties, curse selection, Eye charge, survival/migration,
  traps, combat and respawn. Scene 87 passed 800 floors with zero generation,
  supply/route or room-weight failures; 429 contained chasms, 341 regular floors
  used Loop and 299 FigureEight, with zero ordinary-floor fallbacks.
- Existing builder/room/painter/pathfinder comparisons were rerun successfully.
- Both release formats build for the development tree. The historical
  v0.1.12alpha CIA install/title check is separate from the current V15 3DSX
  scene tests. All 16 SMDH language entries have version-free titles/descriptions.
  The real slot hash is unchanged; disposable slots removed.

V12 introduced the original header and V10/V11 trailers for migration, then appended
versioned actor, item, status, gas and visited-floor data. Old saves import as
known, uncursed equipment; missing prior floors cannot be reconstructed. V12
cannot be loaded by older releases. The extended floor archive preserves native
value-struct layouts for the ARM build; portable cross-platform save interchange
is not claimed. No hardware testing or whole-game parity is claimed.

V12 起保存已访问楼层、完整实体、行动时间、装备状态、鉴定进度和毒气浓度。
V15 在原有布局后扩展种子/法术状态、牧师、幽灵装备和先见感知等状态。
旧版缺失的历史楼层不能还原；不支持降级。

## Verified in v0.1.11alpha / 前版验证

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
6. Floor returns, Amulet/ascent, in-run challenge modifiers, audio and end-to-end fixtures.
   Badges, daily runs and other meta progression are outside the requested scope.

测试结束关闭测试模拟器，只保留一个标题菜单的试玩实例。

## v0.1.14alpha requested fixes / 历史八项修正

Wiki requests for Swarm/grass pages returned HTTP 403 on 2026-11-23; formulas
were verified against pinned v4.0.2 `Swarm.defenseProc`, `HighGrass.trample`,
`Dewdrop.consumeDew`, `Waterskin.execute`, `Ghost.interact` and `SentryRoom.paint`.
The native sentry-room Guard was an incorrect substitute and has been removed;
the original stationary sentry remains outstanding.

- Physical Swarm splits share remaining HP, delay clone turns and grant no clone
  XP. Generation and limited healing-potion counts are persistent.
- High grass blocks sight, tramples to grass and uses base 1/25 seed, 1/6 dew
  chances (half dew on grassy floors). V15 adds all twelve seed entries;
  Huntress furrowed grass, full elemental effects and trinket modifiers remain outstanding.
- Waterskin holds 20 drops; each heals 5% HT. Pickup retains dew if full-health
  and unable to store it. Native water uses the existing persistent charges field.
- Equipped inspection, blue-cursor tile inspection, conservative next-step enemy
  interruption and visible-exclusion quest spawns are implemented. Travel uses
  current/next FOV and action windows, not a speculative full-game simulation.
- Potions/scrolls/rings have per-run appearance permutations and saved global
  type knowledge. Equipment upgrade/curse fields stay hidden until known. This
  did not include artifacts/enchantments, which are added in V15; every
  identification source and the original complete appearance catalogs remain incomplete.
- V13 appends DID1 knowledge/drop counts after the unchanged V12 archive; V12
  imports retain old type knowledge. V9–V11 migration still applies. Old versions
  cannot read V13. ASan/UBSan requested-mechanics tests and the existing runtime
  tests pass, including V12 import. 48 early floors contain no injected Guards.
