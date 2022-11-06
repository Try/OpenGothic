#include "globaleffects.h"

#include <charconv>

#include "world/objects/globalfx.h"
#include "world/world.h"
#include "graphics/visualfx.h"

GlobalEffects::GlobalEffects(World& owner):owner(owner){
  }

void GlobalEffects::tick(uint64_t dt) {
  tick(dt,timeEff);
  tick(dt,scrEff);
  tick(dt,morphEff);
  tick(dt,quakeEff);
  }

template<class T>
void GlobalEffects::tick(uint64_t /*dt*/, std::vector<T>& eff) {
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
  for(auto& pi:timeEff) {
    auto& i = *pi;
    dt          = dt*i.mul + timeWrldRem;
    timeWrldRem = dt%i.div;
    dt /=i.div;
    }
  }

static float generator(uint64_t dt, uint64_t period) {
  float a0 = float((dt+period/2)%period)/float(period); //[0..1]
  a0 = a0*2.f-1.f;
  return a0;
  }

void GlobalEffects::morph(Tempest::Matrix4x4& proj) {
  for(auto& pi:morphEff) {
    auto& i = *pi;
    float x = i.amplitude*0.075f;
    float y = i.amplitude*0.075f;

    uint64_t time = owner.tickCount()-i.timeStart;
    // a0 = std::sin(float(2*a0*M_PI));

    float a0 = generator(time,5000);
    float a1 = generator(time,9000);
    a0 = std::sin(a0*float(M_PI));
    a1 = std::sin(a1*float(M_PI));

    proj.scale(1.f+a0*x,1.f-a0*y,1.f);
    proj.scale(1.f+a1*x,1.f+a1*y,1.f);
    }
  }

void GlobalEffects::scrBlend(Tempest::Painter& p, const Tempest::Rect& rect) {
  for(auto& pi:scrEff) {
    auto&    i  = *pi;
    auto     cl = i.cl;
    if(i.inout>0) {
      uint64_t now = owner.tickCount();
      float    a0  = std::min(1.f, (float(now-i.timeStart)/1000.f)/i.inout);
      float    a1  = std::min(1.f, (float(i.timeUntil-now)/1000.f)/i.inout);
      float    a   = std::min(a0,a1);
      cl.set(cl.r(),cl.g(),cl.b(),cl.a()*a);
      }
    if(i.frames.size()==0) {
      p.setBrush(Tempest::Brush(cl,Tempest::Painter::Alpha));
      } else {
      uint64_t dt = owner.tickCount()-i.timeStart;
      size_t   frame = size_t(i.fps*dt)/1000;
      if(i.fps>0)
        frame%=i.fps;
      p.setBrush(Tempest::Brush(*i.frames[frame],Tempest::Color(1,1,1,cl.a()),Tempest::Painter::Alpha));
      }
    p.drawRect(rect.x,rect.y,rect.w,rect.h,0,0,p.brush().w(),p.brush().h());
    }
  }

GlobalFx GlobalEffects::startEffect(std::string_view what, uint64_t len, const std::string* argv, size_t argc) {
  auto     ret = create(what,argv,argc);
  auto&    eff = *ret.h;
  if(len==0)
    eff.timeUntil = uint64_t(-1); else
    eff.timeUntil = owner.tickCount()+len;
  eff.timeStart = owner.tickCount();
  eff.timeLen   = len==0 ? uint64_t(-1) : len;
  return ret;
  }

void GlobalEffects::stopEffect(const VisualFx& vfx) {
  auto& what = vfx.visName_S;
  if(what=="time.slw")
    timeEff.clear();
  if(what=="screenblend.scx")
    scrEff.clear();
  if(what=="morph.fov")
    morphEff.clear();
  if(what=="earthquake.eqk")
    quakeEff.clear();
  }

GlobalFx GlobalEffects::create(std::string_view what, const std::string* argv, size_t argc) {
  if(what=="time.slw")
    return addSlowTime(argv,argc);
  if(what=="screenblend.scx")
    return addScreenBlend(argv,argc);
  if(what=="morph.fov")
    return addMorphFov(argv,argc);
  if(what=="earthquake.eqk")
    return addEarthQuake(argv,argc);
  return GlobalFx();
  }

GlobalFx GlobalEffects::addSlowTime(const std::string* argv, size_t argc) {
  double val[2] = {1,1};
  for(size_t i=0; i<argc && i<2; ++i)
    val[i] = std::stof(argv[i]);
  uint64_t v[2] = {};
  for(int i=0; i<2; ++i)
    v[i] = uint64_t(val[i]*1000);
  // todo: separated time-scale
  SlowTime s;
  s.mul = v[0];
  s.div = 1000;
  timeEff.emplace_back(std::make_shared<SlowTime>(s));
  return GlobalFx(timeEff.back());
  }

GlobalFx GlobalEffects::addScreenBlend(const std::string* argv, size_t argc) {
  ScreenBlend sc;

  if(0<argc)
    sc.loop   = float(std::stof(argv[0]));
  if(1<argc)
    sc.cl     = parseColor(argv[1]);
  if(2<argc)
    sc.inout  = float(std::stof(argv[2]));
  if(3<argc)
    sc.frames = Resources::loadTextureAnim(argv[3]);
  if(4<argc)
    sc.fps    = size_t(std::stoi(argv[4]));

  sc.timeLoop = uint64_t(sc.loop*1000.f);

  scrEff.emplace_back(std::make_shared<ScreenBlend>(sc));
  return GlobalFx(scrEff.back());
  }

GlobalFx GlobalEffects::addMorphFov(const std::string* argv, size_t argc) {
  double val[4] = {0,0,0,0};
  for(size_t i=0; i<argc && i<4; ++i) {
    val[i] = std::stof(argv[i]);
    }
  Morph m;
  m.amplitude = float(val[0]);
  m.speed     = float(val[1]);
  m.baseX     = float(val[2]);
  m.baseY     = float(val[3]);

  morphEff.emplace_back(std::make_shared<Morph>(m));
  return GlobalFx(morphEff.back());
  }

GlobalFx GlobalEffects::addEarthQuake(const std::string*, size_t) {
  quakeEff.emplace_back(std::make_shared<Quake>());
  return GlobalFx(quakeEff.back());
  }

Tempest::Color GlobalEffects::parseColor(std::string_view sv) {
  auto  s    = std::string(sv);
  auto  str  = s.data();

  float v[4] = {};
  for(int i=0;i<4;++i) {
    char* next=nullptr;
    v[i] = std::strtof(str,&next)/255.f;
    if(str==next)
      if(i==1) {
        return Tempest::Color(v[0],v[0],v[0],1.f);
      if(i==2)
        return Tempest::Color(v[0],v[1],0.f,1.f);
      if(i==3)
        return Tempest::Color(v[0],v[1],v[2],1.f);
      }
    str = next;
    }
  return Tempest::Color(v[0],v[1],v[2],v[3]);
  }

void GlobalEffects::Effect::stop() {
  timeUntil = 0;
  }
