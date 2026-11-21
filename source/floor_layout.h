#pragma once

// Builder.java, RegularBuilder.java, LoopBuilder.java and FigureEightBuilder.java
// adapted from Shattered Pixel Dungeon v4.0.2 (GPL-3.0-or-later).
// Copyright (C) 2012-2015 Oleg Dolya and (C) 2014-2026 Evan Debenham.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace floor_layout {

// java.util.Random, with the MX3 seed scramble from SPD's Random.pushGenerator.
class Random {
  uint64_t state;
  uint32_t bits(int n) {
    state = (state * 0x5deece66dULL + 11) & ((1ULL << 48) - 1);
    return uint32_t(state >> (48 - n));
  }
public:
  explicit Random(uint64_t seed) {
    constexpr uint64_t m = 0xbea225f9eb34556dULL;
    seed ^= seed >> 32; seed *= m;
    seed ^= seed >> 29; seed *= m;
    seed ^= seed >> 32; seed *= m;
    seed ^= seed >> 29;
    state = (seed ^ 0x5deece66dULL) & ((1ULL << 48) - 1);
  }
  uint64_t next_long() {
    const uint64_t hi = bits(32), lo = bits(32);
    return (hi << 32) + lo - (lo & 0x80000000ULL ? 1ULL << 32 : 0);
  }
  int index(int n) {
    if (n <= 0) return 0;
    if ((n & (n - 1)) == 0) return int((uint64_t(n) * bits(31)) >> 31);
    uint32_t v, rem;
    do { v = bits(31); rem = v % uint32_t(n); }
    while (v - rem > 0x80000000u - uint32_t(n));
    return int(rem);
  }
  float unit() { return bits(24) * (1.f / 16777216.f); }
  int normal(int lo, int hi) {
    const float a = unit(), b = unit();
    return lo + int((a + b) * (hi - lo + 1) / 2.f);
  }
  template<class T> void shuffle(std::vector<T>& v) {
    for (int i = int(v.size()); i > 1; --i) std::swap(v[i - 1], v[index(i)]);
  }
  template<size_t N> int chances(const std::array<int, N>& weights) {
    int total = 0;
    for (int n : weights) total += n;
    if (!total) return -1;
    const float draw = unit() * total;
    total = 0;
    for (size_t i = 0; i < N; ++i) if (draw < (total += weights[i])) return int(i);
    return -1;
  }
};

inline Random floor_random(uint64_t seed, int depth) {
  Random run(seed);
  for (int i = 0; i < depth; ++i) run.next_long();
  return Random(run.next_long());
}

enum class Kind { Entrance, Exit, Standard, Shop, Leaf, Secret, Connector };
enum Connector { Tunnel, Bridge, Perimeter, Walkway, RingTunnel, RingBridge, Maze };
struct Spec {
  Kind kind;
  int min_w, max_w, min_h, max_h;
  int factor = 1, total = 16, side = 4;
};
struct Point { int x, y; };
struct Center { float x, y; };
struct Rect {
  int l = 0, t = 0, r = 0, b = 0;
  int w() const { return r-l+1; }
  int h() const { return b-t+1; }
  Center center() const { return {(l+r)/2.f, (t+b)/2.f}; }
  void shift(int x, int y) { l += x; r += x; t += y; b += y; }
};
struct Room {
  Spec spec;
  Rect box{};
  int type = -1;
  std::vector<int> neighbors{}, links{};
};
struct Layout {
  std::vector<Room> rooms;
  std::vector<int> order;
  int width = 0, height = 0;
};
struct Shape {
  bool eight;
  float intensity, offset;
  float angle(float position) const {
    position += offset;
    const double x = position;
    const double curve = 256 * std::pow(std::fmod(x, 0.5) - 0.25, 5) + 0.25 + 0.5*std::floor(2*x);
    return 360.f * float(intensity * curve + (1.f-intensity)*position - offset);
  }
};

inline Shape select_shape(Random& rng) {
  if (rng.index(2) == 0) return {false, rng.unit() * 0.65f, rng.unit() * 0.50f};
  return {true, 0.3f + rng.unit() * (0.8f - 0.3f), 0.f};
}

