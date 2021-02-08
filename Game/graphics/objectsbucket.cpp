#include "objectsbucket.h"

#include <Tempest/Log>

#include "graphics/dynamic/painter3d.h"
#include "graphics/mesh/pose.h"
#include "graphics/mesh/skeleton.h"
#include "sceneglobals.h"

#include "utils/workers.h"
#include "visualobjects.h"
#include "rendererstorage.h"

using namespace Tempest;

void ObjectsBucket::Item::setObjMatrix(const Tempest::Matrix4x4 &mt) {
  owner->setObjMatrix(id,mt);
  }

void ObjectsBucket::Item::setPose(const Pose &p) {
  owner->setPose(id,p);
  }

void ObjectsBucket::Item::setBounds(const Bounds& bbox) {
  owner->setBounds(id,bbox);
  }

void ObjectsBucket::Item::setAsGhost(bool g) {
  if(owner->mat.isGhost==g)
    return;

  auto m = owner->mat;
  m.isGhost = g;
  auto& bucket = owner->owner.getBucket(m,owner->boneCnt,owner->shaderType);

  auto&  v      = owner->val[id];
  size_t idNext = size_t(-1);
  switch(v.vboType) {
    case NoVbo:
      break;
    case VboVertex:{
      idNext = bucket.alloc(*v.vbo,*v.ibo,v.bounds);
      break;
      }
    case VboVertexA:{
      idNext = bucket.alloc(*v.vboA,*v.ibo,v.bounds);
      break;
      }
    case VboMorph:{
      idNext = bucket.alloc(v.vboM,v.bounds);
      break;
      }
    }
  if(idNext==size_t(-1))
    return;

  owner->free(id);
  owner = &bucket;
  id    = idNext;
  }

const Bounds& ObjectsBucket::Item::bounds() const {
  if(owner!=nullptr)
    return owner->bounds(id);
  static Bounds b;
  return b;
  }

void ObjectsBucket::Item::draw(Tempest::Encoder<Tempest::CommandBuffer>& p, uint8_t fId) const {
  owner->draw(id,p,fId);
  }


void ObjectsBucket::Descriptors::invalidate() {
  for(size_t i=0;i<Resources::MaxFramesInFlight;++i)
    uboIsReady[i] = false;
  }

void ObjectsBucket::Descriptors::alloc(ObjectsBucket& owner) {
  for(size_t i=0;i<Resources::MaxFramesInFlight;++i) {
    if(owner.pMain!=nullptr)
      ubo[i] = owner.scene.storage.device.uniforms(owner.pMain->layout());
    if(owner.pShadow!=nullptr) {
      for(size_t lay=0;lay<Resources::ShadowLayers;++lay)
        uboSh[i][lay] = owner.scene.storage.device.uniforms(owner.pShadow->layout());
      }
    }
  }

