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
# Build actual release code to produce compatibility fixtures independently.
mkdir -p "$scratch/legacy"
for file in $(git ls-tree --name-only c7c608a:source | grep -E '\.h$|^(game|items|content)\.cpp$'); do
  git show "c7c608a:source/$file" > "$scratch/legacy/$file"
done
cat > "$scratch/legacy/fixture.cpp" <<'EOF'
#include "game.h"
#include <cassert>
#include <fstream>
#include <iterator>
void dlog(const char*,...) {}
int main(){
  Game g;g.new_run(0,0x12345678);g.inv_n=0;g.add_item(IT_WATERSKIN);g.inv[0].charges=17;
  assert(g.save(108));
  std::ifstream in("sdmc:/3ds/spd/slot108.bin",std::ios::binary);
  std::string bytes{std::istreambuf_iterator<char>(in),{}};
  std::ofstream v13("legacy-v13.bin",std::ios::binary);v13.write(bytes.data(),bytes.size());v13.close();
  uint16_t version=12;memcpy(bytes.data()+4,&version,2);bytes.resize(bytes.rfind("DID1"));
  std::ofstream v12("legacy-v12.bin",std::ios::binary);v12.write(bytes.data(),bytes.size());
}
EOF
${CXX:-c++} -std=c++17 -O1 -I"$scratch/include" -I"$scratch/legacy" \
  "$scratch/legacy/game.cpp" "$scratch/legacy/content.cpp" "$scratch/legacy/items.cpp" \
  "$scratch/legacy/fixture.cpp" -o "$scratch/legacy-fixture"
(cd "$scratch" && ./legacy-fixture)
${CXX:-c++} -std=c++17 -O1 -g -fsanitize=address,undefined \
  -fno-omit-frame-pointer -I"$scratch/include" -Isource \
  source/game.cpp source/game_magic.cpp source/game_mechanics.cpp source/game_scrolls.cpp source/content.cpp source/items.cpp tools/check-game-rules.cpp \
  -o "$scratch/check-game-rules"
cd "$scratch"
"$scratch/check-game-rules"
