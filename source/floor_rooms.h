#pragma once

// RegularLevel and the standard/entrance/exit room factories, Shattered Pixel
// Dungeon v4.0.2 (GPL-3.0-or-later). Copyright (C) 2014-2026 Evan Debenham.
#include "floor_layout.h"

namespace floor_layout {

enum class RoomClass {
  SewerPipe, Ring, WaterBridge, RegionDecoPatch, CircleBasin,
  RegionDecoLine, Segmented, Pillars, ChasmBridge, CellBlock,
  Cave, RegionDecoBridge, CavesFissure, CirclePit, CircleWall,
  Hallway, LibraryHall, LibraryRing, Statues, SegmentedLibrary,
  Ruins, Chasm, Skulls, Ritual, Plants, Aquarium, Platform, Burned,
  Fissure, GrassyGrave, Striped, Study, SuspiciousChest, Minefield
};

struct RoomInput {
  RoomClass type;
  Spec spec;
};

struct RoomRules {
  int min_dim;
  std::array<int,3> sizes;
};

inline RoomRules room_rules(RoomClass type, Kind kind) {
  using C = RoomClass;
  constexpr RoomRules regular[] = {
    {7,{3,2,1}}, {7,{9,3,1}}, {5,{1,0,0}}, {5,{1,0,0}}, {0,{0,3,1}},
    {5,{1,0,0}}, {7,{9,3,1}}, {7,{9,3,1}}, {5,{1,0,0}}, {4,{0,3,1}},
    {5,{4,2,1}}, {5,{2,1,0}}, {7,{9,3,1}}, {8,{4,2,1}}, {4,{0,3,1}},
    {5,{1,0,0}}, {7,{2,1,0}}, {9,{4,2,1}}, {7,{9,3,1}}, {4,{0,3,1}},
    {4,{4,2,1}}, {5,{4,2,1}}, {7,{0,3,1}}, {9,{6,3,1}}, {5,{3,1,0}},
    {7,{3,1,0}}, {6,{6,3,1}}, {4,{4,1,0}}, {5,{6,3,1}}, {4,{1,0,0}},
    {4,{2,1,0}}, {7,{2,1,0}}, {5,{1,0,0}}, {4,{4,1,0}}
  };
  RoomRules r = regular[int(type)];
  if (kind == Kind::Standard) return r;
  switch (type) {
    case C::WaterBridge: case C::RegionDecoPatch: case C::RegionDecoLine:
    case C::ChasmBridge: return {7,{1,0,0}};
    case C::Ring: return {7,{0,1,0}};
    case C::CircleBasin: return {0,{0,1,0}};
    case C::CellBlock: return {4,{0,1,0}};
    case C::Pillars: case C::CavesFissure: case C::Statues: return {7,{3,1,0}};
    case C::Cave: case C::Ruins: case C::Chasm: return {7,{2,1,0}};
    case C::RegionDecoBridge: return {8,{2,1,0}};
    case C::CircleWall: return {11,{0,1,0}};
    case C::LibraryRing: return {13,{0,1,0}};
    case C::Ritual: return {9,{0,1,0}};
    default: return r;
  }
}

inline Spec room_spec(RoomClass type, Kind kind, RoomRules r, int category) {
  const int min_cat[] = {4,10,14}, max_cat[] = {10,14,18};
  const int min = type == RoomClass::CircleBasin ? min_cat[category]+1 :
                                                  std::max(r.min_dim,min_cat[category]);
  const int max = max_cat[category];
  const auto parity = type == RoomClass::CircleBasin ? DimParity::Odd :
      type == RoomClass::LibraryRing && category == 2 ? DimParity::Even : DimParity::Any;
  return {kind,min,max,min,max,category+1,16,4,parity,type == RoomClass::SewerPipe ? 2 : 1};
}

inline std::vector<RoomInput> standard_room_inputs(Random& rng, int depth, bool large) {
  using C = RoomClass;
  constexpr C transitions[5][4] = {
    {C::WaterBridge,C::RegionDecoPatch,C::Ring,C::CircleBasin},
    {C::RegionDecoLine,C::ChasmBridge,C::Pillars,C::CellBlock},
    {C::Cave,C::RegionDecoBridge,C::CavesFissure,C::CircleWall},
    {C::Hallway,C::Statues,C::LibraryHall,C::LibraryRing},
    {C::RegionDecoPatch,C::Ruins,C::Chasm,C::Ritual}
  };
  constexpr C regional[5][5] = {
    {C::SewerPipe,C::Ring,C::WaterBridge,C::RegionDecoPatch,C::CircleBasin},
    {C::RegionDecoLine,C::Segmented,C::Pillars,C::ChasmBridge,C::CellBlock},
    {C::Cave,C::RegionDecoBridge,C::CavesFissure,C::CirclePit,C::CircleWall},
    {C::Hallway,C::LibraryHall,C::LibraryRing,C::Statues,C::SegmentedLibrary},
    {C::Ruins,C::RegionDecoPatch,C::Chasm,C::Skulls,C::Ritual}
  };
  constexpr C shared[] = {C::Plants,C::Aquarium,C::Platform,C::Burned,C::Fissure,
                          C::GrassyGrave,C::Striped,C::Study,C::SuspiciousChest,C::Minefield};
  constexpr std::array<int,15> weights[] = {
    {16,8,8,4,4, 1,1,1,1,1,1,1,1,1,1}, {10,10,10,5,5, 1,1,1,1,1,1,1,1,1,1},
    {16,8,8,4,4, 1,1,1,1,1,1,1,1,1,1}, {10,10,10,5,5, 1,1,1,1,1,1,1,1,1,1},
    {10,10,10,5,5, 1,1,1,1,1,1,1,1,1,1}
  };
  const int region = (depth-1)/5;
  std::vector<RoomInput> rooms;
  for (Kind kind : {Kind::Entrance, Kind::Exit}) {
    const bool early = depth == 1 || (depth == 2 && kind == Kind::Entrance);
    const auto type = transitions[region][rng.chances(std::array<int,4>{4,3,early ? 0 : 2,early ? 0 : 1})];
    const auto rules = room_rules(type,kind);
    rooms.push_back({type,room_spec(type,kind,rules,rng.chances(rules.sizes))});
  }
  constexpr int base[] = {4,5,6,6,8}, maximum[] = {6,6,7,8,9};
  constexpr std::array<int,3> budgets[] = {{1,3,1},{1,1,0},{2,1,0},{1,3,1},{2,1,0}};
  int remaining = large ? (maximum[region]*3+1)/2 : base[region] + rng.chances(budgets[region]);
  const auto normal = depth == 1 ? std::array<int,15>{16,8,8,4,4, 1,0,1,0,1,0,1,1,0,0} : weights[region];
  while (remaining > 0) {
    // StandardRoom's constructor rolls once before initRooms applies its budget.
    const int id = rng.chances(normal);
    const auto type = id < 5 ? regional[region][id] : shared[id-5];
    auto rules = room_rules(type,Kind::Standard);
    rng.chances(rules.sizes);
    for (int i = remaining; i < 3; ++i) rules.sizes[i] = 0;
    const int category = rng.chances(rules.sizes);
    if (category < 0) continue;
    rooms.push_back({type,room_spec(type,Kind::Standard,rules,category)});
    remaining -= category+1;
  }
  return rooms;
}

} // namespace floor_layout
