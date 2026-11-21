#pragma once

// Patch.java from Shattered Pixel Dungeon v4.0.2, GPL-3.0-or-later.
// Copyright (C) 2012-2015 Oleg Dolya and (C) 2014-2026 Evan Debenham.
#include "floor_layout.h"

namespace floor_layout {

inline std::vector<uint8_t> patch(int w, int h, float fill, int passes, bool force, Random& rng) {
  const int count = w*h;
  int diff = -int(std::floor(double(count*fill) + 0.5));
  if (force && passes > 0) fill += (0.5f-fill)*0.5f;
  std::vector<uint8_t> cells(count), next(count);
  for (auto& c : cells) {
    c = rng.unit() < fill;
    diff += c;
  }
  for (int pass = 0; pass < passes; ++pass) {
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
      const int i = y*w+x;
      int hits = 0, total = 0;
      for (int yy = std::max(0,y-1); yy <= std::min(h-1,y+1); ++yy)
        for (int xx = std::max(0,x-1); xx <= std::min(w-1,x+1); ++xx) {
          hits += cells[yy*w+xx];
          ++total;
        }
      next[i] = 2*hits >= total;
      diff += next[i] - cells[i];
    }
    cells.swap(next);
  }
  if (force && w > 2 && h > 2) {
    const bool grow = diff < 0;
    while (diff) {
      int cell, tries = 0;
      do {
        const int x = 1+rng.index(w-2), y = 1+rng.index(h-2);
        cell = y*w+x;
      } while (cells[cell] != grow && ++tries*10 < count);
      for (int y = -1; y <= 1 && diff; ++y) for (int x = -1; x <= 1 && diff; ++x) {
        auto& c = cells[cell+y*w+x];
        if (c != grow) { c = grow; diff += grow ? 1 : -1; }
      }
    }
  }
  return cells;
}

} // namespace floor_layout
