#include "pfxobjects.h"

#include <Tempest/Log>
#include <cstring>
#include <cassert>

#include "graphics/mesh/submesh/pfxemittermesh.h"
#include "graphics/mesh/pose.h"
#include "graphics/mesh/skeleton.h"
#include "graphics/sceneglobals.h"
#include "graphics/lightsource.h"
#include "world/world.h"

#include "pfxbucket.h"
#include "particlefx.h"

using namespace Tempest;

PfxObjects::PfxObjects(WorldView& world, const SceneGlobals& scene, VisualObjects& visual)
  :world(world), scene(scene), visual(visual), trails(visual) {
  }

PfxObjects::~PfxObjects() {
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
  ctx.vp     = m;

  ctx.left.x = m.at(0,0);
  ctx.left.y = m.at(1,0);
  ctx.left.z = m.at(2,0);

  ctx.top.x  = m.at(0,1);
  ctx.top.y  = m.at(1,1);
  ctx.top.z  = m.at(2,1);

  ctx.z.x    = m.at(0,2);
  ctx.z.y    = m.at(1,2);
  ctx.z.z    = m.at(2,2);

  ctx.left/=ctx.left.length();
  ctx.top /=ctx.top.length();
  ctx.z   /=ctx.z.length();

  ctx.leftA.x = ctx.left.x;
  ctx.leftA.z = ctx.left.z;
  ctx.topA.y  = -1;

  for(auto& i:bucket)
    i.tick(dt,viewerPos);

  trails.tick(dt);

  for(auto& i:bucket)
    i.buildVbo(ctx);
  trails.buildVbo(-ctx.z);

  lastUpdate = ticks;
  }

bool PfxObjects::isInPfxRange(const Vec3& pos) const {
  auto dp = viewerPos-pos;
  return dp.quadLength()<viewRage*viewRage;
  }

void PfxObjects::preFrameUpdate(uint8_t fId) {
  for(auto i=bucket.begin(), end = bucket.end(); i!=end; ) {
    if(i->isEmpty()) {
      i = bucket.erase(i);
      } else {
      ++i;
      }
    }

  auto& device = Resources::device();
  for(auto& i:bucket) {
    auto& vbo = i.vboGpu[fId];
    if(i.vboCpu.size()!=vbo.size())
      vbo = device.vbo(BufferHeap::Upload,i.vboCpu); else
      vbo.update(i.vboCpu);
    }

  trails.preFrameUpdate(fId);
  }

PfxBucket& PfxObjects::getBucket(const ParticleFx &decl) {
  for(auto& i:bucket)
    if(&i.decl==&decl)
      return i;
  bucket.emplace_back(decl,*this,visual);
  return bucket.back();
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

