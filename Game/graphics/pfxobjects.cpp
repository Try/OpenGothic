#include "pfxobjects.h"

#include <cstring>

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
    auto& p = bucket->impl[id];
    bucket->freeBlock(p.id);
    p.id    = size_t(-1);
    p.alive = false;
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
  auto& v = bucket->impl[id];
  v.pos[0] = x;
  v.pos[1] = y;
  v.pos[2] = z;
  if(bucket->impl[id].id==size_t(-1))
    return; // no backup memory
  auto& p = bucket->getBlock(*this);
  p.pos[0] = x;
  p.pos[1] = y;
  p.pos[2] = z;
  }

void PfxObjects::Emitter::setActive(bool act) {
  if(bucket==nullptr)
    return;
  if(bucket->impl[id].id==size_t(-1) && act==false)
    return; // no backup memory
  auto& v = bucket->getBlock(*this);
  v.active=act;
  }

void PfxObjects::Emitter::setAttachPoint(const Skeleton *sk, const char *defBone) {
  skeleton = sk;
  if(skeleton!=nullptr)
    boneId = skeleton->findNode(defBone); else
    boneId = size_t(-1);
  }

void PfxObjects::Emitter::setSkeleton(const Pose &p, const Matrix4x4 &mt) {
  auto id=boneId;
  if(id>=p.tr.size()) {
    setPosition(mt.at(3,0),mt.at(3,1),mt.at(3,2));
    return;
    }
  auto mat=mt;
  mat.mul(p.tr[id]);
  setPosition(mat.at(3,0),mat.at(3,1),mat.at(3,2));
  }

void PfxObjects::Emitter::setObjMatrix(const Matrix4x4 &mt) { //fixme: usless for Npc
  auto id=boneId;
  if(skeleton==nullptr || id>=skeleton->tr.size()) {
    setPosition(mt.at(3,0),mt.at(3,1),mt.at(3,2));
    return;
    }
  auto mat=mt;
  mat.mul(skeleton->tr[id]);
  setPosition(mat.at(3,0),mat.at(3,1),mat.at(3,2));
  }

PfxObjects::Bucket::Bucket(const RendererStorage &storage, const ParticleFx &ow, PfxObjects *parent)
  :owner(&ow), parent(parent) {
  auto cnt = storage.device.maxFramesInFlight();

  pf.reset(new PerFrame[cnt]);
  for(size_t i=0;i<cnt;++i)
    pf[i].ubo = storage.device.uniforms(storage.uboPfxLayout());

  uint64_t lt      = ow.maxLifetime();
  uint64_t pps     = uint64_t(std::ceil(ow.ppsValue));
  uint64_t reserve = (lt*pps+1000-1)/1000;
  blockSize        = size_t(reserve);
  }

size_t PfxObjects::Bucket::allocBlock() {
  for(size_t i=0;i<block.size();++i) {
    if(!block[i].alive) {
      block[i].alive=true;
      return i;
      }
    }

  parent->updateCmd = true;
  block.emplace_back();

  Block& b = block.back();
  b.offset = particles.size();

  particles.resize(particles.size()+blockSize);
  vbo.resize(particles.size()*6);

  for(size_t i=0; i<blockSize; ++i)
    particles[i].life = 0;
  return block.size()-1;
  }

void PfxObjects::Bucket::freeBlock(size_t i) {
  if(i==size_t(-1))
    return;
  auto& b = block[i];
  b.active = false;
  b.alive  = b.count==0; // wait until all particles are gone
  }

PfxObjects::Block &PfxObjects::Bucket::getBlock(PfxObjects::ImplEmitter &e) {
  if(e.id==size_t(-1)) {
    e.id = allocBlock();

    auto& p  = block[e.id];

    p.owner  = size_t(&e-impl.data());
    p.pos[0] = e.pos[0];
    p.pos[1] = e.pos[1];
    p.pos[2] = e.pos[2];
    }
  return block[e.id];
  }

PfxObjects::Block &PfxObjects::Bucket::getBlock(PfxObjects::Emitter &e) {
  return getBlock(impl[e.id]);
  }

size_t PfxObjects::Bucket::alloc() {
  for(size_t i=0; i<impl.size(); ++i)
    if(!impl[i].alive) {
      impl[i].alive = true;
      return i;
      }
  impl.emplace_back();
  auto& e = impl.back();
  e.id    = size_t(-1); // no backup memory

  return impl.size()-1;
  }

