#include "globaleffects.h"

#include "world/world.h"

GlobalEffects::SlowTime::SlowTime(uint64_t mul, uint64_t div)
  :mul(mul),div(div) {
  }

void GlobalEffects::SlowTime::scaleTime(GlobalEffects& owner, uint64_t& dt) {
  dt                = dt*mul + owner.timeWrldRem;
  owner.timeWrldRem = dt%div;
  dt /=div;
  }


GlobalEffects::GlobalEffects(World& owner):owner(owner){
  }

void GlobalEffects::tick(uint64_t /*dt*/) {
  uint64_t time = owner.tickCount();
  for(size_t i=0; i<eff.size(); ) {
    if(eff[i]->timeUntil<time) {
      eff.erase(eff.begin()+int(i));
      } else {
      ++i;
      }
    }
  }

void GlobalEffects::scaleTime(uint64_t& dt) {
  for(auto& i:eff)
    i->scaleTime(*this,dt);
  }

void GlobalEffects::start(const Daedalus::ZString& what, uint64_t len, const Daedalus::ZString* argv, size_t argc) {
  if(what=="time.slw")
    addSlowTime(len,argv,argc);
  }

void GlobalEffects::addSlowTime(uint64_t len, const Daedalus::ZString* argv, size_t argc) {
  double val[2] = {1,1};
  for(size_t i=0; i<argc && i<2; ++i)
    val[i] = std::atof(argv->c_str());
  uint64_t v[2] = {};
  for(int i=0; i<2; ++i)
    v[i] = uint64_t(val[i]*1000);
  // todo: separated time-scale
  eff.emplace_back(new SlowTime(v[0],1000));
  eff.back()->timeUntil = owner.tickCount()+len;
  }

