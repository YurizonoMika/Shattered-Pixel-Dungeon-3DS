#pragma once

// SPD v4.0.2 ShadowCaster. GPL-3.0-or-later.
// Copyright (C) 2012-2015 Oleg Dolya; (C) 2014-2026 Evan Debenham.
#include <algorithm>
#include <array>
#include <cmath>
namespace shadowcaster {
constexpr int max_distance=20;
inline const auto& rounding() {
  static const auto table=[] {
    std::array<std::array<int,max_distance+1>,max_distance+1> result{};
    for (int i=1; i<=max_distance; ++i) for (int j=1; j<=i; ++j)
      result[i][j]=std::min(j,int(std::floor(i*std::cos(std::asin(j/(i+.5)))+.5)));
    return result;
  }();
  return table;
}
template<class Blocking,class Visible>
void scan(int distance,Blocking blocking,Visible visible,int row,int x,int y,
          double left,double right,int mx,int my,bool swapped) {
  bool in_blocking=false;
  for (; row<=distance; ++row) {
    if (right<left) return;
    const int limit=distance==2 && row==2 ? 2 : rounding()[distance][row];
    const int start=left==0 ? 0 : int(std::floor((row-.5)*left+.499));
    const int end=right==1 ? limit : std::min(limit,int(std::ceil((row+.5)*right-.499)));
    for (int col=start; col<=end; ++col) {
      if (col==end && in_blocking && int(std::ceil((row-.5)*right-.499))!=end) break;
      const int cx=x+(swapped ? my*row : mx*col);
      const int cy=y+(swapped ? mx*col : my*row);
      visible(cx,cy);
      if (blocking(cx,cy)) {
        if (!in_blocking) {
          in_blocking=true;
          if (col!=start) scan(distance,blocking,visible,row+1,x,y,left,
                              (col-.5)/(row+.5),mx,my,swapped);
        }
      } else if (in_blocking) {
        in_blocking=false;
        left=(col-.5)/(row-.5);
      }
    }
    if (in_blocking) return;
  }
}
template<class Blocking,class Visible>
void cast(int x,int y,int distance,Blocking blocking,Visible visible) {
  distance=std::clamp(distance,0,max_distance);
  visible(x,y);
  scan(distance,blocking,visible,1,x,y,0,1,+1,-1,false);
  scan(distance,blocking,visible,1,x,y,0,1,-1,+1,true);
  scan(distance,blocking,visible,1,x,y,0,1,+1,+1,true);
  scan(distance,blocking,visible,1,x,y,0,1,+1,+1,false);
  scan(distance,blocking,visible,1,x,y,0,1,-1,+1,false);
  scan(distance,blocking,visible,1,x,y,0,1,+1,-1,true);
  scan(distance,blocking,visible,1,x,y,0,1,-1,-1,true);
  scan(distance,blocking,visible,1,x,y,0,1,-1,-1,false);
}
} // namespace shadowcaster