ObjectsBucket::ObjectsBucket(const Material& mat, size_t boneCount, VisualObjects& owner, const SceneGlobals& scene, Storage& storage, const Type type)
  :owner(owner), boneCnt(boneCount), scene(scene), storage(storage), mat(mat), shaderType(type), useSharedUbo(type!=Animated) {
  static_assert(sizeof(UboPush)<=128, "UboPush is way too big");


  if(mat.isGhost) {
    if(shaderType==Animated)
      pMain = &scene.storage.pAnimGhost; else
      pMain = &scene.storage.pObjectGhost;
    // if(shaderType==Animated)
    //   pMain = &scene.storage.pAnimMAdd; else
    //   pMain = &scene.storage.pObjectMAdd;
    } else {
    switch(mat.alpha) {
      case Material::AlphaTest:
        if(shaderType==Animated) {
          pMain    = &scene.storage.pAnimAt;
          pGbuffer = &scene.storage.pAnimAtG;
          pLight   = &scene.storage.pAnimAtLt;
          pShadow  = &scene.storage.pAnimAtSh;
          } else {
          pMain    = &scene.storage.pObjectAt;
          pGbuffer = &scene.storage.pObjectAtG;
          pLight   = &scene.storage.pObjectAtLt;
          pShadow  = &scene.storage.pObjectAtSh;
          }
        break;
      case Material::Transparent:
        if(shaderType==Animated) {
          pMain   = &scene.storage.pAnimAlpha;
          pLight  = nullptr;
          pShadow = nullptr;
          } else {
          pMain   = &scene.storage.pObjectAlpha;
          //pLight  = &scene.storage.pObjectLt;
          pShadow = &scene.storage.pObjectAtSh;
          }
        break;
      case Material::AdditiveLight: {
        if(shaderType==Animated) {
          pMain   = &scene.storage.pAnimMAdd;
          pLight  = nullptr;
          pShadow = nullptr;
          } else {
          pMain   = &scene.storage.pObjectMAdd;
          pLight  = nullptr;
          pShadow = nullptr;
          }
        break;
        }
      case Material::Multiply:
      case Material::Multiply2:
      case Material::Solid:
        if(shaderType==Animated) {
          pMain    = &scene.storage.pAnim;
          pLight   = &scene.storage.pAnimLt;
          pShadow  = &scene.storage.pAnimSh;
          pGbuffer = &scene.storage.pAnimG;
          } else {
          pMain    = &scene.storage.pObject;
          pLight   = &scene.storage.pObjectLt;
          pShadow  = &scene.storage.pObjectSh;
          pGbuffer = &scene.storage.pObjectG;
          }
        break;
      case Material::Water:{
        if(shaderType==Animated)
          pMain = &scene.storage.pAnimWater; else
          pMain = &scene.storage.pObjectWater;
        }
        break;
      case Material::InvalidAlpha:
      case Material::LastGothic:
      case Material::FirstOpenGothic:
      case Material::Last:
        break;
      }
    }

  if(mat.frames.size()>0)
    useSharedUbo = false;

  textureInShadowPass = (pShadow==&scene.storage.pObjectAtSh || pShadow==&scene.storage.pAnimAtSh);

  for(auto& i:uboMat) {
    UboMaterial zero;
    i = scene.storage.device.ubo<UboMaterial>(&zero,1);
    }

  if(useSharedUbo) {
    uboShared.invalidate();
    uboShared.alloc(*this);
    uboSetCommon(uboShared);
    }
  }

ObjectsBucket::~ObjectsBucket() {
  }

const Material& ObjectsBucket::material() const {
  return mat;
  }

ObjectsBucket::Object& ObjectsBucket::implAlloc(const VboType type, const Bounds& bounds) {
  Object* v = nullptr;
  for(size_t i=0; i<CAPACITY; ++i) {
    auto& vx = val[i];
    if(vx.isValid())
      continue;
    v = &vx;
    if(valLast<=i)
      valLast = i+1;
    break;
    }

  ++valSz;
  v->vboType   = type;
  v->vbo       = nullptr;
  v->vboA      = nullptr;
  v->ibo       = nullptr;
  v->bounds    = bounds;
  v->timeShift = uint64_t(0-scene.tickCount);

  if(!useSharedUbo) {
    v->ubo.invalidate();
    if(v->ubo.ubo[0].isEmpty()) {
      v->ubo.alloc(*this);
      uboSetCommon(v->ubo);
      }
    }
  return *v;
  }

void ObjectsBucket::uboSetCommon(Descriptors& v) {
  for(size_t i=0;i<Resources::MaxFramesInFlight;++i) {
    auto& t   = *mat.tex;
    auto& ubo = v.ubo[i];

    if(!ubo.isEmpty()) {
      ubo.set(0,t);
      ubo.set(1,*scene.shadowMap,Resources::shadowSampler());
      ubo.set(2,scene.uboGlobalPf[i][0]);
      ubo.set(4,uboMat[i]);
      if(isSceneInfoRequired()) {
        ubo.set(5,*scene.lightingBuf,Sampler2d::nearest());
        ubo.set(6,*scene.gbufDepth,  Sampler2d::nearest());
        }
      }

    for(size_t lay=0;lay<Resources::ShadowLayers;++lay) {
      auto& uboSh = v.uboSh[i][lay];
      if(uboSh.isEmpty())
        continue;

      if(textureInShadowPass)
        uboSh.set(0,t);
      uboSh.set(2,scene.uboGlobalPf[i][lay]);
      uboSh.set(4,uboMat[i]);
      }
    }
  }

