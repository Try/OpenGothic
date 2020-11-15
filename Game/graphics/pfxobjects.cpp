#include "pfxobjects.h"
#include "sceneglobals.h"

#include <cstring>
#include <cassert>

#include "graphics/mesh/submesh/pfxemittermesh.h"
#include "graphics/mesh/pose.h"
#include "graphics/mesh/skeleton.h"
#include "graphics/dynamic/painter3d.h"
#include "world/world.h"
#include "particlefx.h"
#include "lightsource.h"
#include "rendererstorage.h"

using namespace Tempest;

std::mt19937 PfxObjects::rndEngine;

PfxObjects::Emitter::Emitter(PfxObjects::Bucket& b, size_t id)
  :bucket(&b), id(id) {
  }

PfxObjects::Emitter::~Emitter() {
  if(bucket!=nullptr) {
    std::lock_guard<std::recursive_mutex> guard(bucket->parent->sync);
    auto& p  = bucket->impl[id];
    p.alive  = false;
    p.active = false;
    p.next.reset();
    }
  }

PfxObjects::Emitter::Emitter(PfxObjects::Emitter && b)
  :bucket(b.bucket), id(b.id) {
  b.bucket = nullptr;
  }

PfxObjects::Emitter &PfxObjects::Emitter::operator=(PfxObjects::Emitter &&b) {
  std::swap(bucket,b.bucket);
  std::swap(id,b.id);
  return *this;
  }

void PfxObjects::Emitter::setPosition(float x, float y, float z) {
  setPosition(Vec3(x,y,z));
  }

void PfxObjects::Emitter::setPosition(const Vec3& pos) {
  if(bucket==nullptr)
    return;
  std::lock_guard<std::recursive_mutex> guard(bucket->parent->sync);
  auto& v = bucket->impl[id];
  v.pos = pos;
  if(v.next!=nullptr)
    v.next->setPosition(pos);
  if(bucket->impl[id].block==size_t(-1))
    return; // no backup memory
  auto& p = bucket->getBlock(*this);
  p.pos = pos;
  }

void PfxObjects::Emitter::setTarget(const Vec3& pos) {
  if(bucket==nullptr)
    return;
  std::lock_guard<std::recursive_mutex> guard(bucket->parent->sync);
  if(bucket->impl[id].block==size_t(-1))
    return; // no backup memory
  auto& p     = bucket->getBlock(*this);
  p.target    = pos;
  p.hasTarget = true;
  }

void PfxObjects::Emitter::setDirection(const Matrix4x4& d) {
  if(bucket==nullptr)
    return;
  std::lock_guard<std::recursive_mutex> guard(bucket->parent->sync);
  auto& v = bucket->impl[id];
  v.direction[0] = Vec3(d.at(0,0),d.at(0,1),d.at(0,2));
  v.direction[1] = Vec3(d.at(1,0),d.at(1,1),d.at(1,2));
  v.direction[2] = Vec3(d.at(2,0),d.at(2,1),d.at(2,2));

  if(v.next!=nullptr)
    v.next->setDirection(d);

  if(bucket->impl[id].block==size_t(-1))
    return; // no backup memory
  auto& p = bucket->getBlock(*this);
  p.direction[0] = v.direction[0];
  p.direction[1] = v.direction[1];
  p.direction[2] = v.direction[2];
  }

void PfxObjects::Emitter::setActive(bool act) {
  if(bucket==nullptr)
    return;
  std::lock_guard<std::recursive_mutex> guard(bucket->parent->sync);
  auto& v = bucket->impl[id];
  if(act==v.active)
    return;
  v.active = act;
  if(v.next!=nullptr)
    v.next->setActive(act);
  if(act==true)
    v.waitforNext = bucket->owner->ppsCreateEmDelay;
  }

bool PfxObjects::Emitter::isActive() const {
  if(bucket==nullptr)
    return false;
  std::lock_guard<std::recursive_mutex> guard(bucket->parent->sync);
  return bucket->impl[id].active;
  }

uint64_t PfxObjects::Emitter::effectPrefferedTime() const {
  if(bucket==nullptr)
    return 0;
  return bucket->owner->effectPrefferedTime();
  }

void PfxObjects::Emitter::setObjMatrix(const Matrix4x4 &mt) {
  setPosition (mt.at(3,0),mt.at(3,1),mt.at(3,2));
  setDirection(mt);
  }

