#!/usr/bin/env bash
# Copy the assets we need from the upstream Shattered Pixel Dungeon repo and
# convert them into textures the 3DS build can consume.
#
#   UPSTREAM=... bash tools/prepare-assets.sh
#
# Upstream: https://github.com/00-Evan/shattered-pixel-dungeon (GPLv3)

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UPSTREAM="${UPSTREAM:-$ROOT/../spd-upstream}"
ASSETS="$UPSTREAM/core/src/main/assets"

if [ ! -d "$ASSETS" ]; then
  echo "找不到上游素材：$ASSETS"
  echo "先执行：git clone --depth 1 https://github.com/00-Evan/shattered-pixel-dungeon.git $ROOT/../spd-upstream"
  exit 1
fi

cd "$ROOT"
mkdir -p gfx romfs/gfx

copy() { cp "$ASSETS/$1" "gfx/$2"; }

copy interfaces/banners.png      banners.png
copy interfaces/chrome.png       chrome.png
copy interfaces/toolbar.png      toolbar.png
copy interfaces/status_pane.png  status_pane.png
copy interfaces/icons.png        icons.png
copy interfaces/hero_icons.png   hero_icons.png
copy sprites/avatars.png         avatars.png
copy sprites/items.png           items.png
copy sprites/item_icons.png      item_icons.png
copy environment/tiles_sewers.png tiles_sewers.png
copy environment/tiles_prison.png tiles_prison.png
copy environment/tiles_caves.png  tiles_caves.png
copy environment/tiles_city.png   tiles_city.png
copy environment/tiles_halls.png  tiles_halls.png

for h in warrior mage rogue huntress duelist cleric; do
  copy "sprites/$h.png" "hero_$h.png"
  # Hero splash art for the top screen (800x450 -> 400 wide).
  ffmpeg -loglevel error -y -i "$ASSETS/splashes/$h.jpg" \
    -vf "scale=400:-1:flags=neighbor" "gfx/art_$h.png"
done

for m in rat gnoll crab skeleton thief guard bat brute shaman spinner golem monk \
         warlock succubus eye ripper wraith goo tengu dm300 king yog \
         ghost shopkeeper blacksmith imp wandmaker; do
  copy "sprites/$m.png" "mob_$m.png"
done

copy splashes/title/archs.png         title_archs.png
copy splashes/title/back_clusters.png title_back.png
copy splashes/title/front_small.png   title_front.png

# One .t3s per PNG: each becomes its own t3x texture. Keeping sheets separate
# avoids the 1024x1024 atlas limit and lets us address grid cells at runtime.
for png in gfx/*.png; do
  t3s="${png%.png}.t3s"
  printf -- '-f rgba8888\n%s\n' "$(basename "$png")" > "$t3s"
done

echo "素材已准备：$(ls gfx/*.png | wc -l | tr -d ' ') 张 PNG"
