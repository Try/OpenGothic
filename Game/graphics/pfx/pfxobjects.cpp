#include "pfxobjects.h"

#include <Tempest/Log>
#include <cstring>
#include <cassert>

#include "graphics/mesh/submesh/pfxemittermesh.h"
#include "graphics/dynamic/painter3d.h"
#include "graphics/mesh/pose.h"
#include "graphics/mesh/skeleton.h"
#include "graphics/sceneglobals.h"
#include "graphics/lightsource.h"
#include "graphics/rendererstorage.h"
#include "world/world.h"

#include "pfxbucket.h"
#include "particlefx.h"

using namespace Tempest;

PfxObjects::Emitter::Emitter(PfxBucket& b, size_t id)
  :bucket(&b), id(id) {
  }

PfxObjects::Emitter::~Emitter() {
  if(bucket!=nullptr) {
    std::lock_guard<std::recursive_mutex> guard(bucket->parent->sync);
    bucket->freeEmitter(id);
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
  auto& v = bucket->get(id);
  v.pos = pos;
  if(v.next!=nullptr)
    v.next->setPosition(pos);
  if(v.block==size_t(-1))
    return; // no backup memory
  auto& p = bucket->getBlock(*this);
  p.pos = pos;
  }

void PfxObjects::Emitter::setTarget(const Vec3& pos) {
  if(bucket==nullptr)
    return;
  std::lock_guard<std::recursive_mutex> guard(bucket->parent->sync);
  auto& v = bucket->get(id);
  v.target    = pos;
  v.hasTarget = true;
  }

void PfxObjects::Emitter::setDirection(const Matrix4x4& d) {
  if(bucket==nullptr)
    return;
  std::lock_guard<std::recursive_mutex> guard(bucket->parent->sync);
  auto& v = bucket->get(id);
  v.direction[0] = Vec3(d.at(0,0),d.at(0,1),d.at(0,2));
  v.direction[1] = Vec3(d.at(1,0),d.at(1,1),d.at(1,2));
  v.direction[2] = Vec3(d.at(2,0),d.at(2,1),d.at(2,2));

  if(v.next!=nullptr)
    v.next->setDirection(d);

  if(v.block==size_t(-1))
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
  auto& v = bucket->get(id);
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
  auto& v = bucket->get(id);
  return v.active;
  }

void PfxObjects::Emitter::setLooped(bool loop) {
  if(bucket==nullptr)
    return;
  std::lock_guard<std::recursive_mutex> guard(bucket->parent->sync);
  auto& v = bucket->get(id);
  v.isLoop = loop;
  if(v.next!=nullptr)
    v.next->setLooped(loop);
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
  viewerPos = pos;
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
    bucket[i]->tick(dt,viewerPos);

  for(auto& i:bucket)
    i->buildVbo(ctx);

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

PfxBucket& PfxObjects::getBucket(const ParticleFx &ow) {
  for(auto& i:bucket)
    if(i->owner==&ow)
      return *i;
  bucket.emplace_back(new PfxBucket(ow,this,visual));
  return *bucket.back();
  }

PfxBucket& PfxObjects::getBucket(const Material& mat, const ZenLoad::zCVobData& vob) {
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