PfxObjects::Bucket::Bucket(const ParticleFx &ow, PfxObjects *parent)
  :owner(&ow), parent(parent) {
  Bounds bbox;
  bbox.assign(Vec3(0,0,0),1000000); //TODO

  const Tempest::VertexBuffer<Resources::Vertex>* vbo[Resources::MaxFramesInFlight] = {};
  for(size_t i=0;i<Resources::MaxFramesInFlight;++i)
    vbo[i] = &vboGpu[i];
  item = parent->visual.get(vbo,owner->visMaterial,bbox);

  Matrix4x4 ident;
  ident.identity();
  item.setObjMatrix(ident);

  uint64_t lt      = ow.maxLifetime();
  uint64_t pps     = uint64_t(std::ceil(ow.maxPps()));
  uint64_t reserve = (lt*pps+1000-1)/1000;
  blockSize        = size_t(reserve);
  }

bool PfxObjects::Bucket::isEmpty() const {
  for(size_t i=0;i<Resources::MaxFramesInFlight;++i) {
    if(vboGpu[i].size()>0)
      return false;
    }
  return impl.size()==0;
  }

size_t PfxObjects::Bucket::allocBlock() {
  for(size_t i=0;i<block.size();++i) {
    if(!block[i].alive) {
      block[i].alive=true;
      return i;
      }
    }

  block.emplace_back();

  Block& b = block.back();
  b.offset = particles.size();

  particles.resize(particles.size()+blockSize);
  vboCpu.resize(particles.size()*6);

  for(size_t i=0; i<blockSize; ++i)
    particles[i].life = 0;
  return block.size()-1;
  }

void PfxObjects::Bucket::freeBlock(size_t& i) {
  if(i==size_t(-1))
    return;
  auto& b = block[i];
  assert(b.count==0);
  b.alive = false;
  b.alive = false;
  i = size_t(-1);
  }

PfxObjects::Block &PfxObjects::Bucket::getBlock(PfxObjects::ImplEmitter &e) {
  if(e.block==size_t(-1)) {
    e.block        = allocBlock();
    auto& p        = block[e.block];
    p.pos          = e.pos;
    p.direction[0] = e.direction[0];
    p.direction[1] = e.direction[1];
    p.direction[2] = e.direction[2];
    }
  return block[e.block];
  }

PfxObjects::Block &PfxObjects::Bucket::getBlock(PfxObjects::Emitter &e) {
  return getBlock(impl[e.id]);
  }

size_t PfxObjects::Bucket::allocEmitter() {
  for(size_t i=0; i<impl.size(); ++i) {
    auto& b = impl[i];
    if(!b.alive && b.block==size_t(-1)) {
      b.alive = true;
      return i;
      }
    }
  impl.emplace_back();
  auto& e = impl.back();
  e.block = size_t(-1); // no backup memory
  e.alive = true;

  return impl.size()-1;
  }

bool PfxObjects::Bucket::shrink() {
  while(impl.size()>0) {
    auto& b = impl.back();
    if(b.alive || b.block!=size_t(-1))
      break;
    impl.pop_back();
    }
  while(block.size()>0) {
    auto& b = block.back();
    if(b.alive || b.count>0)
      break;
    block.pop_back();
    }
  if(particles.size()!=block.size()*blockSize) {
    particles.resize(block.size()*blockSize);
    vboCpu.resize(particles.size()*6);
    return true;
    }
  return false;
  }

