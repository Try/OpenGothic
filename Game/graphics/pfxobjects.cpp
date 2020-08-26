#include "pfxobjects.h"
#include "sceneglobals.h"

#include <cstring>
#include <cassert>

#include "graphics/submesh/pfxemittermesh.h"
#include "graphics/dynamic/painter3d.h"
#include "light.h"
#include "particlefx.h"
#include "pose.h"
#include "rendererstorage.h"
#include "skeleton.h"

using namespace Tempest;

std::mt19937 PfxObjects::rndEngine;

PfxObjects::Emitter::Emitter(PfxObjects::Bucket& b, size_t id)
  :bucket(&b), id(id) {
  }

PfxObjects::Emitter::~Emitter() {
  if(bucket) {
    std::lock_guard<std::mutex> guard(bucket->parent->sync);
    auto& p  = bucket->impl[id];
    p.alive  = false;
    p.active = false;
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
  if(bucket==nullptr)
    return;
  std::lock_guard<std::mutex> guard(bucket->parent->sync);
  auto& v = bucket->impl[id];
  v.pos = Vec3(x,y,z);
  if(bucket->impl[id].block==size_t(-1))
    return; // no backup memory
  auto& p = bucket->getBlock(*this);
  p.pos = Vec3(x,y,z);
  }

void PfxObjects::Emitter::setTarget(const Vec3& pos) {
  if(bucket==nullptr)
    return;
  std::lock_guard<std::mutex> guard(bucket->parent->sync);
  if(bucket->impl[id].block==size_t(-1))
    return; // no backup memory
  auto& p     = bucket->getBlock(*this);
  p.target    = pos;
  p.hasTarget = true;
  }

void PfxObjects::Emitter::setDirection(const Matrix4x4& d) {
  if(bucket==nullptr)
    return;
  std::lock_guard<std::mutex> guard(bucket->parent->sync);
  auto& v = bucket->impl[id];
  v.direction[0] = Vec3(d.at(0,0),d.at(0,1),d.at(0,2));
  v.direction[1] = Vec3(d.at(1,0),d.at(1,1),d.at(1,2));
  v.direction[2] = Vec3(d.at(2,0),d.at(2,1),d.at(2,2));

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
  std::lock_guard<std::mutex> guard(bucket->parent->sync);
  bucket->impl[id].active = act;
  }

bool PfxObjects::Emitter::isActive() const {
  if(bucket==nullptr)
    return false;
  std::lock_guard<std::mutex> guard(bucket->parent->sync);
  return bucket->impl[id].active;
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
  std::lock_guard<std::mutex> guard(sync);
  auto&  b = getBucket(decl);
  size_t e = b.allocEmitter();
  return Emitter(b,e);
  }

PfxObjects::Emitter PfxObjects::get(const ZenLoad::zCVobData& vob) {
  Material mat(vob);

  std::lock_guard<std::mutex> guard(sync);
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
  const auto& m = scene.modelView();
  for(int i=0;i<3;++i) {
    ctx.left[i] = m.at(i,0);
    ctx.top [i] = m.at(i,1);
    ctx.z   [i] = m.at(i,2);

    ctx.left[3] += (ctx.left[i]*ctx.left[i]);
    ctx.top [3] += (ctx.top [i]*ctx.top [i]);
    ctx.z   [3] += (ctx.z   [i]*ctx.z   [i]);
    }

  ctx.left[3] = std::sqrt(ctx.left[3]);
  ctx.top [3] = std::sqrt(ctx.top[3]);
  ctx.z   [3] = std::sqrt(ctx.z[3]);

  for(int i=0;i<3;++i) {
    ctx.left[i] /= ctx.left[3];
    ctx.top [i] /= ctx.top [3];
    ctx.z   [i] /= ctx.z[3];
    }

  ctx.leftA[0] = ctx.left[0];
  ctx.leftA[2] = ctx.left[2];
  ctx.topA[1]  = -1;

  for(auto& i:bucket) {
    tickSys(*i,dt);
    buildVbo(*i,ctx);
    }
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

void PfxObjects::tickSys(PfxObjects::Bucket &b, uint64_t dt) {
  bool doShrink = false;
  for(auto& emitter:b.impl) {
    const auto dp      = emitter.pos-viewePos;
    const bool active  = emitter.active;
    const bool nearby  = (dp.quadLength()<4000*4000);
    const bool process = active && nearby;

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

    p.timeTotal+=dt;
    float          fltScale = 100;
    const float    pps      = b.owner->ppsValue*b.owner->ppsScale(p.timeTotal)*fltScale;
    uint64_t       emited   = uint64_t(p.timeTotal*uint64_t(pps))/uint64_t(1000.f*fltScale);
    if(b.owner->ppsValue<0){
      p.emited = p.count;
      emited   = 1;
      tickSysEmit(b,p,emited);
      }
    else if(!nearby) {
      p.emited = emited;
      }
    else if(active) {
      tickSysEmit(b,p,emited);
      }
    }

  if(doShrink)
    b.shrink();
  }

void PfxObjects::tickSysEmit(PfxObjects::Bucket& b, PfxObjects::Block& p, uint64_t emited) {
  while(p.emited<emited) {
    p.emited++;

    for(size_t i=0;i<b.blockSize;++i) {
      ParState& ps = b.particles[i+p.offset];
      if(ps.life==0) { // free slot
        p.count++;
        b.init(p,i+p.offset);
        break;
        }
      }
    }
  }

static void rotate(float* rx,float* ry,float a,const float* x, const float* y){
  const float c = std::cos(a);
  const float s = std::sin(a);

  for(int i=0;i<3;++i) {
    rx[i] = x[i]*c - y[i]*s;
    ry[i] = x[i]*s + y[i]*c;
    }
  }

static void cross(float* out,const float* u,const float* v) {
  out[0] = (u[1]*v[2] - u[2]*v[1]);
  out[1] = (u[2]*v[0] - u[0]*v[2]);
  out[2] = (u[0]*v[1] - u[1]*v[0]);
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

  const float* left = ctx.left;
  const float* top  = ctx.top;
  if(pfx.visYawAlign) {
    left = ctx.leftA;
    top  = ctx.topA;
    }

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

      float l[3]={};
      float t[3]={};

      if(pfx.visOrientation==ParticleFx::Orientation::Velocity3d) {
        static float k1 = 1, k2 = -1;
        t[0] = k1* ps.dir.x;
        t[1] = k1* ps.dir.y;
        t[2] = k1* ps.dir.z;
        cross(l, t,ctx.z);
        l[0]*=k2;
        l[1]*=k2;
        l[2]*=k2;
        } else {
        rotate(l,t,ps.rotation,left,top);
        }

      struct Color {
        uint8_t r=255;
        uint8_t g=255;
        uint8_t b=255;
        uint8_t a=255;
        } color;

      if(visAlphaFunc==Material::ApphaFunc::AdditiveLight) {
        color.r = uint8_t(cl.x*clA);
        color.g = uint8_t(cl.y*clA);
        color.b = uint8_t(cl.z*clA);
        color.a = uint8_t(255);
        }
      else if(visAlphaFunc==Material::ApphaFunc::Transparent) {
        color.r = uint8_t(cl.x);
        color.g = uint8_t(cl.y);
        color.b = uint8_t(cl.z);
        color.a = uint8_t(clA*255);
        }

      for(int i=0;i<6;++i) {
        float sx = l[0]*dx[i]*szX + t[0]*dy[i]*szY;
        float sy = l[1]*dx[i]*szX + t[1]*dy[i]*szY;
        float sz = l[2]*dx[i]*szX + t[2]*dy[i]*szY;

        v[i].pos[0] = ps.pos.x + p.pos.x + sx;
        v[i].pos[1] = ps.pos.y + p.pos.y + sy;
        v[i].pos[2] = ps.pos.z + p.pos.z + sz;

        if(pfx.visZBias) {
          v[i].pos[0] -= szZ*ctx.z[0];
          v[i].pos[1] -= szZ*ctx.z[1];
          v[i].pos[2] -= szZ*ctx.z[2];
          }

        v[i].uv[0]  = (dx[i]+0.5f);//float(ow.frameCount);
        v[i].uv[1]  = (dy[i]+0.5f);

        std::memcpy(&v[i].color,&color,4);
        }
      }
    }
  }