void ObjectsBucket::uboSetDynamic(Object& v, uint8_t fId) {
  auto& ubo = v.ubo.ubo[fId];

  if(mat.frames.size()!=0) {
    auto frame = size_t((v.timeShift+scene.tickCount)/mat.texAniFPSInv);
    auto t = mat.frames[frame%mat.frames.size()];
    ubo.set(0,*t);
    if(pShadow!=nullptr && textureInShadowPass) {
      for(size_t lay=0;lay<Resources::ShadowLayers;++lay) {
        auto& uboSh = v.ubo.uboSh[fId][lay];
        uboSh.set(0,*t);
        }
      }
    }

  if(v.ubo.uboIsReady[fId])
    return;
  v.ubo.uboIsReady[fId] = true;
  if(v.storageAni!=size_t(-1)) {
    storage.ani.bind(ubo,3,fId,v.storageAni,boneCnt);
    if(pShadow!=nullptr) {
      for(size_t lay=0;lay<Resources::ShadowLayers;++lay) {
        auto& uboSh = v.ubo.uboSh[fId][lay];
        storage.ani.bind(uboSh,3,fId,v.storageAni,boneCnt);
        }
      }
    }
  }

void ObjectsBucket::setupUbo() {
  for(auto& v:val) {
    setupLights(v,true);
    }

  if(useSharedUbo) {
    uboShared.invalidate();
    uboSetCommon(uboShared);
    } else {
    for(auto& i:val) {
      i.ubo.invalidate();
      if(!i.ubo.ubo[0].isEmpty())
        uboSetCommon(i.ubo);
      }
    }
  }

void ObjectsBucket::invalidateUbo() {
  if(useSharedUbo) {
    uboShared.invalidate();
    } else {
    for(auto& i:val)
      i.ubo.invalidate();
    }
  }

void ObjectsBucket::preFrameUpdate(uint8_t fId) {
  UboMaterial ubo;
  if(mat.texAniMapDirPeriod.x!=0)
    ubo.texAniMapDir.x = float(scene.tickCount%std::abs(mat.texAniMapDirPeriod.x))/float(mat.texAniMapDirPeriod.x);
  if(mat.texAniMapDirPeriod.y!=0)
    ubo.texAniMapDir.y = float(scene.tickCount%std::abs(mat.texAniMapDirPeriod.y))/float(mat.texAniMapDirPeriod.y);

  if(mat.texAniMapDirPeriod.x!=0 || mat.texAniMapDirPeriod.y!=0)
    uboMat[fId].update(&ubo,0,1);

  if(mat.frames.size()>0) {
    //texAnim;
    }
  }

bool ObjectsBucket::groupVisibility(Painter3d& p) {
  if(shaderType!=Static)
    return true;

  if(allBounds.r<=0) {
    Tempest::Vec3 bbox[2] = {};
    bool          fisrt=true;
    for(size_t i=0;i<CAPACITY;++i) {
      if(!val[i].isValid())
        continue;
      auto& b = val[i].bounds;
      if(fisrt) {
        bbox[0] = b.bboxTr[0];
        bbox[1] = b.bboxTr[1];
        fisrt = false;
        }
      bbox[0].x = std::min(bbox[0].x,b.bboxTr[0].x);
      bbox[0].y = std::min(bbox[0].y,b.bboxTr[0].y);
      bbox[0].z = std::min(bbox[0].z,b.bboxTr[0].z);

      bbox[1].x = std::max(bbox[1].x,b.bboxTr[1].x);
      bbox[1].y = std::max(bbox[1].y,b.bboxTr[1].y);
      bbox[1].z = std::max(bbox[1].z,b.bboxTr[1].z);
      }
    allBounds.assign(bbox);
    }
  return p.isVisible(allBounds);
  }

