#pragma once

class WayPoint;

class FpLock final {
  public:
    FpLock();
    FpLock(const WayPoint& p);
    FpLock(const WayPoint* p);
    FpLock(FpLock&& other);
    ~FpLock();

    FpLock& operator=(FpLock&& other);

  private:
    const WayPoint* pt=nullptr;
  };
