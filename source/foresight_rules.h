#pragma once
// SPD v4.0.2 TalismanOfForesight / ConeAOE. GPL-3.0-or-later.
// Copyright (C) 2014-2026 Evan Debenham.
#include "ballistica.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace foresight_rules {
inline float distance(int a, int b, int stride) {
  const int dx = a % stride - b % stride, dy = a / stride - b / stride;
  return float(std::sqrt(double(dx * dx + dy * dy)));
}
struct Scan {
  int target;
  float distance;
  std::vector<int> cells;
};
inline Scan scan(int from, int target, int stride, int width, int height,
                 int level, int charge) {
  // Upstream linear-cell arithmetic uses the actual level width. Convert
  // padded native rows before extending targets at the map boundary.
  if (stride != width) {
    auto result = scan(from % stride + from / stride * width,
        target % stride + target / stride * width, width, width, height, level, charge);
    result.target = result.target % width + result.target / width * stride;
    for (int &cell : result.cells) cell = cell % width + cell / width * stride;
    return result;
  }
  auto inside = [&](int c) {
    return c >= 0 && c % stride > 0 && c % stride < width - 1 &&
           c / stride > 0 && c / stride < height - 1;
  };
  auto ray = [&](int to) {
    return ballistica::cast(from, to, stride, ballistica::stop_target, inside,
                           [](int) { return true; }, [](int) { return false; },
                           [](int) { return false; }, [](int) { return false; });
  };
  if (target == from) return {target, 0, {}};
  if (std::max(std::abs(target % stride - from % stride),
               std::abs(target / stride - from / stride)) == 1)
    target += target - from;
  float dist = distance(from, target, stride);
  const float max_dist = std::min(float(5 + 2 * level), (charge - 3) / 1.08f);
  if (dist >= 3 && dist > max_dist) {
    for (int cell : ray(target).path) {
      if (distance(from, cell, stride) > max_dist) break;
      target = cell;
    }
    dist = distance(from, target, stride);
  }
  const auto core = ray(target);
  const float fx = from % stride + .5f, fy = from / stride + .5f;
  const float tx = core.collision % stride + .5f;
  const float ty = core.collision / stride + .5f;
  const float dx = tx - fx, dy = ty - fy;
  const float radius = float(std::sqrt(double(dx * dx + dy * dy))) + .5f;
  constexpr float g2r = 3.1415926f / 180;
  const float angle = float(std::atan2(double(dy), double(dx))) / g2r;
  const float degrees = std::floor(200 * float(std::pow(.92, dist)) + .5f);
  std::vector<uint8_t> targets(stride * height), cells(stride * height);
  for (float a = angle + degrees / 2; a >= angle - degrees / 2; a -= .5f) {
    for (int inner = 0; inner <= (radius >= 4 ? 1 : 0); ++inner) {
      const float radians = a * g2r;
      float x = (radius - inner) * float(std::cos(double(radians))) + fx;
      float y = (radius - inner) * float(std::sin(double(radians))) + fy;
      x += fx > x ? .5f : -.5f;
      y += fy > y ? .5f : -.5f;
      const int cell = std::clamp(int(std::floor(x)), 0, width - 1) +
                       stride * std::clamp(int(std::floor(y)), 0, height - 1);
      targets[cell] = 1;
    }
  }
  for (int c = 0; c < int(targets.size()); ++c) if (targets[c]) {
    const auto r = ray(c);
    for (int i = 1; i <= r.distance; ++i) cells[r.path[i]] = 1;
  }
  Scan result{target, dist, {}};
  for (int c = 0; c < int(cells.size()); ++c)
    if (cells[c]) result.cells.push_back(c);
  return result;
}
inline void spend(int &charge, float &partial, float dist) {
  charge = int(charge - (3 + dist * 1.08f));
  partial -= std::fmod(dist * 1.08f, 1.f);
  if (partial < 0 && charge > 0) { ++partial; --charge; }
  while (charge < 0) { ++charge; --partial; }
}
} // namespace foresight_rules
