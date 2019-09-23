#include "pfxobjects.h"

#include "light.h"
#include "particlefx.h"
#include "rendererstorage.h"

using namespace Tempest;

PfxObjects::Emitter::Emitter(PfxObjects::Bucket& b, size_t id)
  :bucket(&b), id(id) {
  }

PfxObjects::Emitter::~Emitter() {
  if(bucket)
    bucket->impl[id].enable = false;
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
  }

PfxObjects::Bucket::Bucket(const RendererStorage &storage, const ParticleFx &ow)
  :owner(&ow) {
  auto cnt = storage.device.maxFramesInFlight();

  pf.reset(new PerFrame[cnt]);
  for(size_t i=0;i<cnt;++i)
    pf[i].ubo = storage.device.uniforms(storage.uboPfxLayout());
  }


PfxObjects::PfxObjects(const RendererStorage& storage)
  :storage(storage),uboGlobalPf(storage.device) {
  }

PfxObjects::Emitter PfxObjects::get(const ParticleFx &decl) {
  auto& b = getBucket(decl);
  b.impl.emplace_back();

  updateCmd = true;
  return Emitter(b,b.impl.size()-1);
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

void PfxObjects::updateUbo(uint32_t imgId) {
  uboGlobalPf.update(uboGlobal,imgId);

  for(auto& b:bucket)
    tickSys(b);
  }

void PfxObjects::commitUbo(uint32_t imgId, const Tempest::Texture2d& shadowMap) {
  for(auto& i:bucket) {
    auto& pf = i.pf[imgId];

    pf.ubo.set(0,uboGlobalPf[imgId],0,sizeof(UboGlobal));
    pf.ubo.set(2,*i.owner->view);
    pf.ubo.set(3,shadowMap);

    pf.vbo.update(i.vbo);
    }
  }

void PfxObjects::draw(Tempest::CommandBuffer &cmd, uint32_t imgId) {
  for(auto& i:bucket) {
    tickSys(i);

    auto& pf = i.pf[imgId];
    pf.vbo = storage.device.loadVboDyn(i.vbo);

    uint32_t offset=0;
    cmd.setUniforms(storage.pPfx,pf.ubo,1,&offset);
    cmd.draw(pf.vbo);
    }
  }

PfxObjects::Bucket &PfxObjects::getBucket(const ParticleFx &ow) {
  for(auto& i:bucket)
    if(i.owner==&ow)
      return i;

  updateCmd = true;
  bucket.push_front(Bucket(storage,ow));
  return *bucket.begin();
  }

void PfxObjects::tickSys(PfxObjects::Bucket &b) {
  static const float dx[6] = {-1.f, 1.f, -1.f,  1.f,  1.f, -1.f};
  static const float dy[6] = { 1.f, 1.f, -1.f,  1.f, -1.f, -1.f};

  auto& ow = *b.owner;
  float scale=100.f;

  b.vbo.resize(b.impl.size()*6);
  for(size_t i=0;i<b.impl.size();++i) {
    auto&   p = b.impl[i];
    Vertex* v = &b.vbo[i*6];

    uint32_t color=0;
    color|=uint32_t(ow.colorS.x) << 24;
    color|=uint32_t(ow.colorS.y) << 16;
    color|=uint32_t(ow.colorS.z) << 8;
    color|=uint32_t(255) << 0;

    for(int i=0;i<6;++i) {
      v[i].pos[0] = p.pos[0] + scale*dx[i];
      v[i].pos[1] = p.pos[1] + scale*dy[i] + 150;
      v[i].pos[2] = p.pos[2] + 0.f;

      v[i].uv[0]  = (dx[i]*0.5f+0.5f);//float(ow.frameCount);
      v[i].uv[1]  = (dy[i]*0.5f+0.5f);

      v[i].color  = color;
      }
    }
  }

