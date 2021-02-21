#include "pfxbucket.h"

#include "graphics/mesh/submesh/pfxemittermesh.h"
#include "pfxobjects.h"
#include "particlefx.h"

using namespace Tempest;

static void     rotate(Vec3& rx, Vec3& ry,float a,const Vec3& x, const Vec3& y){
  const float c = std::cos(a);
  const float s = std::sin(a);

  rx.x = x.x*c - y.x*s;
  rx.y = x.y*c - y.y*s;
  rx.z = x.z*c - y.z*s;

  ry.x = x.x*s + y.x*c;
  ry.y = x.y*s + y.y*c;
  ry.z = x.z*s + y.z*c;
  }

static uint64_t ppsDiff(const ParticleFx& decl, bool loop, uint64_t time0, uint64_t time1) {
  if(time1<=time0)
    return 0;
  if(!loop) {
    time0 = std::min(decl.prefferedTime,time0);
    time1 = std::min(decl.prefferedTime,time1);
    }
  const float pps     = decl.ppsScale(time1)*decl.ppsValue;
  uint64_t    emitted0 = uint64_t(pps*float(time0)/1000.f);
  uint64_t    emitted1 = uint64_t(pps*float(time1)/1000.f);
  return emitted1-emitted0;
  }

float PfxBucket::ParState::lifeTime() const {
  return 1.f-life/float(maxLife);
  }

std::mt19937 PfxBucket::rndEngine;


PfxBucket::PfxBucket(const ParticleFx &ow, PfxObjects* parent, VisualObjects& visual)
  :owner(&ow), parent(parent), visual(visual) {
  const Tempest::VertexBuffer<Resources::Vertex>* vbo[Resources::MaxFramesInFlight] = {};
  for(size_t i=0;i<Resources::MaxFramesInFlight;++i)
    vbo[i] = &vboGpu[i];
  item = visual.get(vbo,owner->visMaterial,Bounds());

  Matrix4x4 ident;
  ident.identity();
  item.setObjMatrix(ident);

  uint64_t lt      = ow.maxLifetime();
  uint64_t pps     = uint64_t(std::ceil(ow.maxPps()));
  uint64_t reserve = (lt*pps+1000-1)/1000;
  blockSize        = size_t(reserve);
  if(blockSize==0)
    blockSize=1;
  }

PfxBucket::~PfxBucket() {
  }

bool PfxBucket::isEmpty() const {
  for(size_t i=0;i<Resources::MaxFramesInFlight;++i) {
    if(vboGpu[i].size()>0)
      return false;
    }
  return impl.size()==0;
  }

size_t PfxBucket::allocBlock() {
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
    particles[b.offset+i].life = 0;
  return block.size()-1;
  }

void PfxBucket::freeBlock(size_t& i) {
  if(i==size_t(-1))
    return;
  auto& b = block[i];
  assert(b.count==0);
  b.alive = false;
  i = size_t(-1);
  }

PfxBucket::Block& PfxBucket::getBlock(ImplEmitter &e) {
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

PfxBucket::Block& PfxBucket::getBlock(PfxEmitter& e) {
  return getBlock(impl[e.id]);
  }

size_t PfxBucket::allocEmitter() {
  for(size_t i=0; i<impl.size(); ++i) {
    auto& b = impl[i];
    if(b.st==S_Free) {
      b.st = S_Inactive;
      return i;
      }
    }
  impl.emplace_back();
  auto& e = impl.back();
  e.block = size_t(-1); // no backup memory
  e.st    = S_Inactive;

  return impl.size()-1;
  }

void PfxBucket::freeEmitter(size_t& id) {
  auto& v = impl[id];
  if(v.block!=size_t(-1)) {
    auto& b = getBlock(v);
    v.st    = b.count==0 ? S_Free : S_Fade;
    } else {
    v.st    = S_Free;
    }
  v.next.reset();
  id = size_t(-1);
  }

bool PfxBucket::shrink() {
  while(impl.size()>0) {
    auto& b = impl.back();
    if(b.st!=S_Free)
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

float PfxBucket::randf() {
  return float(rndEngine()%10000)/10000.f;
  }

float PfxBucket::randf(float base, float var) {
  return (2.f*randf()-1.f)*var + base;
  }

void PfxBucket::init(PfxBucket::Block& block, ImplEmitter& emitter, size_t particle) {
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
      auto mesh = (emitter.mesh!=nullptr) ? emitter.mesh : pfx.shpMesh;
      auto pose = (emitter.mesh!=nullptr) ? emitter.pose : nullptr;
      if(mesh!=nullptr) {
        auto pos = mesh->randCoord(randf(),pose);
        p.pos = block.direction[0]*pos.x +
                block.direction[1]*pos.y +
                block.direction[2]*pos.z;
        }
      break;
      }
    }

  if(pfx.shpType!=ParticleFx::EmitterType::Mesh) {
    Vec3 dim = pfx.shpDim*pfx.shpScale(block.timeTotal);
    p.pos.x*=dim.x;
    p.pos.y*=dim.y;
    p.pos.z*=dim.z;
    }

  switch(pfx.shpFOR) {
    case ParticleFx::Frame::Object:
    case ParticleFx::Frame::Node: {
      p.pos += block.direction[0]*pfx.shpOffsetVec.x +
               block.direction[1]*pfx.shpOffsetVec.y +
               block.direction[2]*pfx.shpOffsetVec.z;
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
        case ParticleFx::Frame::Node: {
          p.dir = block.direction[0]*dx +
                  block.direction[1]*dy +
                  block.direction[2]*dz;
          break;
          }
        case ParticleFx::Frame::World:
        case ParticleFx::Frame::Object: {
          p.dir = Vec3(dx,dy,dz);
          break;
          }
        }

      float l = p.dir.manhattanLength();
      if(l>0)
        p.dir/=l;
      dirRotation = std::atan2(p.dir.x,p.dir.y);
      break;
      }
    case ParticleFx::Dir::Target:
      p.dir = pfx.dirModeTargetPos;
      dirRotation = randf()*float(2.0*M_PI);
      break;
    }

  switch(pfx.visOrientation){
    case ParticleFx::Orientation::None:
      switch(pfx.dirMode) {
        case ParticleFx::Dir::Target:
          p.rotation = dirRotation;
          break;
        default:
          p.rotation = 0; //randf()*float(2.0*M_PI);
          break;
        }
      break;
    case ParticleFx::Orientation::Velocity:
      p.rotation = dirRotation;
      break;
    case ParticleFx::Orientation::Velocity3d:
      p.rotation = dirRotation;
      break;
    }

  if(!pfx.useEmittersFOR)
    p.pos += block.pos;
  }

