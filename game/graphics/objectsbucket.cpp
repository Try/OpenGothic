#include "objectsbucket.h"

#include <Tempest/Log>

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

void ObjectsBucket::Item::setAsGhost(bool g) {
  if(owner->mat.isGhost==g)
    return;

  auto m = owner->mat;
  m.isGhost = g;
  auto&  bucket = owner->owner.getBucket(m,{},owner->shaderType);

  auto&  v      = owner->val[id];
  size_t idNext = size_t(-1);
  switch(v.vboType) {
    case NoVbo:
      break;
    case VboVertex:{
      idNext = bucket.alloc(*v.vbo,*v.ibo,v.iboOffset,v.iboLength,v.visibility.bounds());
      break;
      }
    case VboVertexA:{
      idNext = bucket.alloc(*v.vboA,*v.ibo,v.iboOffset,v.iboLength,*v.skiningAni,v.visibility.bounds());
      break;
      }
    case VboMorph:{
      idNext = bucket.alloc(v.vboM,v.visibility.bounds());
      break;
      }
    case VboMorpthGpu:{
      idNext = bucket.alloc(*v.vbo,*v.ibo,v.iboOffset,v.iboLength,v.visibility.bounds());
      break;
      }
    }
  if(idNext==size_t(-1))
    return;

  auto oldId = id;
  auto oldOw = owner;
  owner = &bucket;
  id    = idNext;

  auto& v2 = owner->val[id];
  setObjMatrix(v.pos);
  std::swap(v.timeShift, v2.timeShift);

  oldOw->free(oldId);
  }

void ObjectsBucket::Item::startMMAnim(const char* anim, float intensity, uint64_t timeUntil) {
  if(owner!=nullptr)
    owner->startMMAnim(id,anim,intensity,timeUntil);
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
  auto& device = Resources::device();
  for(size_t i=0;i<Resources::MaxFramesInFlight;++i) {
    if(owner.pMain!=nullptr)
      ubo[i][SceneGlobals::V_Main] = device.descriptors(owner.pMain->layout());
    if(owner.pShadow!=nullptr) {
      for(size_t lay=SceneGlobals::V_Shadow0; lay<=SceneGlobals::V_ShadowLast; ++lay)
        ubo[i][lay] = device.descriptors(owner.pShadow->layout());
      }
    }
  }

