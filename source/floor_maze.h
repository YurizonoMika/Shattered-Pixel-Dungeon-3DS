#pragma once

// Maze.java from Shattered Pixel Dungeon v4.0.2, GPL-3.0-or-later.
// Copyright (C) 2012-2015 Oleg Dolya and (C) 2014-2026 Evan Debenham.
#include "floor_layout.h"

namespace floor_layout {

inline std::vector<uint8_t> maze(Rect box, const std::vector<Point>& doors, Random& rng) {
  const int w = box.w(), h = box.h();
  std::vector<uint8_t> cells(w*h);
  auto at = [&](int x, int y) -> uint8_t& { return cells[y*w+x]; };
  for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x)
    at(x,y) = x == 0 || y == 0 || x == w-1 || y == h-1;
  for (auto d : doors) at(d.x-box.l,d.y-box.t) = 0;

  constexpr Point moves[]{{0,-1},{1,0},{0,1},{-1,0}};
  auto valid = [&](int x, int y, Point d) {
    const int dx = 1-std::abs(d.x), dy = 1-std::abs(d.y);
    for (int step = 0; step < 2; ++step) {
      x += d.x; y += d.y;
      if (x <= 0 || x >= w-1 || y <= 0 || y >= h-1 ||
          at(x,y) || at(x+dx,y+dy) || at(x-dx,y-dy)) return false;
    }
    return true;
  };

  for (int fails = 0; fails < 2500;) {
    int x, y;
    do { x = rng.index(w); y = rng.index(h); } while (!at(x,y));
    int dir = 0;
    for (; dir < 4; ++dir)
      if ((dir == 3 || rng.index(4-dir) == 0) && valid(x,y,moves[dir])) break;
    if (dir == 4) { ++fails; continue; }
    fails = 0;
    int n = 0;
    const Point d = moves[dir];
    do { x += d.x; y += d.y; at(x,y) = 1; }
    while (rng.index(++n) == 0 && valid(x,y,d));
  }
  return cells;
}

} // namespace floor_layout