inline int side_of(Rect a, Rect b) {
  if (std::min(a.b,b.b) - std::max(a.t,b.t) >= 2) {
    if (a.l == b.r) return 0;
    if (a.r == b.l) return 2;
  }
  if (std::min(a.r,b.r) - std::max(a.l,b.l) >= 2) {
    if (a.t == b.b) return 1;
    if (a.b == b.t) return 3;
  }
  return -1;
}

inline Point choose_door(Rect a, Rect b, Random& rng) {
  const int side = side_of(a, b);
  if (side % 2 == 0) return {std::max(a.l,b.l), std::max(a.t,b.t) + 1 +
      rng.index(std::min(a.b,b.b) - std::max(a.t,b.t) - 1)};
  return {std::max(a.l,b.l) + 1 + rng.index(std::min(a.r,b.r) - std::max(a.l,b.l) - 1),
          std::max(a.t,b.t)};
}

class Builder {
  Random& rng;
  Shape shape;
  int depth;
  int landmark = -1;
  Layout out;
  static constexpr double degrees = 180.0 / 3.14159265358979323846;

  static bool contains(const std::vector<int>& list, int i) {
    return std::find(list.begin(), list.end(), i) != list.end();
  }
  static void erase(std::vector<int>& list, int i) {
    list.erase(std::remove(list.begin(), list.end(), i), list.end());
  }
  static float angle_between(Center a, Center b) {
    const double slope = (b.y-a.y)/(b.x-a.x);
    const float angle = float(degrees * (std::atan(slope) + 90.0/degrees));
    return a.x > b.x ? angle - 180.f : angle;
  }
  static int round_java(float x) { return int(std::floor(double(x) + 0.5)); }
  void add_neighbor(int a, int b) {
    if (contains(out.rooms[a].neighbors, b)) return;
    out.rooms[a].neighbors.push_back(b);
    out.rooms[b].neighbors.push_back(a);
  }
  bool connect(int a, int b) {
    auto& ra = out.rooms[a]; auto& rb = out.rooms[b];
    const int s = side_of(ra.box, rb.box);
    if (s >= 0) add_neighbor(a, b);
    if (s < 0 || contains(ra.links, b) ||
        (ra.spec.kind == Kind::Entrance && rb.spec.kind == Kind::Exit) ||
        (ra.spec.kind == Kind::Exit && rb.spec.kind == Kind::Entrance)) return false;
    auto available = [&](const Room& r, int side) {
      if (int(r.links.size()) >= r.spec.total) return false;
      int used = 0;
      for (int n : r.links) used += side_of(r.box, out.rooms[n].box) == side;
      return used < r.spec.side;
    };
    if (!available(ra, s) || !available(rb, (s+2)%4)) return false;
    ra.links.push_back(b); rb.links.push_back(a);
    return true;
  }
  void clear(int i) {
    auto& room = out.rooms[i];
    for (int n : room.neighbors) erase(out.rooms[n].neighbors, i);
    for (int n : room.links) erase(out.rooms[n].links, i);
    room.neighbors.clear(); room.links.clear();
  }

  Rect free_space(Point p, const std::vector<int>& collision, int max) {
    Rect space{p.x-max, p.y-max, p.x+max, p.y+max};
    auto hits = collision;
    while (!hits.empty()) {
      int closest = -1, distance = std::numeric_limits<int>::max();
      for (int i : hits) {
        const auto r = out.rooms[i].box;
        if (r.w() <= 1 || r.h() <= 1 || std::max(space.l,r.l) >= std::min(space.r,r.r) ||
            std::max(space.t,r.t) >= std::min(space.b,r.b)) continue;
        if (p.x > r.l && p.x < r.r && p.y > r.t && p.y < r.b) return {p.x,p.y,p.x,p.y};
        const int dx = std::max({r.l-p.x, p.x-r.r, 0});
        const int dy = std::max({r.t-p.y, p.y-r.b, 0});
        if (dx*dx + dy*dy < distance) { closest = i; distance = dx*dx + dy*dy; }
      }
      if (closest < 0) break;
      const Rect r = out.rooms[closest].box;
      const int inf = std::numeric_limits<int>::max();
      const int dx = r.l >= p.x ? (space.r-r.l)*space.h() : r.r <= p.x ? (r.r-space.l)*space.h() : inf;
      const int dy = r.t >= p.y ? (space.b-r.t)*space.w() : r.b <= p.y ? (r.b-space.t)*space.w() : inf;
      if (dx < dy || (dx == dy && rng.index(2) == 0)) {
        if (r.l >= p.x) space.r = std::min(space.r,r.l);
        if (r.r <= p.x) space.l = std::max(space.l,r.r);
      } else {
        if (r.t >= p.y) space.b = std::min(space.b,r.t);
        if (r.b <= p.y) space.t = std::max(space.t,r.b);
      }
      erase(hits, closest);
    }
    return space;
  }

