#include "fplock.h"
#include "waypoint.h"

#include "game/serialize.h"

FpLock::FpLock() {
  }

FpLock::FpLock(const WayPoint &p)
  :pt(&p){
  pt->useCount++;
  }

FpLock::FpLock(const WayPoint *p)
  :pt(p){
  if(pt)
    pt->useCount++;
  }

FpLock::FpLock(FpLock &&other)
  :pt(other.pt){
  other.pt=nullptr;
  }

FpLock::~FpLock() {
  if(pt)
    pt->useCount--;
  }

FpLock &FpLock::operator=(FpLock &&other) {
  if(pt)
    pt->useCount--;
  pt = other.pt;
  other.pt=nullptr;
  return *this;
  }

void FpLock::load(Serialize &fin) {
  fin.read(pt);
  if(pt)
    pt->useCount++;
  }

void FpLock::save(Serialize &fout) const {
  fout.write(pt);
  }
