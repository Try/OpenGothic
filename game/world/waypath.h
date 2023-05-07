#pragma once

#include <vector>

class WayPoint;
class Serialize;

class WayPath final {
  public:
    WayPath();

    void load(Serialize& fin);
    void save(Serialize& fout);

    void add(const WayPoint& p){ dat.push_back(&p); }
    void clear();
    void reverse();

    const WayPoint* pop();
    const WayPoint* first() const;
    const WayPoint* last()  const;

  private:
    std::vector<const WayPoint*> dat;
  };