void PfxObjects::Bucket::init(PfxObjects::Block& emitter, size_t particle) {
  auto& p   = particles[particle];
  auto& pfx = *owner;

  p.life    = uint16_t(randf(pfx.lspPartAvg,pfx.lspPartVar));
  p.maxLife = p.life;

  // TODO: pfx.shpDistribType, pfx.shpDistribWalkSpeed;
  switch(pfx.shpType) {
    case ParticleFx::EmitterType::Point:{
      p.pos = Vec3();
      break;
      }
    case ParticleFx::EmitterType::Line:{
      float at = randf();
      p.pos = Vec3(at,at,at);
      break;
      }
    case ParticleFx::EmitterType::Box:{
      if(pfx.shpIsVolume) {
        p.pos = Vec3(randf()*2.f-1.f,
                     randf()*2.f-1.f,
                     randf()*2.f-1.f);
        p.pos*=0.5;
        } else {
        // TODO
        p.pos = Vec3(randf()*2.f-1.f,
                     randf()*2.f-1.f,
                     randf()*2.f-1.f);
        p.pos*=0.5;
        }
      break;
      }
    case ParticleFx::EmitterType::Sphere:{
      float theta = float(2.0*M_PI)*randf();
      float phi   = std::acos(1.f - 2.f * randf());
      p.pos = Vec3(std::sin(phi) * std::cos(theta),
                   std::sin(phi) * std::sin(theta),
                   std::cos(phi));
      if(pfx.shpIsVolume)
        p.pos*=randf();
      break;
      }
    case ParticleFx::EmitterType::Circle:{
      float a = float(2.0*M_PI)*randf();
      p.pos = Vec3(std::sin(a),
                   0,
                   std::cos(a));
      p.pos*=0.5;
      if(pfx.shpIsVolume)
        p.pos = p.pos*std::sqrt(randf());
      break;
      }
    case ParticleFx::EmitterType::Mesh:{
      p.pos = Vec3();
      if(pfx.shpMesh!=nullptr) {
        auto pos = pfx.shpMesh->randCoord(randf());
        p.pos = emitter.direction[0]*pos.x +
                emitter.direction[1]*pos.y +
                emitter.direction[2]*pos.z;
        }
      break;
      }
    }

  Vec3 dim = pfx.shpDim*pfx.shpScale(emitter.timeTotal);
  p.pos.x*=dim.x;
  p.pos.y*=dim.y;
  p.pos.z*=dim.z;

  switch(pfx.shpFOR) {
    case ParticleFx::Frame::Object: {
      p.pos += emitter.direction[0]*pfx.shpOffsetVec.x +
               emitter.direction[1]*pfx.shpOffsetVec.y +
               emitter.direction[2]*pfx.shpOffsetVec.z;
      break;
      }
    case ParticleFx::Frame::World: {
      p.pos += pfx.shpOffsetVec;
      break;
      }
    }

  p.velocity = randf(pfx.velAvg,pfx.velVar);

  float dirRotation = 0;
  switch(pfx.dirMode) {
    case ParticleFx::Dir::Rand: {
      float dy    = 1.f - 2.f * randf();
      float sn    = std::sqrt(1-dy*dy);
      float theta = float(2.0*M_PI)*randf();
      float dx    = sn * std::cos(theta);
      float dz    = sn * std::sin(theta);

      dirRotation = std::atan2(dy,(dx>0 ? sn : -sn));
      p.dir = Vec3(dx,dy,dz);
      break;
      }
    case ParticleFx::Dir::Dir: {
      float theta = (90+randf(pfx.dirAngleHead,pfx.dirAngleHeadVar))*float(M_PI)/180.f;
      float phi   = (   randf(pfx.dirAngleElev,pfx.dirAngleElevVar))*float(M_PI)/180.f;

      float dx = std::cos(phi) * std::cos(theta);
      float dy = std::sin(phi);
      float dz = std::cos(phi) * std::sin(theta);

      if(pfx.dirModeTargetFOR==ParticleFx::Frame::Object &&
         pfx.shpType         ==ParticleFx::EmitterType::Sphere &&
         pfx.dirAngleHeadVar>=180 &&
         pfx.dirAngleElevVar>=180 ) {
        dx = p.pos.x;
        dy = p.pos.y;
        dz = p.pos.z;
        }

      switch(pfx.dirFOR) {
        case ParticleFx::Frame::Object: {
          p.dir = emitter.direction[0]*dx +
                  emitter.direction[1]*dy +
                  emitter.direction[2]*dz;
          float l = p.dir.manhattanLength();
          if(l>0)
            p.dir/=l;
          break;
          }
        case ParticleFx::Frame::World: {
          p.dir = Vec3(dx,dy,dz);
          break;
          }
        }
      dirRotation = std::atan2(p.dir.x,p.dir.y);
      break;
      }
    case ParticleFx::Dir::Target:
      dirRotation = randf()*float(2.0*M_PI);
      break;
    }

  switch(pfx.visOrientation){
    case ParticleFx::Orientation::None:
      p.rotation = randf()*float(2.0*M_PI);
      break;
    case ParticleFx::Orientation::Velocity:
      p.rotation = dirRotation;
      break;
    case ParticleFx::Orientation::Velocity3d:
      p.rotation = dirRotation;
      break;
    }

  if(pfx.useEmittersFOR==0)
    p.pos += emitter.pos;
  }

