#include "globaleffects.h"

#include "graphics/visualfx.h"
#include "world/world.h"

GlobalEffects::ScreenBlend::ScreenBlend(const char* tex) {
  }


GlobalEffects::GlobalEffects(World& owner):owner(owner){
  }

void GlobalEffects::tick(uint64_t dt) {
  tick(dt,timeEff);
  tick(dt,scrEff);
  tick(dt,morphEff);
  }

template<class T>
void GlobalEffects::tick(uint64_t /*dt*/, std::vector<T>& eff) {
  uint64_t time = owner.tickCount();
  for(size_t i=0; i<eff.size(); ) {
    if(eff[i].timeUntil<time) {
      eff.erase(eff.begin()+int(i));
      } else {
      ++i;
      }
    }
  }

void GlobalEffects::scaleTime(uint64_t& dt) {
  for(auto& i:timeEff) {
    dt          = dt*i.mul + timeWrldRem;
    timeWrldRem = dt%i.div;
    dt /=i.div;
    }
  }

void GlobalEffects::morph(Tempest::Matrix4x4& proj) {
  for(auto& i:morphEff) {
    float x = i.amplitude*0.05f;
    float y = i.amplitude*0.05f;

    float a = float(owner.tickCount()%2000)/2000.f;
    a = std::fabs(a*2.f-1.f);
    a = a*2.f-1.f;
    proj.scale(1.f+a*x,1.f-a*y,1.f);
    }
  }

void GlobalEffects::startEffect(const Daedalus::ZString& what, float lenF, const Daedalus::ZString* argv, size_t argc) {
  uint64_t len = uint64_t(lenF*1000.f);
  auto&    eff = create(what,argv,argc);
  if(lenF<0)
    eff.timeUntil = uint64_t(-1); else
    eff.timeUntil = owner.tickCount()+len;
  }

void GlobalEffects::stopEffect(const VisualFx& vfx) {
  auto& what = vfx.handle().visName_S;
  if(what=="time.slw")
    timeEff.clear();
  if(what=="screenblend.scx")
    scrEff.clear();
  if(what=="morph.fov")
    morphEff.clear();
  if(what=="earthquake.eqk")
    ;
  }

GlobalEffects::Effect& GlobalEffects::create(const Daedalus::ZString& what, const Daedalus::ZString* argv, size_t argc) {
  if(what=="time.slw")
    return addSlowTime(argv,argc);
  if(what=="screenblend.scx")
    return addScreenBlend(argv,argc);
  if(what=="morph.fov")
    return addMorphFov(argv,argc);
  if(what=="earthquake.eqk")
    ;
  static Effect eff;
  return eff;
  }

GlobalEffects::Effect& GlobalEffects::addSlowTime(const Daedalus::ZString* argv, size_t argc) {
  double val[2] = {1,1};
  for(size_t i=0; i<argc && i<2; ++i)
    val[i] = std::atof(argv[i].c_str());
  uint64_t v[2] = {};
  for(int i=0; i<2; ++i)
    v[i] = uint64_t(val[i]*1000);
  // todo: separated time-scale
  SlowTime s;
  s.mul = v[0];
  s.div = 1000;
  timeEff.emplace_back(s);
  return timeEff.back();
  }

GlobalEffects::Effect& GlobalEffects::addScreenBlend(const Daedalus::ZString* argv, size_t argc) {
  double val[4] = {0,0,0,0};
  for(size_t i=0; i<argc && i<4; ++i) {
    val[i] = std::atof(argv[i].c_str());
    }
  const char* tex = 3<argc ? argv[3].c_str() : nullptr;
  (void)val;
  scrEff.emplace_back(ScreenBlend(tex));
  return scrEff.back();
  }

GlobalEffects::Effect& GlobalEffects::addMorphFov(const Daedalus::ZString* argv, size_t argc) {
  double val[4] = {0,0,0,0};
  for(size_t i=0; i<argc && i<4; ++i) {
    val[i] = std::atof(argv[i].c_str());
    }
  Morph m;
  m.amplitude = float(val[0]);
  m.speed     = float(val[1]);
  m.baseX     = float(val[2]);
  m.baseY     = float(val[3]);

  morphEff.emplace_back(m);
  return morphEff.back();
  }