void ObjectsBucket::visibilityPass(Painter3d& p) {
  indexSz = 0;
  if(!groupVisibility(p))
    return;

  Object** idx = index;
  for(size_t i=0; i<valLast; ++i) {
    auto& v = val[i];
    if(!v.isValid())
      continue;
    if(!p.isVisible(v.bounds) && v.vboType!=VboType::VboMorph)
      continue;
    idx[indexSz] = &v;
    ++indexSz;
    }
  }

void ObjectsBucket::visibilityPassAnd(Painter3d& p) {
  size_t nextSz = 0;
  for(size_t i=0; i<indexSz; ++i) {
    auto& v = *index[i];
    if(!p.isVisible(v.bounds))
      continue;
    index[nextSz] = &v;
    ++nextSz;
    }
  indexSz = nextSz;
  }

size_t ObjectsBucket::alloc(const Tempest::VertexBuffer<Vertex>&  vbo,
                            const Tempest::IndexBuffer<uint32_t>& ibo,
                            const Bounds& bounds) {
  Object* v = &implAlloc(VboType::VboVertex,bounds);
  v->vbo = &vbo;
  v->ibo = &ibo;
  polySz+=ibo.size();
  polyAvg = polySz/valSz;
  return std::distance(val,v);
  }

size_t ObjectsBucket::alloc(const Tempest::VertexBuffer<VertexA>& vbo,
                            const Tempest::IndexBuffer<uint32_t>& ibo,
                            const Bounds& bounds) {
  Object* v = &implAlloc(VboType::VboVertexA,bounds);
  v->vboA   = &vbo;
  v->ibo    = &ibo;
  v->storageAni = storage.ani.alloc(boneCnt);
  polySz+=ibo.size();
  polyAvg = polySz/valSz;
  return std::distance(val,v);
  }

size_t ObjectsBucket::alloc(const Tempest::VertexBuffer<ObjectsBucket::Vertex>* vbo[], const Bounds& bounds) {
  Object* v = &implAlloc(VboType::VboMorph,bounds);
  for(size_t i=0; i<Resources::MaxFramesInFlight; ++i)
    v->vboM[i] = vbo[i];
  return std::distance(val,v);
  }

void ObjectsBucket::free(const size_t objId) {
  auto& v = val[objId];
  if(v.storageAni!=size_t(-1))
    storage.ani.free(v.storageAni,boneCnt);
  if(v.ibo!=nullptr)
    polySz -= v.ibo->size();
  v.vboType = VboType::NoVbo;
  v.vbo     = nullptr;
  for(size_t i=0;i<Resources::MaxFramesInFlight;++i)
    v.vboM[i] = nullptr;
  v.vboA    = nullptr;
  v.ibo     = nullptr;
  valSz--;
  valLast = 0;
  for(size_t i=CAPACITY; i>0;) {
    --i;
    if(val[i].isValid()) {
      valLast = i+1;
      break;
      }
    }
  if(valSz>0)
    polyAvg = polySz/valSz; else
    polyAvg = 0;
  }

void ObjectsBucket::draw(Tempest::Encoder<Tempest::CommandBuffer>& p, uint8_t fId) {
  if(pMain==nullptr || indexSz==0)
    return;

  if(useSharedUbo)
    p.setUniforms(*pMain,uboShared.ubo[fId]);

  UboPush pushBlock;
  Object** idx = index;
  for(size_t i=0;i<indexSz;++i) {
    auto& v = *idx[i];

    pushBlock.pos = v.pos;
    const size_t cnt = v.lightCnt;
    for(size_t r=0; r<cnt && r<LIGHT_BLOCK; ++r) {
      pushBlock.light[r].pos   = v.light[r]->position();
      pushBlock.light[r].color = v.light[r]->currentColor();
      pushBlock.light[r].range = v.light[r]->currentRange();
      }
    for(size_t r=cnt;r<LIGHT_BLOCK;++r) {
      pushBlock.light[r].range = 0;
      }

    p.setUniforms(*pMain,&pushBlock,sizeof(pushBlock));
    if(!useSharedUbo) {
      uboSetDynamic(v,fId);
      p.setUniforms(*pMain,v.ubo.ubo[fId]);
      }

    switch(v.vboType) {
      case VboType::NoVbo:
        break;
      case VboType::VboVertex:
        p.draw(*v.vbo, *v.ibo);
        break;
      case VboType::VboVertexA:
        p.draw(*v.vboA,*v.ibo);
        break;
      case VboType::VboMorph:
        p.draw(*v.vboM[fId]);
        break;
      }
    }
  }

