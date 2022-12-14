#include "pfxobjects.h"

#include <Tempest/Log>
#include <cstring>
#include <cassert>

#include "graphics/sceneglobals.h"

#include "pfxbucket.h"
#include "particlefx.h"

using namespace Tempest;

PfxObjects::PfxObjects(WorldView& world, const SceneGlobals& scene, VisualObjects& visual)
  :world(world), scene(scene), visual(visual) {
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

  for(auto& i:bucket)
    i.tick(dt,viewerPos);

  for(auto& i:bucket)
    i.buildSsbo();

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

  for(auto& i:bucket)
    i.preFrameUpdate(fId);
  }

PfxBucket& PfxObjects::getBucket(const ParticleFx &decl) {
  for(auto& i:bucket)
    if(&i.decl==&decl)
      return i;
  bucket.emplace_back(decl,*this,visual);
  return bucket.back();
  }

PfxBucket& PfxObjects::getBucket(const Material& mat, const phoenix::vob& vob) {
  for(auto& i:spriteEmit)
    if(i.pfx->visMaterial==mat &&
       i.visualCamAlign==vob.sprite_camera_facing_mode &&
       i.zBias==vob.bias &&
       i.decalDim==Tempest::Vec2{vob.visual_decal->dimension.x, vob.visual_decal->dimension.y}) {
      return getBucket(*i.pfx);
      }
  spriteEmit.emplace_back();
  auto& e = spriteEmit.back();

  e.visualCamAlign = vob.sprite_camera_facing_mode;
  e.zBias          = vob.bias;
  e.decalDim       = Tempest::Vec2{vob.visual_decal->dimension.x, vob.visual_decal->dimension.y};
  e.pfx.reset(new ParticleFx(mat,vob));
  return getBucket(*e.pfx);
  }