void PfxObjects::Bucket::finalize(size_t particle) {
  Vertex* v = &vboCpu[particle*6];
  std::memset(v,0,sizeof(*v)*6);
  auto& p = particles[particle];
  p = {};
  }

void PfxObjects::Bucket::tick(Block& sys, size_t particle, uint64_t dt) {
  ParState& ps = particles[particle+sys.offset];
  if(ps.life==0)
    return;

  if(ps.life<=dt){
    ps.life = 0;
    sys.count--;
    finalize(particle+sys.offset);
    return;
    }

  auto&       pfx  = *owner;
  const float dtF  = float(dt);
  Vec3        dpos = {};

  switch(pfx.dirMode) {
    case ParticleFx::Dir::Dir:
      dpos = ps.dir*ps.velocity;
      break;
    case ParticleFx::Dir::Target: {
      Vec3 dx, to;
      if(sys.hasTarget)
        to = sys.target; else
        to = sys.pos;

      dx = to - (ps.pos+sys.pos);
      const float dplen = dx.manhattanLength();
      if(ps.velocity*dtF>dplen)
        dpos = dx/dtF; else
        dpos = dx*ps.velocity/dplen;
      break;
      }
    case ParticleFx::Dir::Rand:
      dpos = ps.dir*ps.velocity;
      break;
    }

  // eval particle
  ps.life  = uint16_t(ps.life-dt);
  ps.pos  += dpos*dtF;
  ps.dir  += pfx.flyGravity*dtF;
  }

float PfxObjects::ParState::lifeTime() const {
  return 1.f-life/float(maxLife);
  }


PfxObjects::PfxObjects(const SceneGlobals& scene, VisualObjects& visual)
  :scene(scene), visual(visual) {
  }

PfxObjects::~PfxObjects() {
  }

PfxObjects::Emitter PfxObjects::get(const ParticleFx &decl) {
  if(decl.visMaterial.tex==nullptr)
    return Emitter();
  std::lock_guard<std::recursive_mutex> guard(sync);
  auto&  b = getBucket(decl);
  size_t e = b.allocEmitter();
  return Emitter(b,e);
  }

PfxObjects::Emitter PfxObjects::get(const ZenLoad::zCVobData& vob) {
  Material mat(vob);

  std::lock_guard<std::recursive_mutex> guard(sync);
  auto&  b = getBucket(mat,vob);
  size_t e = b.allocEmitter();
  return Emitter(b,e);
  }

void PfxObjects::setViewerPos(const Vec3& pos) {
  viewePos = pos;
  }

void PfxObjects::resetTicks() {
  lastUpdate = size_t(-1);
  }

void PfxObjects::tick(uint64_t ticks) {
  static bool disabled = false;
  if(disabled)
    return;

  if(lastUpdate>ticks) {
    lastUpdate = ticks;
    return;
    }

  uint64_t dt = ticks-lastUpdate;
  if(dt==0)
    return;

  VboContext ctx;
  const auto& m = scene.viewProject();
  ctx.left.x = m.at(0,0);
  ctx.left.y = m.at(1,0);
  ctx.left.z = m.at(2,0);

  ctx.top.x  = m.at(0,1);
  ctx.top.y  = m.at(1,1);
  ctx.top.z  = m.at(2,1);

  ctx.z.x    = m.at(0,2);
  ctx.z.y    = m.at(1,2);
  ctx.z.z    = m.at(2,2);

  ctx.left/=ctx.left.manhattanLength();
  ctx.top /=ctx.top.manhattanLength();
  ctx.z   /=ctx.z.manhattanLength();

  ctx.leftA.x = ctx.left.x;
  ctx.leftA.z = ctx.left.z;
  ctx.topA.y  = -1;

  for(size_t i=0; i<bucket.size(); ++i)
    tickSys(*bucket[i],dt);

  for(auto& i:bucket)
    buildVbo(*i,ctx);

  lastUpdate = ticks;
  }

void PfxObjects::preFrameUpdate(uint8_t fId) {
  for(size_t i=0; i<bucket.size(); ) {
    if(bucket[i]->isEmpty()) {
      bucket[i] = std::move(bucket.back());
      bucket.pop_back();
      } else {
      ++i;
      }
    }

  auto& device = scene.storage.device;
  for(auto& pi:bucket) {
    auto& i=*pi;
    auto& vbo = i.vboGpu[fId];
    if(i.vboCpu.size()!=vbo.size())
      vbo = device.vboDyn(i.vboCpu); else
      vbo.update(i.vboCpu);
    }
  }

