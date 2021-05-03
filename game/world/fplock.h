#pragma once

class WayPoint;
class Serialize;

class FpLock final {
  public:
    FpLock();
    FpLock(const WayPoint& p);
    FpLock(const WayPoint* p);
    FpLock(FpLock&& other);
    ~FpLock();

    FpLock& operator=(FpLock&& other);

    void load(Serialize& fin);
    void save(Serialize& fout) const;

  private:
    const WayPoint* pt=nullptr;
  };
