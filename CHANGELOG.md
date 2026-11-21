# Changelog

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