void PfxBucket::finalize(size_t particle) {
  Vertex* v = &vboCpu[particle*6];
  std::memset(v,0,sizeof(*v)*6);
  auto& p = particles[particle];
  p = {};
  }

void PfxBucket::tick(Block& sys, ImplEmitter& emitter, size_t particle, uint64_t dt) {
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
      Vec3 dx, from, to;
      if(emitter.hasTarget)
        to = emitter.target; else
        to = sys.pos;

      if(pfx.useEmittersFOR)
        from = ps.pos+sys.pos; else
        from = ps.pos;

      dx = to - from;
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

void PfxBucket::tick(uint64_t dt, const Vec3& viewPos) {
  bool doShrink = false;
  for(auto& emitter:impl) {
    const auto dp     = emitter.pos-viewPos;
    const bool nearby = (dp.quadLength()<PfxObjects::viewRage*PfxObjects::viewRage);

    if(emitter.next==nullptr && owner->ppsCreateEm!=nullptr && emitter.waitforNext<dt && emitter.st==S_Active) {
      emitter.next.reset(new PfxEmitter(*parent,owner->ppsCreateEm));
      auto& e = *emitter.next;
      e.setPosition(emitter.pos.x,emitter.pos.y,emitter.pos.z);
      e.setActive(true);
      e.setLooped(emitter.isLoop);
      }

    if(emitter.waitforNext>=dt)
      emitter.waitforNext-=dt;

    if(emitter.st==S_Free)
      continue;

    auto& p = getBlock(emitter);
    if(p.count>0) {
      for(size_t i=0;i<blockSize;++i)
        tick(p,emitter,i,dt);
      if(p.count==0 && emitter.st==S_Fade) {
        // free mem
        freeBlock(emitter.block);
        emitter.st = S_Free;
        doShrink = true;
        continue;
        }
      }

    if(owner->ppsValue<0) {
      tickEmit(p,emitter,p.count==0 ? 1 : 0);
      }
    else if(emitter.st==S_Active && nearby) {
      auto dE = ppsDiff(*owner,emitter.isLoop,p.timeTotal,p.timeTotal+dt);
      tickEmit(p,emitter,dE);
      }
    p.timeTotal+=dt;
    }

  if(doShrink)
    shrink();
  }

void PfxBucket::tickEmit(Block& p, ImplEmitter& emitter, uint64_t emited) {
  size_t lastI = 0;
  for(size_t id=1; emited>0; ++id) {
    const size_t i  = id%blockSize;
    ParState&    ps = particles[i+p.offset];
    if(ps.life==0) { // free slot
      --emited;
      lastI = i;
      init(p,emitter,i+p.offset);
      if(ps.life==0)
        continue;
      p.count++;
      } else {
      // out of slots
      if(lastI==i)
        return;
      }
    }
  }

void PfxBucket::buildVbo(const PfxObjects::VboContext& ctx) {
  static const float dx[6] = {-0.5f, 0.5f, -0.5f,  0.5f,  0.5f, -0.5f};
  static const float dy[6] = { 0.5f, 0.5f, -0.5f,  0.5f, -0.5f, -0.5f};

  auto& pfx             = *owner;
  auto  colorS          = pfx.visTexColorStart;
  auto  colorE          = pfx.visTexColorEnd;
  auto  visSizeStart    = pfx.visSizeStart;
  auto  visSizeEndScale = pfx.visSizeEndScale;
  auto  visAlphaStart   = pfx.visAlphaStart;
  auto  visAlphaEnd     = pfx.visAlphaEnd;
  auto  visAlphaFunc    = pfx.visMaterial.alpha;

  const Vec3& left = pfx.visYawAlign ? ctx.leftA : ctx.left;
  const Vec3& top  = pfx.visYawAlign ? ctx.topA  : ctx.top;

  for(auto& p:block) {
    if(p.count==0)
      continue;

    for(size_t i=0;i<blockSize;++i) {
      ParState& ps = particles[i+p.offset];
      Vertex*   v  = &vboCpu[(p.offset+i)*6];

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
        static float k1 = -1, k2 = -1;
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

        if(pfx.useEmittersFOR) {
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
        v[i].uv[1]  = (dy[i]+0.5f);

        v[i].norm[0] = -ctx.z.x;
        v[i].norm[1] = -ctx.z.y;
        v[i].norm[2] = -ctx.z.z;

        std::memcpy(&v[i].color,&color,4);
        }
      }
    }
  }