ObjectsBucket::ObjectsBucket(const Material& mat, const ProtoMesh* anim,
                             VisualObjects& owner, const SceneGlobals& scene, Storage& storage, const Type type)
  :owner(owner), scene(scene), storage(storage), mat(mat), shaderType(type) {
  static_assert(sizeof(UboPush)<=128, "UboPush is way too big");
  auto& device = Resources::device();

  auto st = shaderType;
  if(anim!=nullptr && anim->morph.size()>0) {
    morphAnim = anim;
    st        = Morph;
    }

  pMain    = scene.storage.materialPipeline(mat,st,RendererStorage::T_Forward );
  pGbuffer = scene.storage.materialPipeline(mat,st,RendererStorage::T_Deffered);
  pShadow  = scene.storage.materialPipeline(mat,st,RendererStorage::T_Shadow  );

  if(mat.frames.size()>0 || type==Animated)
    useSharedUbo = false; else
    useSharedUbo = true;

  textureInShadowPass = (mat.alpha==Material::AlphaTest);

  for(auto& i:uboMat) {
    UboMaterial zero;
    i = device.ubo<UboMaterial>(&zero,1);
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

  if(valSz==0)
    owner.resetIndex();

  ++valSz;
  v->vboType    = type;
  v->vbo        = nullptr;
  v->vboA       = nullptr;
  v->ibo        = nullptr;
  v->timeShift  = uint64_t(0-scene.tickCount);
  v->visibility = owner.visGroup.get();
  v->visibility.setBounds(bounds);

  if(!useSharedUbo) {
    v->ubo.invalidate();
    if(v->ubo.ubo[0][SceneGlobals::V_Main].isEmpty()) {
      v->ubo.alloc(*this);
      uboSetCommon(v->ubo);
      }
    }
  return *v;
  }

void ObjectsBucket::uboSetCommon(Descriptors& v) {
  for(size_t i=0; i<Resources::MaxFramesInFlight; ++i) {
    auto& t   = *mat.tex;
    auto& ubo = v.ubo[i][SceneGlobals::V_Main];

    if(!ubo.isEmpty()) {
      ubo.set(L_Diffuse, t);
      ubo.set(L_Shadow0, *scene.shadowMap[0],Resources::shadowSampler());
      ubo.set(L_Shadow1, *scene.shadowMap[1],Resources::shadowSampler());
      ubo.set(L_Scene,   scene.uboGlobalPf[i][SceneGlobals::V_Main]);
      if(shaderType!=ObjectsBucket::Pfx)
        ubo.set(L_Material,uboMat[i]);
      if(isSceneInfoRequired()) {
        ubo.set(L_GDiffuse, *scene.lightingBuf,Sampler2d::nearest());
        ubo.set(L_GDepth,   *scene.gbufDepth,  Sampler2d::nearest());
        }
      if(morphAnim!=nullptr) {
        ubo.set(L_MorphId, morphAnim->morphIndex  );
        ubo.set(L_Morph,   morphAnim->morphSamples);
        }
      }

    for(size_t lay=SceneGlobals::V_Shadow0; lay<=SceneGlobals::V_ShadowLast; ++lay) {
      auto& uboSh = v.ubo[i][lay];
      if(uboSh.isEmpty())
        continue;

      if(textureInShadowPass)
        uboSh.set(L_Diffuse, t);
      uboSh.set(L_Scene,    scene.uboGlobalPf[i][lay]);
      if(shaderType!=ObjectsBucket::Pfx)
        uboSh.set(L_Material, uboMat[i]);
      if(morphAnim!=nullptr) {
        uboSh.set(L_MorphId, morphAnim->morphIndex  );
        uboSh.set(L_Morph,   morphAnim->morphSamples);
        }
      }
    }
  }

void ObjectsBucket::uboSetDynamic(Object& v, uint8_t fId) {
  auto& ubo = v.ubo.ubo[fId][SceneGlobals::V_Main];

  if(mat.frames.size()!=0) {
    auto frame = size_t((v.timeShift+scene.tickCount)/mat.texAniFPSInv);
    auto t = mat.frames[frame%mat.frames.size()];
    ubo.set(L_Diffuse, *t);
    if(pShadow!=nullptr && textureInShadowPass) {
      for(size_t lay=SceneGlobals::V_Shadow0; lay<=SceneGlobals::V_ShadowLast; ++lay) {
        auto& uboSh = v.ubo.ubo[fId][lay];
        uboSh.set(L_Diffuse, *t);
        }
      }
    }

  if(v.ubo.uboIsReady[fId])
    return;
  v.ubo.uboIsReady[fId] = true;
  if(v.skiningAni!=nullptr) {
    v.skiningAni->bind(ubo,L_Skinning,fId);
    if(pShadow!=nullptr) {
      for(size_t lay=SceneGlobals::V_Shadow0; lay<=SceneGlobals::V_ShadowLast; ++lay) {
        auto& uboSh = v.ubo.ubo[fId][lay];
        v.skiningAni->bind(uboSh,L_Skinning,fId);
        }
      }
    }
  }

void ObjectsBucket::setupUbo() {
  if(useSharedUbo) {
    uboShared.invalidate();
    uboSetCommon(uboShared);
    } else {
    for(auto& i:val) {
      i.ubo.invalidate();
      if(!i.ubo.ubo[0][SceneGlobals::V_Main].isEmpty())
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
  if(mat.texAniMapDirPeriod.x==0 && mat.texAniMapDirPeriod.y==0)
    return;

  UboMaterial ubo;
  if(mat.texAniMapDirPeriod.x!=0) {
    uint64_t fract = scene.tickCount%uint64_t(std::abs(mat.texAniMapDirPeriod.x));
    ubo.texAniMapDir.x = float(fract)/float(mat.texAniMapDirPeriod.x);
    }
  if(mat.texAniMapDirPeriod.y!=0) {
    uint64_t fract = scene.tickCount%uint64_t(std::abs(mat.texAniMapDirPeriod.y));
    ubo.texAniMapDir.y = float(fract)/float(mat.texAniMapDirPeriod.y);
    }

  uboMat[fId].update(&ubo,0,1);
  }

bool ObjectsBucket::groupVisibility(const Frustrum& f) {
  if(shaderType!=Static)
    return true;

  if(allBounds.r<=0) {
    Tempest::Vec3 bbox[2] = {};
    bool          fisrt=true;
    for(size_t i=0;i<CAPACITY;++i) {
      if(!val[i].isValid())
        continue;
      auto& b = val[i].visibility.bounds();
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
  return f.testPoint(allBounds.midTr,allBounds.r);
  }

size_t ObjectsBucket::alloc(const Tempest::VertexBuffer<Vertex>&  vbo,
                            const Tempest::IndexBuffer<uint32_t>& ibo,
                            size_t iboOffset, size_t iboLen,
                            const Bounds& bounds) {
  Object* v = &implAlloc(VboType::VboVertex,bounds);
  v->vbo       = &vbo;
  v->ibo       = &ibo;
  v->iboOffset = iboOffset;
  v->iboLength = iboLen;
  return size_t(std::distance(val,v));
  }

size_t ObjectsBucket::alloc(const Tempest::VertexBuffer<VertexA>& vbo,
                            const Tempest::IndexBuffer<uint32_t>& ibo,
                            size_t iboOffset, size_t iboLen,
                            const SkeletalStorage::AnimationId& anim,
                            const Bounds& bounds) {
  Object* v = &implAlloc(VboType::VboVertexA,bounds);
  v->vboA      = &vbo;
  v->ibo       = &ibo;
  v->iboOffset = iboOffset;
  v->iboLength = iboLen;

  v->skiningAni = &anim;//storage.ani.alloc(boneCnt);
  return size_t(std::distance(val,v));
  }

size_t ObjectsBucket::alloc(const Tempest::VertexBuffer<ObjectsBucket::Vertex>* vbo[], const Bounds& bounds) {
  Object* v = &implAlloc(VboType::VboMorph,bounds);
  for(size_t i=0; i<Resources::MaxFramesInFlight; ++i)
    v->vboM[i] = vbo[i];
  return size_t(std::distance(val,v));
  }

void ObjectsBucket::free(const size_t objId) {
  auto& v = val[objId];
  v.visibility = VisibilityGroup::Token();
  v.vboType    = VboType::NoVbo;
  v.vbo        = nullptr;
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

  if(valSz==0)
    owner.resetIndex();
  }

void ObjectsBucket::draw(Encoder<CommandBuffer>& cmd, uint8_t fId) {
  if(pMain==nullptr)
    return;
  drawCommon(cmd,fId,*pMain,SceneGlobals::V_Main);
  }

void ObjectsBucket::drawGBuffer(Encoder<CommandBuffer>& cmd, uint8_t fId) {
  if(pGbuffer==nullptr)
    return;
  drawCommon(cmd,fId,*pGbuffer,SceneGlobals::V_Main);
  }

void ObjectsBucket::drawShadow(Encoder<CommandBuffer>& cmd, uint8_t fId, int layer) {
  if(pShadow==nullptr)
    return;
  drawCommon(cmd,fId,*pShadow,SceneGlobals::VisCamera(SceneGlobals::V_Shadow0+layer));
  }

void ObjectsBucket::drawCommon(Encoder<CommandBuffer>& cmd, uint8_t fId,
                               const RenderPipeline& shader, SceneGlobals::VisCamera c) {
  UboPush pushBlock = {};
  bool    sharedSet = false;

  size_t pushSz = (morphAnim!=nullptr) ? sizeof(pushBlock) : sizeof(Tempest::Matrix4x4);
  if(shaderType==Pfx)
    pushSz = 0;

  for(size_t i=0; i<valLast; ++i) {
    auto& v = val[i];
    if(v.vboType==NoVbo)
      continue;
    if(v.vboType!=VboMorph && !v.visibility.isVisible(c))
      continue;

    updatePushBlock(pushBlock,v);
    if(!useSharedUbo) {
      uboSetDynamic(v,fId);
      cmd.setUniforms(shader, v.ubo.ubo[fId][c], &pushBlock, pushSz);
      }
    else if(!sharedSet) {
      sharedSet = true;
      cmd.setUniforms(shader, uboShared.ubo[fId][c], &pushBlock, pushSz);
      }
    else {
      cmd.setUniforms(shader, &pushBlock, pushSz);
      }

    switch(v.vboType) {
      case VboType::NoVbo:
        break;
      case VboType::VboVertex:
        cmd.draw(*v.vbo, *v.ibo, v.iboOffset, v.iboLength);
        break;
      case VboType::VboVertexA:
        cmd.draw(*v.vboA,*v.ibo, v.iboOffset, v.iboLength);
        break;
      case VboType::VboMorph:
        cmd.draw(*v.vboM[fId]);
        break;
      case VboType::VboMorpthGpu:
        cmd.draw(*v.vbo, *v.ibo, v.iboOffset, v.iboLength);
        break;
      }
    }
  }

void ObjectsBucket::draw(size_t id, Tempest::Encoder<Tempest::CommandBuffer>& p, uint8_t fId) {
  auto& v = val[id];
  if(v.vbo==nullptr || pMain==nullptr)
    return;

  storage.commitUbo(fId);

  UboPush pushBlock = {};
  updatePushBlock(pushBlock,v);

  auto& ubo = (useSharedUbo ? uboShared.ubo[fId] : v.ubo.ubo[fId])[SceneGlobals::V_Main];
  if(!useSharedUbo) {
    ubo.set(L_Diffuse,  *mat.tex);
    ubo.set(L_Shadow0,  Resources::fallbackTexture(),Sampler2d::nearest());
    ubo.set(L_Shadow1,  Resources::fallbackTexture(),Sampler2d::nearest());
    ubo.set(L_Scene,    scene.uboGlobalPf[fId][SceneGlobals::V_Main]);
    ubo.set(L_Material, uboMat[fId]);
    }

  p.setUniforms(*pMain,ubo,&pushBlock,sizeof(pushBlock));
  switch(v.vboType) {
    case VboType::NoVbo:
      break;
    case VboType::VboVertex:
      p.draw(*v.vbo, *v.ibo, v.iboOffset, v.iboLength);
      break;
    case VboType::VboVertexA:
      p.draw(*v.vboA,*v.ibo, v.iboOffset, v.iboLength);
      break;
    case VboType::VboMorph:
      p.draw(*v.vboM[fId]);
      break;
    case VboType::VboMorpthGpu:
      p.draw(*v.vbo, *v.ibo, v.iboOffset, v.iboLength);
      break;
    }
  }

void ObjectsBucket::setObjMatrix(size_t i, const Matrix4x4& m) {
  auto& v = val[i];
  v.visibility.setObjMatrix(m);
  v.pos = m;

  if(shaderType==Static)
    allBounds.r = 0;
  }

void ObjectsBucket::setBounds(size_t i, const Bounds& b) {
  val[i].visibility.setBounds(b);
  }

void ObjectsBucket::startMMAnim(size_t i, const char* anim, float intensity, uint64_t timeUntil) {
  if(morphAnim==nullptr)
    return;
  auto&  v  = val[i];
  size_t id = size_t(-1);
  for(size_t i=0; i<morphAnim->morph.size(); ++i)
    if(morphAnim->morph[i].name==anim) {
      id = i;
      break;
      }

  if(id==size_t(-1))
    return;

  auto& m = morphAnim->morph[id];
  if(timeUntil==uint64_t(-1) && m.duration>0)
    timeUntil = scene.tickCount + m.duration;

  // extend time of anim
  for(auto& i:v.morphAnim) {
    if(i.id!=id || i.timeUntil<scene.tickCount)
      continue;
    i.timeUntil = timeUntil;
    i.intensity = intensity;
    return;
    }

  // find same layer
  for(auto& i:v.morphAnim) {
    if(i.timeUntil<scene.tickCount)
      continue;
    if(morphAnim->morph[i.id].layer!=m.layer)
      continue;
    i.id        = id;
    i.timeUntil = timeUntil;
    i.intensity = intensity;
    return;
    }

  size_t nId = 0;
  for(size_t i=0; i<Resources::MAX_MORPH_LAYERS; ++i) {
    if(v.morphAnim[nId].timeStart<=v.morphAnim[i].timeStart)
      continue;
    nId = i;
    }

  auto& ani = v.morphAnim[nId];
  ani.id        = id;
  ani.timeStart = scene.tickCount;
  ani.timeUntil = timeUntil;
  ani.intensity = intensity;
  }

bool ObjectsBucket::isSceneInfoRequired() const {
  return mat.isGhost || mat.alpha==Material::Water || mat.alpha==Material::Ghost;
  }

void ObjectsBucket::updatePushBlock(ObjectsBucket::UboPush& push, ObjectsBucket::Object& v) {
  push.pos = v.pos;
  if(morphAnim!=nullptr) {
    for(size_t i=0; i<Resources::MAX_MORPH_LAYERS; ++i) {
      auto&    ani  = v.morphAnim[i];
      auto&    anim = morphAnim->morph[ani.id];
      uint64_t time = (scene.tickCount-ani.timeStart);

      float alpha     = float(time%anim.tickPerFrame)/float(anim.tickPerFrame);
      float intensity = ani.intensity;

      if(scene.tickCount>ani.timeUntil)
        intensity = 0;

      const uint32_t samplesPerFrame = uint32_t(anim.samplesPerFrame);
      push.morph[i].indexOffset = uint32_t(anim.index);
      push.morph[i].sample0     = uint32_t((time/anim.tickPerFrame+0)%anim.numFrames)*samplesPerFrame;
      push.morph[i].sample1     = uint32_t((time/anim.tickPerFrame+1)%anim.numFrames)*samplesPerFrame;
      push.morph[i].alpha       = alpha + std::floor(intensity*255);
      }
    }
  }

const Bounds& ObjectsBucket::bounds(size_t i) const {
  return val[i].visibility.bounds();
  }

bool ObjectsBucket::Storage::commitUbo(uint8_t fId) {
  return mat.commitUbo(fId);
  }
