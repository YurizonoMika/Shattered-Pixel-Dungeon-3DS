#include "floor_painter.h"
#include <cstdio>

using namespace floor_layout;

int main() {
  for (int i = 0; i < 1800; ++i) {
    const int type = i%9, sample = i/9;
    const int cat = type == 8 ? 1+sample%2 : sample%3;
    using C = RoomClass;
    const C cls = type == 6 ? C::Pillars : type == 7 ? C::Segmented : C::CircleWall;
    const auto spec = room_spec(cls,Kind::Standard,room_rules(cls,Kind::Standard),cat);
    const int lo = type < 6 ? (type >= 4 ? 5 : 3) : spec.min_w;
    const int hi = type < 6 ? 10 : spec.max_w;
    const int w = lo+sample%(hi-lo+1), h = lo+sample/(hi-lo+1)%(hi-lo+1);
    const Rect p{2,3,w+1,h+2};
    std::vector<Point> doors;
    for (int j = 0; j < 2+sample%5; ++j) {
      const int side = (sample+j)%4;
      int x = 3+(sample*7+j*3)%(w-2), y = 4+(sample*3+j*5)%(h-2);
      if (side == 0) x = p.l;
      if (side == 1) y = p.t;
      if (side == 2) x = p.r;
      if (side == 3) y = p.b;
      doors.push_back({x,y});
    }
    Random rng(0x5e2fd32137bcee91ULL*(i+1));
    auto paint = type < 6 ? paint_connector(p,type,doors,rng) : paint_standard(p,cls,cat+1,doors,rng);
    std::printf("%d %dx%d ",i,w,h);
    for (const auto t : paint.cells) std::putchar('0'+int(t));
    std::printf(" %016llx\n",(unsigned long long)rng.next_long());
  }
}