  bool place(const std::vector<int>& collision, int from, int id, float angle) {
    angle = std::fmod(angle, 360.f);
    if (angle < 0) angle += 360.f;
    const Rect p = out.rooms[from].box;
    const auto c = p.center();
    const double m = std::tan(angle/degrees + 90.0/degrees), b = c.y - m*c.x;
    const bool vertical = std::abs(m) >= 1;
    const bool forward = vertical ? angle >= 90 && angle <= 270 : angle < 180;
    Point start{};
    if (vertical) {
      start.y = forward ? p.b : p.t;
      start.x = std::clamp(int(std::floor((start.y-b)/m + 0.5)), p.l+1, p.r-1);
    } else {
      start.x = forward ? p.r : p.l;
      start.y = std::clamp(int(std::floor(m*start.x+b + 0.5)), p.t+1, p.b-1);
    }
    auto& room = out.rooms[id];
    const Spec s = room.spec;
    const Rect space = free_space(start, collision, std::max(s.max_w,s.max_h));
    if (space.w() < s.min_w || space.h() < s.min_h) return false;
    const int w = std::min(space.w(), rng.normal(s.min_w,s.max_w))-1;
    const int h = std::min(space.h(), rng.normal(s.min_h,s.max_h))-1;
    Rect n{0,0,w,h};
    if (vertical) {
      const int y = forward ? p.b : p.t-h;
      const float cx = float(((y+h/2.f)-b)/m);
      n.shift(round_java(cx-w/2.f), y);
      n.shift(std::clamp(n.l, p.l+2-w, p.r-2)-n.l, 0);
      n.shift(std::clamp(n.l, space.l, space.r-w)-n.l, 0);
    } else {
      const int x = forward ? p.r : p.l-w;
      const float cy = float(m*(x+w/2.f)+b);
      n.shift(x, round_java(cy-h/2.f));
      n.shift(0, std::clamp(n.t, p.t+2-h, p.b-2)-n.t);
      n.shift(0, std::clamp(n.t, space.t, space.b-h)-n.t);
    }
    room.box = n;
    return connect(id, from);
  }

