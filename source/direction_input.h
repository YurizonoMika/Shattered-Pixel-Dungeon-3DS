#pragma once
#include <cstdint>

namespace direction_input {
// Separate press delay from sustained repeat, measured in real time.
// Each axis owns its timer so diagonal jitter cannot retrigger the other axis.
struct Axis {
  int held=0;
  uint64_t next=0;
  int update(int direction,uint64_t now) {
    if(direction!=held) {
      held=direction;next=now+350;
      return direction;
    }
    if(direction && now>=next) {
      next=now+100;
      return direction;
    }
    return 0;
  }
};
inline int analog_axis(int value,int previous) {
  // Hysteresis prevents repeated presses near the activation threshold.
  if(value>60) return 1;
  if(value<-60) return -1;
  if(previous>0 && value>30) return 1;
  if(previous<0 && value<-30) return -1;
  return 0;
}
}
