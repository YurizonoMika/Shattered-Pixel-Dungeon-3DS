#pragma once

// SPD v4.0.2 Blob.evolve, including the evolving area bounds used during
// the scan. GPL-3.0-or-later. Copyright (C) 2012-2015 Oleg Dolya;
// Copyright (C) 2014-2026 Evan Debenham.
#include <algorithm>
#include <array>
namespace blob_rules {
template<int W,int H> struct Gas {
  std::array<int,W*H> cur{},off{};
  int left=0,top=0,right=0,bottom=0;
  int volume=0;
  void clear() { cur.fill(0);off.fill(0);left=top=right=bottom=volume=0; }
  void seed(int cell,int amount) {
    cur[cell]+=amount; volume+=amount;
    const int x=cell%W,y=cell/W;
    if (right<=left || bottom<=top) { left=x;right=x+1;top=y;bottom=y+1; }
    else { left=std::min(left,x);right=std::max(right,x+1);top=std::min(top,y);bottom=std::max(bottom,y+1); }
  }
  void setup_area() {
    left=top=right=bottom=0;
    for (int cell=0;cell<W*H;++cell) if (cur[cell]) {
      const int x=cell%W,y=cell/W;
      if (right<=left || bottom<=top) { left=x;right=x+1;top=y;bottom=y+1; }
      else { left=std::min(left,x);right=std::max(right,x+1);top=std::min(top,y);bottom=std::max(bottom,y+1); }
    }
  }
  template<class Inside,class Solid,class Exposed>
  void act(Inside inside,Solid solid,Exposed exposed) {
    if (!volume) {
      if (right>left && bottom>top) { left=top=right=bottom=0; off=cur; }
      return;
    }
    if (right<=left || bottom<=top) setup_area();
    volume=0;
    for (int y=top-1;y<=bottom;++y) for (int x=left-1;x<=right;++x) {
      const int cell=y*W+x;
      if (!inside(x,y)) continue;
      if (solid(cell)) { off[cell]=0; continue; }
      int count=1,sum=cur[cell];
      if (x>left && !solid(cell-1)) { sum+=cur[cell-1];++count; }
      if (x<right && !solid(cell+1)) { sum+=cur[cell+1];++count; }
      if (y>top && !solid(cell-W)) { sum+=cur[cell-W];++count; }
      if (y<bottom && !solid(cell+W)) { sum+=cur[cell+W];++count; }
      const int value=sum>=count ? sum/count-1 : 0;
      off[cell]=value;
      if (value>0) {
        if (y<top) top=y; else if (y>=bottom) bottom=y+1;
        if (x<left) left=x; else if (x>=right) right=x+1;
      }
      volume+=value;
    }
    // ToxicGas damages occupants of cur after super.evolve(), before swap.
    for (int x=left;x<right;++x) for (int y=top;y<bottom;++y)
      if (cur[y*W+x]>0) exposed(y*W+x);
    cur.swap(off);
  }
};
} // namespace blob_rules