void ObjectsBucket::drawGBuffer(Tempest::Encoder<CommandBuffer>& p, uint8_t fId) {
  if(pGbuffer==nullptr || indexSz==0)
    return;

  if(useSharedUbo)
    p.setUniforms(*pGbuffer,uboShared.ubo[fId]);

  UboPush pushBlock;
  Object** idx = index;
  for(size_t i=0;i<indexSz;++i) {
    auto& v = *idx[i];

    pushBlock.pos = v.pos;
    const size_t cnt = v.lightCnt;
    for(size_t r=0; r<cnt && r<LIGHT_BLOCK; ++r) {
      pushBlock.light[r].pos   = v.light[r]->position();
      pushBlock.light[r].color = v.light[r]->currentColor();
      pushBlock.light[r].range = v.light[r]->currentRange();
      }
    for(size_t r=cnt;r<LIGHT_BLOCK;++r) {
      pushBlock.light[r].range = 0;
      }

    p.setUniforms(*pGbuffer,&pushBlock,sizeof(pushBlock));
    if(!useSharedUbo) {
      uboSetDynamic(v,fId);
      p.setUniforms(*pGbuffer,v.ubo.ubo[fId]);
      }

    switch(v.vboType) {
      case VboType::NoVbo:
        break;
      case VboType::VboVertex:
        p.draw(*v.vbo, *v.ibo);
        break;
      case VboType::VboVertexA:
        p.draw(*v.vboA,*v.ibo);
        break;
      case VboType::VboMorph:
        p.draw(*v.vboM[fId]);
        break;
      }
    }
  }

void ObjectsBucket::drawLight(Tempest::Encoder<Tempest::CommandBuffer>& p, uint8_t fId) {
  static bool disabled = false;
  if(disabled)
    return;

  if(pLight==nullptr || indexSz==0)
    return;

  if(useSharedUbo)
    p.setUniforms(*pLight,uboShared.ubo[fId]);

  UboPush pushBlock;
  Object** idx = index;
  for(size_t i=0;i<indexSz;++i) {
    auto& v = *idx[i];
    if(v.lightCnt<=LIGHT_BLOCK)
      continue;

    if(!useSharedUbo) {
      p.setUniforms(*pLight,v.ubo.ubo[fId]);
      }
    pushBlock.pos = v.pos;

    for(size_t i=LIGHT_BLOCK; i<v.lightCnt; i+=LIGHT_BLOCK) {
      const size_t cnt = v.lightCnt-i;
      for(size_t r=0; r<cnt && r<LIGHT_BLOCK; ++r) {
        pushBlock.light[r].pos   = v.light[i+r]->position();
        pushBlock.light[r].color = v.light[i+r]->color();
        pushBlock.light[r].range = v.light[i+r]->range();
        }
      for(size_t r=cnt;r<LIGHT_BLOCK;++r) {
        pushBlock.light[r].range = 0;
        }
      p.setUniforms(*pLight,&pushBlock,sizeof(pushBlock));
      switch(v.vboType) {
        case VboType::NoVbo:
          break;
        case VboType::VboVertex:
          p.draw(*v.vbo, *v.ibo);
          break;
        case VboType::VboVertexA:
          p.draw(*v.vboA,*v.ibo);
          break;
        case VboType::VboMorph:
          p.draw(*v.vboM[fId]);
          break;
        }
      }
    }
  }