  int corridor(bool secret = false) {
    static constexpr std::array<int,6> weights[] = {
        {20,1,0,2,2,1}, {0,0,22,3,0,0}, {12,0,0,5,5,3}, {0,0,18,3,3,1}, {15,4,0,2,3,2}};
    static constexpr std::array<int,6> tunnel_only{20,0,0,0,0,0};
    const int kind = secret ? int(Maze) : rng.chances(depth == 5 || depth == 21 ? tunnel_only : weights[(depth-1)/5]);
    const int min = kind == RingTunnel || kind == RingBridge ? 5 : 3;
    const int connections = kind == Maze ? 2 : 16;
    const int id = int(out.rooms.size());
    out.rooms.push_back({{Kind::Connector,min,10,min,10,1,connections,kind == Maze ? 2 : 4}, {}, kind});
    return id;
  }
  template<size_t N> int draw(std::array<int,N>& deck, const std::array<int,N>& defaults) {
    int n = rng.chances(deck);
    if (n < 0) { deck = defaults; n = rng.chances(deck); }
    --deck[n];
    return n;
  }
  void weight(std::vector<int>& list) {
    const int size = int(list.size());
    for (int i = 0; i < size; ++i) {
      const int id = list[i], f = out.rooms[id].spec.factor;
      for (int j = 1; j < f*f; ++j) list.push_back(id);
    }
  }
  std::vector<int> pad_loop(const std::vector<int>& path, std::array<int,3>& deck) {
    constexpr std::array<int,3> defaults{2,2,1};
    std::vector<int> loop;
    for (int r : path) {
      loop.push_back(r);
      for (int n = draw(deck, defaults); n > 0; --n) loop.push_back(corridor());
    }
    return loop;
  }
  bool lay_loop(std::vector<int>& loop, int root, float start) {
    int prev = root;
    for (size_t i = 1; i < loop.size(); ++i) {
      const int id = loop[i];
      if (!place(out.order, prev, id, start + shape.angle(float(i)/float(loop.size())))) return false;
      if (!contains(out.order, id)) out.order.push_back(id);
      prev = id;
    }
    for (int steps = 0; !connect(prev, root); ++steps) {
      if (steps == 32) return false;
      int id = corridor();
      const float angle = angle_between(out.rooms[prev].box.center(),out.rooms[root].box.center());
      if (!place(shape.eight ? out.order : loop, prev, id, angle)) return false;
      loop.push_back(id); out.order.push_back(id);
      prev = id;
    }
    return true;
  }
  Center center(const std::vector<int>& loop) const {
    Center c{};
    for (int i : loop) { auto p = out.rooms[i].box.center(); c.x += p.x; c.y += p.y; }
    return {c.x / loop.size(), c.y / loop.size()};
  }
  float branch_angle(int id, Center target) {
    float heading = angle_between(out.rooms[id].box.center(), target);
    if (heading < 0) heading += 360.f;
    float best = rng.unit()*360.f;
    for (int i = 0; i < 4; ++i) {
      const float trial = rng.unit()*360.f;
      if (std::abs(heading-trial) < std::abs(heading-best)) best = trial;
    }
    return best;
  }

public:
  Builder(Random& rng, Shape shape, int depth) : rng(rng), shape(shape), depth(depth) {}

