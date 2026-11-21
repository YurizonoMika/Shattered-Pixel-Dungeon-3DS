#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

// Angle-guided placement on shared walls, following the structure of SPD's
// LoopBuilder and FigureEightBuilder. Coordinates are normalized after packing.
namespace floor_layout {
struct Rect { int x, y, w, h; };
struct Door { int x, y; };
struct Edge { int a, b; Door door; };

inline bool overlaps(Rect a, Rect b) {
  return a.x + a.w - 1 > b.x && b.x + b.w - 1 > a.x &&
         a.y + a.h - 1 > b.y && b.y + b.h - 1 > a.y;
}

inline std::vector<Door> shared_wall(Rect a, Rect b) {
  std::vector<Door> doors;
  if (a.x + a.w - 1 == b.x || b.x + b.w - 1 == a.x) {
    for (int y = std::max(a.y, b.y) + 1; y < std::min(a.y + a.h, b.y + b.h) - 1; ++y)
      doors.push_back({std::max(a.x, b.x), y});
  }
  if (a.y + a.h - 1 == b.y || b.y + b.h - 1 == a.y) {
    for (int x = std::max(a.x, b.x) + 1; x < std::min(a.x + a.w, b.x + b.w) - 1; ++x)
      doors.push_back({x, std::max(a.y, b.y)});
  }
  return doors;
}

struct Layout {
  std::vector<Rect> rooms;
  std::vector<Edge> edges;
  int width = 0, height = 0;
  bool figure_eight = false;
};

template<class Random> Layout build(int regular, int leaves, bool shop, bool eight,
                                    int max_width, int max_height, Random random) {
  const int count = regular + leaves;
  const float tau = 6.28318531f;
  for (int attempt = 0; attempt < 100; ++attempt) {
    Layout out;
    out.rooms.resize(count);
    out.figure_eight = eight;
    std::vector<int> placed{0};
    for (auto& r : out.rooms) r = {0, 0, random(5, 8), random(5, 8)};
    auto connect = [&](int a, int b) {
      auto ds = shared_wall(out.rooms[a], out.rooms[b]);
      if (ds.empty()) return false;
      out.edges.push_back({a, b, ds[random(0, int(ds.size()) - 1)]});
      return true;
    };
    auto place = [&](int from, int index, float angle, int close_to) {
      const Rect a = out.rooms[from];
      const Rect size = out.rooms[index];
      float best = 2.f;
      Rect selected{};
      bool found = false;
      auto consider = [&](int x, int y) {
        const Rect b{x, y, size.w, size.h};
        int min_x = x, min_y = y, max_x = x + size.w, max_y = y + size.h;
        for (int i : placed) {
          const auto r = out.rooms[i];
          if (overlaps(b, r)) return;
          min_x = std::min(min_x, r.x); min_y = std::min(min_y, r.y);
          max_x = std::max(max_x, r.x + r.w); max_y = std::max(max_y, r.y + r.h);
        }
        if (max_x - min_x + 2 > max_width || max_y - min_y + 2 > max_height) return;
        if (close_to >= 0 && shared_wall(b, out.rooms[close_to]).empty()) return;
        float bearing = std::atan2(float(2 * (y - a.y) + b.h - a.h),
                                   float(2 * (x - a.x) + b.w - a.w));
        float score = 1.f - std::cos(bearing - angle) + 0.15f * random(0, 100) / 100.f;
        if (!found || score < best) { selected = b; best = score; found = true; }
      };
      for (int y = a.y - size.h + 3; y <= a.y + a.h - 3; ++y) {
        consider(a.x + a.w - 1, y);
        consider(a.x - size.w + 1, y);
      }
      for (int x = a.x - size.w + 3; x <= a.x + a.w - 3; ++x) {
        consider(x, a.y + a.h - 1);
        consider(x, a.y - size.h + 1);
      }
      if (!found) return false;
      out.rooms[index] = selected;
      connect(from, index);
      if (close_to >= 0) connect(index, close_to);
      placed.push_back(index);
      return true;
    };

    const float start = random(0, 359) * tau / 360.f;
    auto loop = [&](int first, int end, float heading) {
      int previous = 0;
      for (int i = first; i < end; ++i) {
        const float angle = heading + (i - first) * tau / (end - first + 1);
        if (!place(previous, i, angle, i + 1 == end ? 0 : -1)) return false;
        previous = i;
      }
      return true;
    };
    // Keep the cycles small; the remaining standard rooms become branches.
    const int loop_end = std::min(regular, eight ? 7 : 6);
    bool ok = eight ? loop(1, 4, start) && loop(4, loop_end, start + tau / 2)
                    : loop(1, loop_end, start);
    if (!ok) continue;
    for (int i = loop_end; i < count; ++i) {
      const int eligible = std::min(i, regular);
      const bool entrance_shop = shop && i == regular;
      const int initial = entrance_shop ? (eight ? 2 : 0) : random(0, eligible - 1);
      ok = false;
      for (int j = 0; j < (entrance_shop ? 1 : eligible); ++j) {
        if (place((initial + j) % eligible, i, random(0, 359) * tau / 360.f, -1)) {
          ok = true;
          break;
        }
      }
      if (!ok) break;
    }
    if (!ok) continue;

    int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    for (const auto& r : out.rooms) {
      min_x = std::min(min_x, r.x); min_y = std::min(min_y, r.y);
      max_x = std::max(max_x, r.x + r.w); max_y = std::max(max_y, r.y + r.h);
    }
    out.width = max_x - min_x + 2;
    out.height = max_y - min_y + 2;
    for (auto& r : out.rooms) { r.x += 1 - min_x; r.y += 1 - min_y; }
    for (auto& e : out.edges) { e.door.x += 1 - min_x; e.door.y += 1 - min_y; }
    return out;
  }
  return {};
}
}  // namespace floor_layout
