#pragma once

// SPD v4.0.2 Ballistica.build. GPL-3.0-or-later.
// Copyright (C) 2012-2015 Oleg Dolya; (C) 2014-2026 Evan Debenham.
#include <cstdlib>
#include <vector>
namespace ballistica {
constexpr int stop_target=1, stop_chars=2, stop_solid=4, ignore_soft_solid=8;
constexpr int projectile=7, magic_bolt=6;
struct Ray { std::vector<int> path; int collision=-1, distance=0; };
template<class Inside,class Passable,class Avoid,class Solid,class Occupied>
Ray cast(int from,int to,int width,int flags,Inside inside,Passable passable,
         Avoid avoid,Solid solid,Occupied occupied) {
  Ray ray;
  int dx=to%width-from%width, dy=to/width-from/width;
  const int sx=dx>0 ? 1 : -1, sy=dy>0 ? 1 : -1;
  dx=std::abs(dx); dy=std::abs(dy);
  const int step_a=dx>dy ? sx : sy*width, step_b=dx>dy ? sy*width : sx;
  const int da=dx>dy ? dx : dy, db=dx>dy ? dy : dx;
  int cell=from,error=da/2;
  auto collide=[&](int at) {
    if (ray.collision<0) { ray.collision=at; ray.distance=int(ray.path.size())-1; }
  };
  while (inside(cell)) {
    if (ray.collision<0 && (flags&stop_solid) && cell!=from &&
        !passable(cell) && !avoid(cell) && !occupied(cell) && !ray.path.empty())
      collide(ray.path.back());
    ray.path.push_back(cell);
    if (ray.collision<0 && (flags&stop_solid) && cell!=from && solid(cell)) {
      if (!((flags&ignore_soft_solid) && (passable(cell)||avoid(cell)))) collide(cell);
    }
    if (ray.collision<0 && cell!=from && (flags&stop_chars) && occupied(cell)) collide(cell);
    if (ray.collision<0 && cell==to && (flags&stop_target)) collide(cell);
    cell+=step_a;
    error+=db;
    if (error>=da) { error-=da; cell+=step_b; }
  }
  if (ray.collision<0) {
    if (ray.path.empty()) ray.path.push_back(from);
    ray.collision=ray.path.back(); ray.distance=int(ray.path.size())-1;
  }
  return ray;
}
} // namespace ballistica