  Layout attempt(const std::vector<Spec>& specs, const std::vector<int>& order) {
    out = {};
    for (auto s : specs) out.rooms.push_back({s});
    out.order = order;
    int entrance = -1, exit = -1, shop = -1;
    std::vector<int> multi, single, path;
    for (int id : order) {
      const Spec s = specs[id];
      if (s.kind == Kind::Entrance) entrance = id;
      else if (s.kind == Kind::Exit) exit = id;
      else if (s.kind == Kind::Shop && s.total == 1) shop = id;
      else if (s.total > 1) multi.push_back(id);
      else if (s.total == 1) single.push_back(id);
    }
    if (entrance < 0) return {};
    weight(multi); rng.shuffle(multi);
    std::vector<int> unique;
    for (int id : multi) if (!contains(unique,id)) unique.push_back(id);
    multi.swap(unique); rng.shuffle(multi);
    for (int n = int(multi.size())/4 + rng.chances(std::array<int,4>{0,0,0,1}); n > 0 && !multi.empty();) {
      const int id = multi.front(); multi.erase(multi.begin());
      n -= specs[id].factor;
      path.push_back(id);
    }
    std::array<int,3> path_deck{2,2,1};
    std::vector<int> first, second;
    auto init_root = [&](int root) {
      auto& r = out.rooms[root];
      r.box.r = rng.normal(r.spec.min_w,r.spec.max_w)-1;
      r.box.b = rng.normal(r.spec.min_h,r.spec.max_h)-1;
    };
    if (!shape.eight) {
      init_root(entrance);
      const float start = rng.unit()*360.f;
      path.insert(path.begin(),entrance);
      if (exit >= 0) path.insert(path.begin()+(path.size()+1)/2,exit);
      first = pad_loop(path,path_deck);
      if (!lay_loop(first,entrance,start)) return {};
    } else {
      if (landmark < 0) {
        for (int id : path) if (specs[id].total >= 4 && (landmark < 0 ||
            specs[id].min_w*specs[id].min_h > specs[landmark].min_w*specs[landmark].min_h)) landmark = id;
        if (!multi.empty()) { path.push_back(multi.front()); multi.erase(multi.begin()); }
      }
      if (landmark < 0) return {};
      erase(path,landmark); erase(multi,landmark);
      const float start = rng.unit()*360.f;
      const size_t n = path.size()/2 + (path.size()%2 ? rng.index(2) : 0);
      std::vector<int> a{landmark}, b{landmark};
      a.insert(a.end(),path.begin(),path.begin()+n);
      b.insert(b.end(),path.begin()+n,path.end());
      a.insert(a.begin()+(a.size()+1)/2,entrance);
      if (exit >= 0) b.insert(b.begin()+(b.size()+1)/2,exit);
      first = pad_loop(a,path_deck); second = pad_loop(b,path_deck);
      init_root(landmark);
      if (!lay_loop(first,landmark,start) || !lay_loop(second,landmark,start+180.f)) return {};
    }
    if (shop >= 0) {
      bool ok = false;
      for (int i = 0; i < 11 && !ok; ++i) ok = place(shape.eight ? out.order : first,entrance,shop,rng.unit()*360.f);
      if (!ok) return {};
    }
    const Center c1 = center(first), c2 = second.empty() ? c1 : center(second);
    std::vector<int> branchable = first;
    if (!second.empty()) {
      branchable.erase(std::find(branchable.begin(),branchable.end(),landmark));
      branchable.insert(branchable.end(),second.begin(),second.end());
    }
    weight(branchable);
    multi.insert(multi.end(),single.begin(),single.end());
    constexpr std::array<int,3> branch_defaults{1,1,0};
    auto branch_deck = branch_defaults;
    int failed = 0;
    for (int id : multi) {
      const bool secret = specs[id].kind == Kind::Secret;
      for (;;) {
        if (failed > 100) return {};
        int from;
        do { from = branchable[rng.index(int(branchable.size()))]; }
        while (secret && out.rooms[from].spec.kind == Kind::Connector);
        const size_t before = out.rooms.size();
        std::vector<int> tunnels;
        auto try_place = [&](int target, int tries) {
          const Center c = contains(first,from) ? c1 : c2;
          for (int j = 0; j < tries; ++j)
            if (place(out.order,from,target,branch_angle(from,c))) return true;
          return false;
        };
        bool ok = true;
        for (int n = draw(branch_deck,branch_defaults); n > 0; --n) {
          int t = corridor(secret);
          if (!try_place(t,3)) { clear(t); ok = false; break; }
          tunnels.push_back(t); out.order.push_back(t); from = t;
        }
        if (ok && try_place(id,10)) {
          for (int t : tunnels) if (rng.index(3) <= 1) branchable.push_back(t);
          if (specs[id].total > 1 && rng.index(3) == 0)
            for (int j = 0; j < specs[id].factor*specs[id].factor; ++j) branchable.push_back(id);
          break;
        }
        if (ok) clear(id);
        for (int t : tunnels) { clear(t); out.order.pop_back(); }
        out.rooms.resize(before, Room{{Kind::Connector,3,10,3,10}});
        ++failed;
      }
    }
    for (size_t i = 0; i < out.order.size(); ++i)
      for (size_t j = i+1; j < out.order.size(); ++j) {
        const int a = out.order[i], b = out.order[j];
        if (side_of(out.rooms[a].box,out.rooms[b].box) >= 0) add_neighbor(a,b);
      }
    for (int a : out.order)
      for (int b : out.rooms[a].neighbors)
        if (!contains(out.rooms[a].links,b) && rng.unit() < 0.30f) connect(a,b);
    return std::move(out);
  }
};

inline bool normalize(Layout& layout, int pad, int max_width, int max_height) {
  int l = 0, t = 0, r = 0, b = 0;
  for (const auto& room : layout.rooms) {
    const auto p = room.box;
    l = std::min(l,p.l); t = std::min(t,p.t); r = std::max(r,p.r); b = std::max(b,p.b);
  }
  layout.width = r-l+1+2*pad; layout.height = b-t+1+2*pad;
  if (layout.width > max_width || layout.height > max_height) return false;
  for (auto& room : layout.rooms) room.box.shift(pad-l,pad-t);
  return true;
}

} // namespace floor_layout