float PfxObjects::randf() {
  return float(rndEngine()%10000)/10000.f;
  }

float PfxObjects::randf(float base, float var) {
  return (2.f*randf()-1.f)*var + base;
  }

PfxObjects::Bucket &PfxObjects::getBucket(const ParticleFx &ow) {
  for(auto& i:bucket)
    if(i->owner==&ow)
      return *i;
  bucket.emplace_back(new Bucket(ow,this));
  return *bucket.back();
  }

PfxObjects::Bucket& PfxObjects::getBucket(const Material& mat, const ZenLoad::zCVobData& vob) {
  for(auto& i:spriteEmit)
    if(i.pfx->visMaterial==mat &&
       i.visualCamAlign==vob.visualCamAlign &&
       i.zBias==vob.zBias &&
       i.decalDim==vob.visualChunk.zCDecal.decalDim) {
      return getBucket(*i.pfx);
      }
  spriteEmit.emplace_back();
  auto& e = spriteEmit.back();

  e.visualCamAlign = vob.visualCamAlign;
  e.zBias          = vob.zBias;
  e.decalDim       = vob.visualChunk.zCDecal.decalDim;
  e.pfx.reset(new ParticleFx(mat,vob));
  return getBucket(*e.pfx);
  }

static uint64_t ppsDiff(const ParticleFx& decl, uint64_t time0, uint64_t time1) {
  if(time1<=time0)
    return 0;
  const float pps     = decl.ppsScale(time1)*decl.ppsValue;
  uint64_t    emitted0 = uint64_t(pps*float(time0)/1000.f);
  uint64_t    emitted1 = uint64_t(pps*float(time1)/1000.f);
  return emitted1-emitted0;
  }

void PfxObjects::tickSys(PfxObjects::Bucket &b, uint64_t dt) {
  bool doShrink = false;
  for(auto& emitter:b.impl) {
    const auto dp      = emitter.pos-viewePos;
    const bool active  = emitter.active;
    const bool nearby  = (dp.quadLength()<4000*4000);
    const bool process = active && nearby;

    if(emitter.next==nullptr && b.owner->ppsCreateEm!=nullptr && emitter.waitforNext<dt && emitter.active) {
      emitter.next.reset(new Emitter(get(*b.owner->ppsCreateEm)));
      auto& e = *emitter.next;
      e.setPosition(emitter.pos.x,emitter.pos.y,emitter.pos.z);
      e.setActive(true);
      }

    if(emitter.waitforNext>=dt)
      emitter.waitforNext-=dt;

    if(!process && emitter.block==size_t(-1)) {
      continue;
      }

    auto& p = b.getBlock(emitter);

    if(p.count>0) {
      for(size_t i=0;i<b.blockSize;++i)
        b.tick(p,i,dt);
      if(p.count==0 && !process) {
        // free mem
        b.freeBlock(emitter.block);
        doShrink = true;
        continue;
        }
      }

    if(b.owner->ppsValue<0){
      tickSysEmit(b,p,p.count==0 ? 1 : 0);
      }
    else if(active && nearby) {
      auto dE = ppsDiff(*b.owner,p.timeTotal,p.timeTotal+dt);
      tickSysEmit(b,p,dE);
      }
    p.timeTotal+=dt;
    }

  if(doShrink)
    b.shrink();
  }

void PfxObjects::tickSysEmit(PfxObjects::Bucket& b, PfxObjects::Block& p, uint64_t emited) {
  size_t lastI = 0;
  for(size_t id=1; emited>0; ++id) {
    const size_t i = id%b.blockSize;
    ParState& ps = b.particles[i+p.offset];
    if(ps.life==0) { // free slot
      lastI = i;
      p.count++;
      b.init(p,i+p.offset);
      --emited;
      } else {
      // out of slots
      if(lastI==i)
        return;
      }
    }
  }

static void rotate(Vec3& rx, Vec3& ry,float a,const Vec3& x, const Vec3& y){
  const float c = std::cos(a);
  const float s = std::sin(a);

  rx.x = x.x*c - y.x*s;
  rx.y = x.y*c - y.y*s;
  rx.z = x.z*c - y.z*s;

  ry.x = x.x*s + y.x*c;
  ry.y = x.y*s + y.y*c;
  ry.z = x.z*s + y.z*c;
  }