void PfxObjects::Bucket::init(size_t particle) {
  auto& p = particles[particle];

  float dx=0,dy=0,dz=0;

  if(owner->shpType_S==ParticleFx::EmitterType::Point) {
    dx = 0;
    dy = 0;
    dz = 0;
    }
  else if(owner->shpType_S==ParticleFx::EmitterType::Sphere) {
    float theta = float(2.0*M_PI)*randf();
    float phi   = std::acos(1.f - 2.f * randf());
    dx    = std::sin(phi) * std::cos(theta);
    dy    = std::sin(phi) * std::sin(theta);
    dz    = std::cos(phi);
    }
  else if(owner->shpType_S==ParticleFx::EmitterType::Box) {
    dx  = randf()*2.f-1.f;
    dy  = randf()*2.f-1.f;
    dz  = randf()*2.f-1.f;
    } else {
    p.pos = Vec3();
    }
  Vec3 dim = owner->shpDim_S*0.5f;
  p.pos = Vec3(dx*dim.x,dy*dim.y,dz*dim.z)+owner->shpOffsetVec_S;

  if(owner->dirMode_S==ParticleFx::Dir::Rand) {
    p.rotation  = randf()*float(2.0*M_PI);
    }
  else if(owner->dirMode_S==ParticleFx::Dir::Dir) {
    p.rotation  = (owner->dirAngleHead+(2.f*randf()-1.f)*owner->dirAngleHeadVar)*float(M_PI)/180.f;
    p.drotation = (owner->dirAngleElev+(2.f*randf()-1.f)*owner->dirAngleElevVar)*float(M_PI)/180.f;
    }
  else if(owner->dirMode_S==ParticleFx::Dir::Target) {
    // p.rotation  = std::atan2(p.pos.y,p.pos.x);
    p.rotation  = randf()*float(2.0*M_PI); //FIXME
    }

  p.life    = uint16_t(owner->lspPartAvg+owner->lspPartVar*(2.f*randf()-1.f));
  p.maxLife = p.life;
  }

void PfxObjects::Bucket::finalize(size_t particle) {
  Vertex* v = &vbo[particle*6];
  std::memset(v,0,sizeof(*v)*6);
  }

bool PfxObjects::Bucket::shrink() {
  while(impl.size()>0) {
    auto& b = impl.back();
    if(b.alive)
      break;
    if(b.id!=size_t(-1))
      block[b.id].owner = size_t(-1);
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
    vbo.resize(particles.size()*6);
    return true;
    }
  return false;
  }

float PfxObjects::ParState::lifeTime() const {
  return 1.f-life/float(maxLife);
  }

PfxObjects::PfxObjects(const RendererStorage& storage)
  :storage(storage),uboGlobalPf(storage.device) {
  }

PfxObjects::Emitter PfxObjects::get(const ParticleFx &decl) {
  auto&  b = getBucket(decl);
  size_t e = b.alloc();
  updateCmd = true;
  return Emitter(b,e);
  }

void PfxObjects::setModelView(const Tempest::Matrix4x4 &m,const Tempest::Matrix4x4 &shadow) {
  uboGlobal.modelView  = m;
  uboGlobal.shadowView = shadow;
  }

void PfxObjects::setLight(const Light &l, const Vec3 &ambient) {
  auto  d = l.dir();
  auto& c = l.color();

  uboGlobal.lightDir = {-d[0],-d[1],-d[2]};
  uboGlobal.lightCl  = {c.x,c.y,c.z,0.f};
  uboGlobal.lightAmb = {ambient.x,ambient.y,ambient.z,0.f};
  }

bool PfxObjects::needToUpdateCommands() const {
  return updateCmd;
  }

void PfxObjects::setAsUpdated() {
  updateCmd=false;
  }

void PfxObjects::updateUbo(uint32_t imgId, uint64_t ticks) {
  uboGlobalPf.update(uboGlobal,imgId);
  uint64_t dt = ticks-lastUpdate;

  for(auto& i:bucket) {
    if(dt!=0) {
      tickSys(i,dt);
      buildVbo(i);
      }

    auto& pf = i.pf[imgId];
    pf.vbo.update(i.vbo);
    }
  lastUpdate = ticks;
  }

void PfxObjects::commitUbo(uint32_t imgId, const Texture2d& shadowMap) {
  bucket.remove_if([](const Bucket& ){
    // FIXME: Cannot free VkNonDispatchableHandle that is in use by a command buffer.
    return false;//b.impl.size()==0;
    });

  for(auto& i:bucket) {
    auto& pf = i.pf[imgId];

    pf.ubo.set(0,uboGlobalPf[imgId],0,1);
    pf.ubo.set(2,*i.owner->visName_S);
    pf.ubo.set(3,shadowMap);
    }
  }

void PfxObjects::draw(Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint32_t imgId) {
  for(auto& i:bucket) {
    auto& pf = i.pf[imgId];
    pf.vbo = storage.device.vboDyn(i.vbo);

    uint32_t offset=0;
    cmd.setUniforms(storage.pPfx,pf.ubo,1,&offset);
    cmd.draw(pf.vbo);
    }
  }

float PfxObjects::randf() {
  return float(rndEngine()%10000)/10000.f;
  }

PfxObjects::Bucket &PfxObjects::getBucket(const ParticleFx &ow) {
  for(auto& i:bucket)
    if(i.owner==&ow)
      return i;
  bucket.push_front(Bucket(storage,ow,this));
  return *bucket.begin();
  }

