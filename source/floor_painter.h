#pragma once

// Terrain painters from Shattered Pixel Dungeon v4.0.2 (GPL-3.0-or-later).
// Copyright (C) 2012-2015 Oleg Dolya and (C) 2014-2026 Evan Debenham.
#include "floor_rooms.h"

namespace floor_layout {

enum class Paint : uint8_t { Wall, Floor, Chasm };

struct RoomPaint {
  Rect box;
  std::vector<Paint> cells;
  explicit RoomPaint(Rect p) : box(p), cells(p.w()*p.h()) {}
  Paint& at(Point p) { return cells[(p.y-box.t)*box.w()+p.x-box.l]; }
  void fill(Rect area, Paint terrain) {
    for (int y = area.t; y <= area.b; ++y)
      for (int x = area.l; x <= area.r; ++x) at({x,y}) = terrain;
  }
  void line(Point from, Point to, Paint terrain = Paint::Floor) {
    fill({std::min(from.x,to.x),std::min(from.y,to.y),
          std::max(from.x,to.x),std::max(from.y,to.y)},terrain);
  }
  void ellipse(Rect area, Paint terrain) {
    const double rx = area.w()/2.0, ry = area.h()/2.0;
    for (int i = 0; i < area.h(); ++i) {
      const double y = -ry+0.5+i;
      const double half = std::sqrt(rx*rx*(1.0-y*y/(ry*ry)));
      const int width = area.w()%2 ? 2*int(half)+1 : 2*int(half+0.5);
      const int left = area.l+(area.w()-width)/2;
      fill({left,area.t+i,left+width-1,area.t+i},terrain);
    }
  }
};

inline Rect inset(Rect r, int n) { return {r.l+n,r.t+n,r.r-n,r.b-n}; }
inline Point inward(Rect r, Point d) {
  return {d.x == r.l ? 1 : d.x == r.r ? -1 : 0,
          d.y == r.t ? 1 : d.y == r.b ? -1 : 0};
}
inline bool chasm_connector(int type) { return type == Bridge || type == RingBridge || type == Walkway; }

inline void paint_perimeter(RoomPaint& out, Rect p, const std::vector<Point>& starts) {
  const Point corners[] = {{p.l,p.t},{p.r,p.t},{p.r,p.b},{p.l,p.b}};
  auto distance = [&](Point a, Point b) {
    if (((a.x == p.l || a.x == p.r) && a.y == b.y) ||
        ((a.y == p.t || a.y == p.b) && a.x == b.x))
      return std::max(std::abs(a.x-b.x),std::abs(a.y-b.y))-1;
    return std::min(a.x+b.x-2*p.l,2*p.r-a.x-b.x) +
           std::min(a.y+b.y-2*p.t,2*p.b-a.y-b.y)-1;
  };
  auto path = [&](auto&& self, Point a, Point b) -> void {
    if (((a.x == p.l || a.x == p.r) && a.x == b.x) ||
        ((a.y == p.t || a.y == p.b) && a.y == b.y)) { out.line(a,b); return; }
    for (Point c : corners) if ((c.x == a.x || c.y == a.y) && (c.x == b.x || c.y == b.y)) {
      out.line(a,c); out.line(c,b); return;
    }
    Point mid;
    if (a.y == p.t || a.y == p.b)
      mid = {a.x+b.x <= p.l+p.r ? p.l : p.r, p.t+p.h()/2};
    else mid = {p.l+p.w()/2, a.y+b.y <= p.t+p.b ? p.t : p.b};
    self(self,a,mid); self(self,mid,b);
  };
  std::vector<Point> pending(starts.begin()+1,starts.end()), painted{starts.front()};
  while (!pending.empty()) {
    size_t next = 0;
    Point from = painted.front();
    int best = std::numeric_limits<int>::max();
    for (Point a : painted) for (size_t i = 0; i < pending.size(); ++i) {
      const int d = distance(a,pending[i]);
      if (d < best) { best = d; from = a; next = i; }
    }
    path(path,from,pending[next]);
    painted.push_back(pending[next]);
    pending.erase(pending.begin()+next);
  }
}

inline RoomPaint paint_connector(Rect p, int type, const std::vector<Point>& doors, Random& rng) {
  RoomPaint out(p);
  if (doors.empty()) return out;
  const Rect inner = inset(p,1);
  if (chasm_connector(type) && std::min(p.w(),p.h()) > 3) out.fill(inner,Paint::Chasm);
  Point sum{};
  std::vector<Point> starts;
  for (Point door : doors) {
    sum.x += door.x; sum.y += door.y;
    const Point step = inward(p,door);
    starts.push_back({door.x+step.x,door.y+step.y});
  }
  if (type == Perimeter || type == Walkway) {
    paint_perimeter(out,inner,starts);
    return out;
  }
  const int radius = type == RingTunnel || type == RingBridge ? 1 : 0;
  const int count = int(doors.size());
  const Point center{std::clamp(sum.x/count,inner.l+radius,inner.r-radius),
                     std::clamp(sum.y/count,inner.t+radius,inner.b-radius)};
  // TunnelRoom's getDoorCenter consumes two draws even for integral sums.
  rng.unit(); rng.unit();
  for (size_t i = 0; i < doors.size(); ++i) {
    const Point start = starts[i];
    const Point end{std::clamp(start.x,center.x-radius,center.x+radius),
                    std::clamp(start.y,center.y-radius,center.y+radius)};
    const Point bend = inward(p,doors[i]).x ? Point{end.x,start.y} : Point{start.x,end.y};
    out.line(start,bend); out.line(bend,end);
  }
  if (radius) {
    out.fill({center.x-1,center.y-1,center.x+1,center.y+1},Paint::Floor);
    out.at(center) = Paint::Wall;
  } else if (p.w() >= 7 && p.h() >= 7 && count >= 4) {
    constexpr Point circle[] = {{-1,-1},{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0}};
    const int i = 2*rng.index(4);
    auto cell = [&](int j) -> Paint& { return out.at({center.x+circle[j].x,center.y+circle[j].y}); };
    if (cell((i+7)%8) == Paint::Floor && cell((i+1)%8) == Paint::Floor) cell(i) = Paint::Floor;
  }
  return out;
}

inline bool has_standard_painter(RoomClass type) {
  return type == RoomClass::Pillars || type == RoomClass::Segmented || type == RoomClass::CircleWall;
}

inline RoomPaint paint_standard(Rect p, RoomClass type, int factor, const std::vector<Point>& doors, Random& rng) {
  RoomPaint out(p);
  const Rect inner = inset(p,1);
  auto range = [&](int lo, int hi) { return lo+rng.index(hi-lo+1); };
  if (type == RoomClass::CircleWall) {
    out.ellipse(inner,Paint::Floor);
    for (Point d : doors) {
      const Point s = inward(p,d);
      const int n = s.x ? p.w()/2 : p.h()/2;
      out.line({d.x+s.x,d.y+s.y},{d.x+n*s.x,d.y+n*s.y});
    }
    out.ellipse(inset(p,3),Paint::Wall);
  } else if (type == RoomClass::Pillars) {
    out.fill(inner,Paint::Floor);
    const int dim = std::min(p.w(),p.h());
    auto pillar = [&](int x, int y, int size) { out.fill({x,y,x+size-1,y+size-1},Paint::Wall); };
    if (dim == 7 || (factor == 1 && rng.index(2) == 0)) {
      const int inset = dim >= 11 ? 2 : 1, size = (dim-3)/2-inset;
      const int l = p.l+1+inset, t = p.t+1+inset;
      int x = l, y = t;
      if (rng.index(2) == 0) x = range(l,p.r-size-inset);
      else y = range(t,p.b-size-inset);
      pillar(x,y,size);
      pillar(p.l+p.r-x-size+1,p.t+p.b-y-size+1,size);
    } else {
      const int inset = dim >= 12 ? 2 : 1, size = (dim-6)/(inset+1);
      const int xs = p.w()-2*inset-size-2, ys = p.h()-2*inset-size-2;
      const int span = std::min(xs,ys);
      const float skew = std::floor(rng.unit()*span+0.5f)/span;
      const int dx = int(std::floor(skew*xs+0.5f)), dy = int(std::floor(skew*ys+0.5f));
      const int l = p.l+1+inset, t = p.t+1+inset, r = p.r-size-inset, b = p.b-size-inset;
      pillar(l+dx,t,size); pillar(r,t+dy,size); pillar(r-dx,b,size); pillar(l,b-dy,size);
    }
  } else {
    out.fill(inner,Paint::Floor);
    if (type == RoomClass::Segmented) {
      for (Point d : doors) out.at(d) = Paint::Floor;
      auto split = [&](auto&& self, Rect a) -> void {
        if (std::max(a.w(),a.h()) < 5 || std::min(a.w(),a.h()) < 3) return;
        if (a.w() > a.h() || (a.w() == a.h() && rng.index(2) == 0)) {
          for (int tries = 0; tries < 10; ++tries) {
            const int x = range(a.l+2,a.r-2);
            if (out.at({x,a.t-1}) != Paint::Wall || out.at({x,a.b+1}) != Paint::Wall) continue;
            out.line({x,a.t},{x,a.b},Paint::Wall);
            const int y = range(a.t,a.b-1);
            out.line({x,y},{x,y+1});
            self(self,{a.l,a.t,x-1,a.b}); self(self,{x+1,a.t,a.r,a.b});
            break;
          }
        } else {
          for (int tries = 0; tries < 10; ++tries) {
            const int y = range(a.t+2,a.b-2);
            if (out.at({a.l-1,y}) != Paint::Wall || out.at({a.r+1,y}) != Paint::Wall) continue;
            out.line({a.l,y},{a.r,y},Paint::Wall);
            const int x = range(a.l,a.r-1);
            out.line({x,y},{x+1,y});
            self(self,{a.l,a.t,a.r,y-1}); self(self,{a.l,y+1,a.r,a.b});
            break;
          }
        }
      };
      split(split,inner);
    }
  }
  return out;
}

} // namespace floor_layout
