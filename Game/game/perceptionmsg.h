#pragma once

#include <cstdint>

class Npc;

class PerceptionMsg final {
  public:
    int32_t what=0;
    float   x=0,y=0,z=0;
    Npc*    self  =nullptr;
    Npc*    other =nullptr;
    Npc*    victum=nullptr;
  };