void PfxObjects::tickSys(PfxObjects::Bucket &b,uint64_t dt) {
  float k = float(dt)/1000.f;

  for(auto& p:b.block) {
    if(p.count>0) {
      for(size_t i=0;i<b.blockSize;++i) {
        ParState& ps = b.particles[i+p.offset];
        if(ps.life==0)
          continue;
        if(ps.life<=dt){
          ps.life = 0;
          p.count--;
          b.finalize(i+p.offset);
          } else {
          // eval particle
          ps.life  = uint16_t(ps.life-dt);
          ps.pos  += ps.dir*k;
          ps.pos  += b.owner->flyGravity_S;
          }
        }
      }

    p.timeTotal+=dt;
    uint64_t fltScale = 100;
    uint64_t emited   = uint64_t(p.timeTotal*uint64_t(b.owner->ppsValue*float(fltScale)))/uint64_t(1000u*fltScale);
    if(!p.active) {
      p.emited = emited;
      if(p.count==0) {
        p.alive=false;
        if(p.owner!=size_t(-1))
          b.impl[p.owner].id=size_t(-1);
        p.owner=size_t(-1);
        updateCmd |= b.shrink();
        }
      } else {
      while(p.emited<emited) {
        p.emited++;

        for(size_t i=0;i<b.blockSize;++i) {
          ParState& ps = b.particles[i+p.offset];
          if(ps.life==0) { // free slot
            p.count++;
            b.init(i+p.offset);
            break;
            }
          }
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

void PfxObjects::buildVbo(PfxObjects::Bucket &b) {
  static const float dx[6] = {-0.5f, 0.5f, -0.5f,  0.5f,  0.5f, -0.5f};
  static const float dy[6] = { 0.5f, 0.5f, -0.5f,  0.5f, -0.5f, -0.5f};

  const auto& m = uboGlobal.modelView;
  float left[4] = {m.at(0,0),m.at(1,0),m.at(2,0)};
  float top [4] = {m.at(0,1),m.at(1,1),m.at(2,1)};

  left[3] = std::sqrt(left[0]*left[0]+left[1]*left[1]+left[2]*left[2]);
  top [3] = std::sqrt( top[0]* top[0]+ top[1]* top[1]+ top[2]* top[2]);

  for(int i=0;i<3;++i) {
    left[i] /= left[3];
    top [i] /= top [3];
    }

  auto& ow              = *b.owner;
  auto  colorS          = ow.visTexColorStart_S;
  auto  colorE          = ow.visTexColorEnd_S;
  auto  visSizeStart_S  = ow.visSizeStart_S;
  auto  visSizeEndScale = ow.visSizeEndScale;
  auto  visAlphaStart   = ow.visAlphaStart;
  auto  visAlphaEnd     = ow.visAlphaEnd;
  auto  visAlphaFunc    = ow.visAlphaFunc_S;

  for(auto& p:b.block) {
    if(p.count==0)
      continue;

    for(size_t i=0;i<b.blockSize;++i) {
      ParState& ps = b.particles[i+p.offset];
      Vertex*   v  = &b.vbo[(p.offset+i)*6];

      const float a   = ps.lifeTime();
      const Vec3  cl  = colorS*(1.f-a)        + colorE*a;
      const float clA = visAlphaStart*(1.f-a) + visAlphaEnd*a;

      const float szX = visSizeStart_S.x*(1.f + a*(visSizeEndScale-1.f));
      const float szY = visSizeStart_S.y*(1.f + a*(visSizeEndScale-1.f));

      float l[3]={};
      float t[3]={};
      rotate(l,t,ps.rotation,left,top);

      struct Color {
        uint8_t r=0;
        uint8_t g=0;
        uint8_t b=0;
        uint8_t a=0;
        } color;

      if(visAlphaFunc==ParticleFx::AlphaFunc::Add) {
        color.r = uint8_t(cl.x*clA);
        color.g = uint8_t(cl.y*clA);
        color.b = uint8_t(cl.z*clA);
        color.a = uint8_t(255);
        }
      else if(visAlphaFunc==ParticleFx::AlphaFunc::Blend) {
        color.r = uint8_t(cl.x);
        color.g = uint8_t(cl.y);
        color.b = uint8_t(cl.z);
        color.a = uint8_t(clA*255);
        }

      for(int i=0;i<6;++i) {
        float sx = l[0]*dx[i]*szX + t[0]*dy[i]*szY;
        float sy = l[1]*dx[i]*szX + t[1]*dy[i]*szY;
        float sz = l[2]*dx[i]*szX + t[2]*dy[i]*szY;

        v[i].pos[0] = ps.pos.x + p.pos[0] + sx;
        v[i].pos[1] = ps.pos.y + p.pos[1] + sy;
        v[i].pos[2] = ps.pos.z + p.pos[2] + sz;

        v[i].uv[0]  = (dx[i]+0.5f);//float(ow.frameCount);
        v[i].uv[1]  = (dy[i]+0.5f);

        std::memcpy(&v[i].color,&color,4);
        }
      }
    }
  }
