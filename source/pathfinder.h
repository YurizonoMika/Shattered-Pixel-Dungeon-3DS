#pragma once

#include <algorithm>
#include <cstdint>

// Native port of the step and retreat searches in SPD v4.0.2 PathFinder.java
// Copyright (C) 2012-2015 Oleg Dolya, 2014-2026 Evan Debenham.
// SPDX-License-Identifier: GPL-3.0-or-later
template<int Width, int Height> class PathFinder {
  static constexpr int Size = Width * Height;
  static constexpr int Infinity = Size + 1;
  static_assert(Infinity <= INT16_MAX);
  int16_t distance[Size], queue[Size];
  bool passable[Size], queued[Size];

  template<class F> static void adjacent(int p, bool scan, F visit) {
    // dirLR for flood fill; dir for selecting a step (orthogonals first).
    static constexpr int dx[2][8] = {{-1, 1, 0, 0, -1, 1, -1, 1},
                                     {-1, -1, -1, 0, 0, 1, 1, 1}};
    static constexpr int dy[2][8] = {{0, 0, -1, 1, -1, -1, 1, 1},
                                     {-1, 0, 1, -1, 1, -1, 0, 1}};
    for (int i = 0; i < 8; ++i) {
      const int x = p % Width + dx[scan][i], y = p / Width + dy[scan][i];
      if (x >= 0 && x < Width && y >= 0 && y < Height) visit(y * Width + x);
    }
  }

  int descent(int start) const {
    int best = start;
    adjacent(start, false, [&](int n) { if (distance[n] < distance[best]) best = n; });
    return best;
  }

  template<class F> bool flood(int start, int tail, F pass) {
    for (int head = 0; head < tail; ++head) {
      const int p = queue[head];
      if (p == start) return true;
      adjacent(p, true, [&](int n) {
        if (distance[n] == Infinity && (n == start || pass(n))) {
          distance[n] = distance[p] + 1;
          queue[tail++] = n;
        }
      });
    }
    return false;
  }

public:
  template<class F> int step(int start, int goal, F pass) {
    if (start == goal) return -1;
    std::fill_n(distance, Size, Infinity);
    distance[goal] = 0;
    queue[0] = goal;
    return flood(start, 1, pass) ? descent(start) : -1;
  }

  template<class F> int retreat(int start, int threat, int lookahead,
                                bool can_approach, F pass) {
    for (int i = 0; i < Size; ++i) passable[i] = pass(i);
    std::fill_n(distance, Size, Infinity);
    distance[threat] = 0;
    queue[0] = threat;
    int target = Infinity, farthest = 0, tail = 1;
    for (int head = 0; head < tail; ++head) {
      const int p = queue[head];
      if (distance[p] > target) break;
      farthest = distance[p];
      if (p == start) target = farthest + lookahead;
      adjacent(p, true, [&](int n) {
        if (passable[n] && distance[n] == Infinity) {
          distance[n] = distance[p] + 1;
          queue[tail++] = n;
        }
      });
    }
    if (!farthest) return -1;

    if (!can_approach) {
      int reachable = distance[start];
      std::fill_n(queued, Size, false);
      queue[0] = start;
      queued[start] = true;
      tail = 1;
      for (int head = 0; head < tail; ++head) {
        const int p = queue[head];
        reachable = std::max(reachable, int(distance[p]));
        adjacent(p, true, [&](int n) {
          if (!passable[n]) return;
          if (distance[n] < distance[start]) passable[n] = false;
          else if (!queued[n] && distance[n] >= distance[p]) {
            queued[n] = true;
            queue[tail++] = n;
          }
        });
      }
      farthest = std::min(farthest, reachable);
    }

    if (distance[start] == farthest) return -1;
    tail = 0;
    for (int i = 0; i < Size; ++i) {
      const bool goal = distance[i] == farthest;
      distance[i] = goal ? 0 : Infinity;
      if (goal) queue[tail++] = i;
    }
    return flood(start, tail, [&](int n) { return passable[n]; }) ? descent(start) : -1;
  }
};
