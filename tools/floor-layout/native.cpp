#include "floor_patch.h"
#include "floor_maze.h"
#include <cstdio>

using namespace floor_layout;

int main() {
  for (int i = 0; i < 1000; ++i) {
    int depth = 1 + i % 20 + i % 20 / 4;
    auto rng = floor_random(0x89abcdef01234567ULL * (i + 1), depth);
    auto shape = select_shape(rng);
    std::vector<Spec> specs{{Kind::Entrance,5,10,5,10}, {Kind::Exit,5,10,5,10}};
    for (int j = 0; j < 4 + i % 8; ++j) {
      int factor = j == 0 ? 2 : j == 1 && i % 5 == 0 ? 3 : 1;
      int lo = factor == 1 ? 4 : factor == 2 ? 10 : 14;
      int hi = factor == 1 ? 10 : factor == 2 ? 14 : 18;
      specs.push_back({Kind::Standard,lo,hi,lo,hi,factor});
    }
    if (i % 3 == 0) specs.push_back({Kind::Shop,7,10,7,10,1,1,1});
    specs.push_back({Kind::Leaf,5,10,5,10,1,1,1});
    specs.push_back({Kind::Secret,5,10,5,10,1,1,1});
    std::vector<int> order;
    for (int j = 0; j < int(specs.size()); ++j) order.push_back(j);
    rng.shuffle(order);
    Builder builder(rng,shape,depth);
    Layout result;
    int attempt;
    for (attempt = 1; attempt <= 256; ++attempt) {
      result = builder.attempt(specs,order);
      if (!result.rooms.empty()) break;
    }
    if (result.rooms.empty()) { std::fprintf(stderr,"exhausted attempts for %d\n",i); return 1; }
    std::vector<int> ids(result.rooms.size());
    for (int j = 0; j < int(ids.size()); ++j) ids[result.order[j]] = j < int(specs.size()) ? order[j] : j;
    std::printf("%d %d %zu %016llx\n",i,attempt,result.rooms.size(),(unsigned long long)rng.next_long());
    for (auto id : result.order) {
      const auto& r = result.rooms[id];
      std::printf("%d %d %d %d %d %d:",ids[id],r.type,r.box.l,r.box.t,r.box.r,r.box.b);
      for (int n : r.links) std::printf(" %d",ids[n]);
      std::puts("");
    }
  }
  for (int i = 0; i < 500; ++i) {
    Random rng(0x713abc0012345678ULL * (i + 1));
    const auto cells = patch(2+i%63, 2+(i*17)%63, (i%21)/20.f, i%7, i%3 != 0, rng);
    for (auto c : cells) std::putchar(c ? '1' : '0');
    std::printf(" %016llx\n", (unsigned long long)rng.next_long());
  }
  for (int i = 0; i < 100; ++i) {
    Random rng(0x0123456789abcd2fULL * (i + 1));
    const int w = 3+i%8, h = 3+(i*5)%8;
    const auto cells = maze({0,0,w-1,h-1},{{0,1+i%(h-2)},{w-1,1+(i*3)%(h-2)}},rng);
    for (auto c : cells) std::putchar(c ? '1' : '0');
    std::printf(" %016llx\n", (unsigned long long)rng.next_long());
  }
}
