# Changelog

## Unreleased

Generation rework toward SPD's actual mechanics:

- exact room budgets per region, shop floors 6/11/16 and the Imp shop on 20
- SPD special-room rotation (treasury, armory, crypt, pool, well, garden,
  library, storage, lab, statue, sentry, trap/toxic/fire, runestone, vault)
  with single entrances, locked doors and iron keys
- level feelings (water/grass/large/traps/secrets/dark) on depths 2+
- exact depth mob rotation, mob cap, 50-turn respawns, rare alts and 2.5% rares
- 10 new enemies (snake, swarm, slime, DM-100, necromancer, DM-200, ghoul,
  elemental, scorpio, mimic) with SPD stats
- loot decks, 3/4/5 item counts, skeleton/chest/mimic/locked-chest heaps,
  mob meat/gold drops and no guaranteed food
- raised-wall internal tops and wall/floor overhang shading
- quest floors/rooms corrected (Wandmaker 7–9, Blacksmith 12–14)
- debug scene 74 (generation audit), 75/76 (enemy galleries)


## 0.1.0alpha — 2026-11-21

First public build.

- Native C++17 port for Nintendo 3DS (devkitARM + libctru + citro2d)
- 25 floors across five regions, five bosses, Yog-Dzewa finale
- Six heroes, title/menu/settings/rankings/save-slot UI
- Turn-based movement, combat, FOV, stairs, traps, secret doors
- 90 items: weapons/armor tiers, 13 wands, 11 potions, 11 scrolls, 8 rings,
  food, bombs, missiles, seeds, quest items, bags
- Shops on floors 6/11/16/20, buy/sell/buyback tabs, shopkeeper warn/flee
- Ghost / Wandmaker / Blacksmith / Imp quests with rolled quest floors
- Alchemy pot, Scroll of Transmutation picker, Ankh revival, talents
- Binary save format v9 shared by 3DSX and CIA builds
- Item sprites audited against SPD's `ItemSpriteSheet.java`
- Map uses SPD raised walls, stitched water, raised doors and chest/tomb sprites
- CIA build with custom banner, icon and title metadata

Known gaps: no music/SFX yet, talent tree is simplified (4 fixed tiers),
wall internal-top/overhang layers are approximated, and the Old Mine is a
sealed area on the same floor rather than a separate sub-level.