void ObjectsBucket::drawShadow(Tempest::Encoder<Tempest::CommandBuffer>& p, uint8_t fId, int layer) {
  if(pShadow==nullptr || indexSz==0)
    return;

  UboPush pushBlock = {};
  if(useSharedUbo)
    p.setUniforms(*pShadow,uboShared.uboSh[fId][layer]);

  Object** idx = index;
  for(size_t i=0;i<indexSz;++i) {
    auto& v = *idx[i];

    if(!useSharedUbo) {
      uboSetDynamic(v,fId);
      p.setUniforms(*pShadow, v.ubo.uboSh[fId][layer]);
      }

    pushBlock.pos = v.pos;
    p.setUniforms(*pShadow,&pushBlock,sizeof(pushBlock));
    switch(v.vboType) {
      case VboType::NoVbo:
        break;
      case VboType::VboVertex:
        p.draw(*v.vbo, *v.ibo);
        break;
      case VboType::VboVertexA:
        p.draw(*v.vboA,*v.ibo);
        break;
      case VboType::VboMorph:
        p.draw(*v.vboM[fId]);
        break;
      }
    }
  }

void ObjectsBucket::draw(size_t id, Tempest::Encoder<Tempest::CommandBuffer>& p, uint8_t fId) {
  auto& v = val[id];
  if(v.vbo==nullptr || pMain==nullptr)
    return;

  storage.commitUbo(scene.storage.device,fId);

  UboPush pushBlock = {};
  pushBlock.pos = v.pos;

  auto& ubo = useSharedUbo ? uboShared.ubo[fId] : v.ubo.ubo[fId];
  if(!useSharedUbo) {
    ubo.set(0,*mat.tex);
    ubo.set(1,Resources::fallbackTexture(),Sampler2d::nearest());
    ubo.set(2,scene.uboGlobalPf[fId][0]);
    ubo.set(4,uboMat[fId]);
    }

  p.setUniforms(*pMain,ubo);
  p.setUniforms(*pMain,&pushBlock,sizeof(pushBlock));
  p.draw(*v.vbo, *v.ibo);
  }

void ObjectsBucket::setObjMatrix(size_t i, const Matrix4x4& m) {
  auto& v = val[i];
  v.bounds.setObjMatrix(m);
  v.pos = m;

  if(shaderType==Static)
    allBounds.r = 0;

  setupLights(v,false);
  }

void ObjectsBucket::setPose(size_t i, const Pose& p) {
  if(shaderType!=Animated)
    return;
  auto& v       = val[i];

  auto& skel = storage.ani.element(v.storageAni);
  auto& tr = p.transform();
  std::memcpy(&skel,tr.data(),std::min(tr.size(),boneCnt)*sizeof(tr[0]));
  storage.ani.markAsChanged(v.storageAni);
  }

void ObjectsBucket::setBounds(size_t i, const Bounds& b) {
  val[i].bounds = b;
  }

bool ObjectsBucket::isSceneInfoRequired() const {
  return pMain==&scene.storage.pObjectGhost ||
         pMain==&scene.storage.pAnimGhost   ||
         pMain==&scene.storage.pObjectWater ||
         pMain==&scene.storage.pAnimWater;
  }

const Bounds& ObjectsBucket::bounds(size_t i) const {
  return val[i].bounds;
  }

void ObjectsBucket::setupLights(Object& val, bool noCache) {
  if(pGbuffer!=nullptr)
    return;
  int cx = int(val.bounds.midTr.x/2.f);
  int cy = int(val.bounds.midTr.y/2.f);
  int cz = int(val.bounds.midTr.z/2.f);

  if(cx==val.lightCacheKey[0] &&
     cy==val.lightCacheKey[1] &&
     cz==val.lightCacheKey[2] &&
     !noCache)
    return;

  val.lightCacheKey[0] = cx;
  val.lightCacheKey[1] = cy;
  val.lightCacheKey[2] = cz;

  val.lightCnt = scene.lights.get(val.bounds,val.light,MAX_LIGHT);
  }

bool ObjectsBucket::Storage::commitUbo(Device& device, uint8_t fId) {
  return ani.commitUbo(device,fId) | mat.commitUbo(device,fId);
  }
