#pragma once

// Default use/experience identification, SPD v4.0.2. GPL-3.0-or-later.
// Copyright (C) 2014-2026 Evan Debenham.
#include <algorithm>
struct Identification {
  float used=0.f,available=10.f;
  void use(float amount=1.f) {const float n=std::min(available,amount);used+=n;available-=n;}
  void experience(float fraction,float total,float gain=1.f) {
    available=std::min(total/2.f,available+fraction*total*gain);
  }
};
