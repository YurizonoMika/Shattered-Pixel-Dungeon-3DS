#!/usr/bin/env bash
# Fetch the OFL-licensed Fusion Pixel font used by tools/gen_font.py.
#
#   bash tools/fetch-fonts.sh
#
# The TTF is ~7 MB and is not committed; it is verified against a pinned SHA-256.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/tools/fonts"
URL="https://github.com/TakWolf/fusion-pixel-font/releases/download/2026.11.10/fusion-pixel-font-12px-monospaced-ttf-v2026.11.10.zip"
SHA="2d13900c20b57a01c00eea13b1f050a96a3392d1db7fbf2b1b6b8aee59ca7859"
ZIP="$DEST/fusion-pixel.zip"

mkdir -p "$DEST"
if [ -f "$DEST/fusion-pixel-12px-monospaced-zh_hans.ttf" ] && [ -f "$DEST/OFL.txt" ]; then
  echo "字体已存在，跳过下载。"
  exit 0
fi

if [ ! -f "$ZIP" ]; then
  echo "下载 Fusion Pixel Font…"
  curl -fL --retry 3 --connect-timeout 20 -o "$ZIP" "$URL"
fi

if command -v shasum >/dev/null 2>&1; then
  ACTUAL=$(shasum -a 256 "$ZIP" | awk '{print $1}')
else
  ACTUAL=$(sha256sum "$ZIP" | awk '{print $1}')
fi
if [ "$ACTUAL" != "$SHA" ]; then
  echo "字体包校验失败："
  echo "  expected $SHA"
  echo "  actual   $ACTUAL"
  exit 1
fi

unzip -o -j "$ZIP" \
  fusion-pixel-12px-monospaced-zh_hans.ttf OFL.txt -d "$DEST" >/dev/null
echo "已安装：$DEST/fusion-pixel-12px-monospaced-zh_hans.ttf"
