#include "floor_rooms.h"
#include <cstdio>

using namespace floor_layout;

int main() {
  constexpr const char* names[] = {
    "SewerPipe", "Ring", "WaterBridge", "RegionDecoPatch", "CircleBasin",
    "RegionDecoLine", "Segmented", "Pillars", "ChasmBridge", "CellBlock",
    "Cave", "RegionDecoBridge", "CavesFissure", "CirclePit", "CircleWall",
    "Hallway", "LibraryHall", "LibraryRing", "Statues", "SegmentedLibrary",
    "Ruins", "Chasm", "Skulls", "Ritual", "Plants", "Aquarium", "Platform", "Burned",
    "Fissure", "GrassyGrave", "Striped", "Study", "SuspiciousChest", "Minefield"
  };
  for (int i = 0; i < 2000; ++i) {
    const int depth = 1 + i % 20 + i % 20 / 4;
    const bool large = (i / 20) % 2 != 0;
    Random rng(0xb13fcacf41234561ULL * (i + 1));
    const auto rooms = standard_room_inputs(rng, depth, large);
    std::printf("%d %d %s %zu %016llx\n", i, depth, large ? "true" : "false", rooms.size(),
                (unsigned long long)rng.next_long());
    for (const auto& r : rooms) {
      const auto s = r.spec;
      std::printf("%s %d %d %d %d %d %d %d", names[int(r.type)], s.factor,
                  s.min_w, s.max_w, s.min_h, s.max_h, s.total, s.side);
      for (int j = 0; j < 3; ++j) {
        const int w = room_dimension(rng.normal(s.min_w,s.max_w),s.min_w+j,s.parity);
        const int h = room_dimension(rng.normal(s.min_h,s.max_h),std::max(s.min_h,s.max_h-j),s.parity);
        std::printf(" %dx%d:",w,h);
        const Room a{s,{0,0,w-1,h-1}}, b{{Kind::Connector,3,10,3,10},{-1,-3,w,0}};
        const auto [lo,hi] = door_span(a,b,1);
        for (int x = 0; x < w; ++x) std::putchar(x >= lo && x <= hi ? '1' : '0');
      }
      std::puts("");
    }
    std::printf("%016llx\n", (unsigned long long)rng.next_long());
  }
}
