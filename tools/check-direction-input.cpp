#include "direction_input.h"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main() {
  for(int frame_ms : {8,16,33}) {
    direction_input::Axis axis;
    int steps=axis.update(1,1000);
    for(int t=frame_ms;t<300;t+=frame_ms) steps+=axis.update(1,1000+t);
    assert(steps==1);
    assert(axis.update(1,1350)==1);
    assert(axis.update(1,1449)==0);
    assert(axis.update(1,1450)==1);
    assert(axis.update(1,9000)==1); // never catch up with multiple events
    assert(axis.update(0,9001)==0);
    assert(axis.update(1,9002)==1);
    assert(axis.update(-1,9003)==-1);
  }
  direction_input::Axis x,y;
  assert(x.update(1,0)==1);
  for(int t=1;t<300;++t) {
    y.update(t%2, t);
    assert(x.update(1,t)==0);
  }
  int held=0,presses=0;
  for(int value : {0,45,59,65,58,40,55,61,35,31,0}) {
    int next=direction_input::analog_axis(value,held);
    if(next && next!=held) ++presses;
    held=next;
  }
  assert(presses==1 && held==0);
  assert(direction_input::analog_axis(-65,0)==-1);
  assert(direction_input::analog_axis(-35,-1)==-1);
  assert(direction_input::analog_axis(-25,-1)==0);
  puts("Direction input passed: short taps, timed repeat, independent axes, frame stalls and analog hysteresis.");
}
