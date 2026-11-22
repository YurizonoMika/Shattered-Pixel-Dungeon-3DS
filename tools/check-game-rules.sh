#!/usr/bin/env bash
# Compile the actual game runtime with platform-only stubs. Run in an isolated
# directory so save/migration tests can never touch emulator or real SD saves.
set -euo pipefail
cd "$(dirname "$0")/.."
project=$PWD
scratch=$(mktemp -d "${TMPDIR:-/tmp}/opencode/spd-rules.XXXXXX")
trap 'find "$scratch" -depth -delete' EXIT
mkdir -p "$scratch/include" "$scratch/sdmc:/3ds/spd"
cat > "$scratch/include/3ds.h" <<'EOF'
#pragma once
#include <cstdint>
using u32 = uint32_t;
inline uint64_t osGetTime() { return 0; }
EOF
cat > "$scratch/include/citro2d.h" <<'EOF'
#pragma once
struct C3D_Tex {};
struct Tex3DS_SubTexture {};
EOF
${CXX:-c++} -std=c++17 -O1 -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer -I"$scratch/include" -Isource \
  source/game.cpp source/content.cpp source/items.cpp tools/check-game-rules.cpp \
  -o "$scratch/check-game-rules"
cd "$scratch"
"$scratch/check-game-rules"