void PfxObjects::buildVbo(PfxObjects::Bucket &b, const VboContext& ctx) {
  static const float dx[6] = {-0.5f, 0.5f, -0.5f,  0.5f,  0.5f, -0.5f};
  static const float dy[6] = { 0.5f, 0.5f, -0.5f,  0.5f, -0.5f, -0.5f};

  auto& pfx             = *b.owner;
  auto  colorS          = pfx.visTexColorStart;
  auto  colorE          = pfx.visTexColorEnd;
  auto  visSizeStart    = pfx.visSizeStart;
  auto  visSizeEndScale = pfx.visSizeEndScale;
  auto  visAlphaStart   = pfx.visAlphaStart;
  auto  visAlphaEnd     = pfx.visAlphaEnd;
  auto  visAlphaFunc    = pfx.visMaterial.alpha;

  const Vec3& left = pfx.visYawAlign ? ctx.leftA : ctx.left;
  const Vec3& top  = pfx.visYawAlign ? ctx.topA  : ctx.top;

  for(auto& p:b.block) {
    if(p.count==0)
      continue;

    for(size_t i=0;i<b.blockSize;++i) {
      ParState& ps = b.particles[i+p.offset];
      Vertex*   v  = &b.vboCpu[(p.offset+i)*6];

      if(ps.life==0) {
        std::memset(v,0,6*sizeof(*v));
        continue;
        }

      const float a   = ps.lifeTime();
      const Vec3  cl  = colorS*(1.f-a)        + colorE*a;
      const float clA = visAlphaStart*(1.f-a) + visAlphaEnd*a;

      const float scale = 1.f*(1.f-a) + a*visSizeEndScale;
      const float szX   = visSizeStart.x*scale;
      const float szY   = visSizeStart.y*scale;
      const float szZ   = 0.1f*((szX+szY)*0.5f);

      Vec3 l={};
      Vec3 t={};

      if(pfx.visOrientation==ParticleFx::Orientation::Velocity3d) {
        static float k1 = 1, k2 = -1;
        t = ps.dir*k1;
        l = Vec3::crossProduct(t,ctx.z)*k2;
        } else {
        rotate(l,t,ps.rotation,left,top);
        }

      struct Color {
        uint8_t r=255;
        uint8_t g=255;
        uint8_t b=255;
        uint8_t a=255;
        } color;

      if(visAlphaFunc==Material::AlphaFunc::AdditiveLight) {
        color.r = uint8_t(cl.x*clA);
        color.g = uint8_t(cl.y*clA);
        color.b = uint8_t(cl.z*clA);
        color.a = uint8_t(255);
        }
      else if(visAlphaFunc==Material::AlphaFunc::Transparent) {
        color.r = uint8_t(cl.x);
        color.g = uint8_t(cl.y);
        color.b = uint8_t(cl.z);
        color.a = uint8_t(clA*255);
        }

      for(int i=0;i<6;++i) {
        float sx = l.x*dx[i]*szX + t.x*dy[i]*szY;
        float sy = l.y*dx[i]*szX + t.y*dy[i]*szY;
        float sz = l.z*dx[i]*szX + t.z*dy[i]*szY;

        if(b.owner->useEmittersFOR) {
          v[i].pos[0] = p.pos.x + ps.pos.x + sx;
          v[i].pos[1] = p.pos.y + ps.pos.y + sy;
          v[i].pos[2] = p.pos.z + ps.pos.z + sz;
          } else {
          v[i].pos[0] = ps.pos.x + sx;
          v[i].pos[1] = ps.pos.y + sy;
          v[i].pos[2] = ps.pos.z + sz;
          }

        if(pfx.visZBias) {
          v[i].pos[0] -= szZ*ctx.z.x;
          v[i].pos[1] -= szZ*ctx.z.y;
          v[i].pos[2] -= szZ*ctx.z.z;
          }

        v[i].uv[0]  = (dx[i]+0.5f);
        v[i].uv[1]  = (0.5f-dy[i]);

        v[i].norm[0] = -ctx.z.x;
        v[i].norm[1] = -ctx.z.y;
        v[i].norm[2] = -ctx.z.z;

        std::memcpy(&v[i].color,&color,4);
        }
      }
    }
  }
