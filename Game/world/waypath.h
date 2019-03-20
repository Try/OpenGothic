#pragma once

#include <vector>

class WayPoint;

class WayPath final {
  public:
    WayPath();

    void add(const WayPoint& p){ dat.push_back(&p); }
    void clear();

    const WayPoint* pop();

  private:
    std::vector<const WayPoint*> dat;
  };
