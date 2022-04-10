#include "objectsbucket.h"

#include <Tempest/Log>

#include "graphics/mesh/pose.h"
#include "graphics/mesh/skeleton.h"
#include "sceneglobals.h"

#include "utils/workers.h"
#include "visualobjects.h"
#include "shaders.h"

using namespace Tempest;

void ObjectsBucket::Item::setObjMatrix(const Tempest::Matrix4x4 &mt) {
  owner->setObjMatrix(id,mt);
  }

void ObjectsBucket::Item::setAsGhost(bool g) {
  if(owner->mat.isGhost==g)
    return;

  auto m = owner->mat;
  m.isGhost = g;
  auto&  bucket = owner->owner.getBucket(m,{},owner->objType);

  auto&  v      = owner->val[id];
  size_t idNext = size_t(-1);
  switch(owner->vboType) {
    case NoVbo:
      break;
    case VboVertex:{
      idNext = bucket.alloc(*v.vbo,*v.ibo,v.blas,v.iboOffset,v.iboLength,v.visibility.bounds());
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
    case VboMorphGpu:{
      idNext = bucket.alloc(*v.vbo,*v.ibo,v.blas,v.iboOffset,v.iboLength,v.visibility.bounds());
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

void ObjectsBucket::Item::setFatness(float f) {
  if(owner!=nullptr)
    owner->setFatness(id,f);
  }

void ObjectsBucket::Item::setWind(ZenLoad::AnimMode m, float intensity) {
  if(owner!=nullptr)
    owner->setWind(id,m,intensity);
  }

void ObjectsBucket::Item::startMMAnim(std::string_view anim, float intensity, uint64_t timeUntil) {
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


void ObjectsBucket::Descriptors::alloc(ObjectsBucket& owner) {
  auto& device = Resources::device();
  for(uint8_t i=0; i<Resources::MaxFramesInFlight; ++i) {
    if(owner.pMain!=nullptr)
      ubo[i][SceneGlobals::V_Main] = device.descriptors(owner.pMain->layout());
    if(owner.pShadow!=nullptr) {
      for(size_t lay=SceneGlobals::V_Shadow0; lay<=SceneGlobals::V_ShadowLast; ++lay)
        ubo[i][lay] = device.descriptors(owner.pShadow->layout());
      }
    }
  }


ObjectsBucket::VboType ObjectsBucket::toVboType(const Type type) {
  switch(type) {
    case Type::Landscape:
    case Type::Static:
    case Type::Movable:
      return VboType::VboVertex;

    case Type::Animated:
      return VboType::VboVertexA;

    case Type::Pfx:
      return VboType::VboMorph;

    case Type::Morph:
      return VboType::VboMorphGpu;
    }
  return VboType::NoVbo;
  }

ObjectsBucket::ObjectsBucket(const Material& mat, const ProtoMesh* anim,
                             VisualObjects& owner, const SceneGlobals& scene,
                             const Type type)
  :objType(type), vboType(toVboType(type)), owner(owner), scene(scene), mat(mat) {
  static_assert(sizeof(UboPush)<=128, "UboPush is way too big");
  auto& device = Resources::device();

  auto st = objType;
  if(anim!=nullptr && anim->morph.size()>0) {
    morphAnim = anim;
    st        = Morph;
    }

  pMain    = Shaders::inst().materialPipeline(mat,st,Shaders::T_Forward );
  pGbuffer = Shaders::inst().materialPipeline(mat,st,Shaders::T_Deffered);
  pShadow  = Shaders::inst().materialPipeline(mat,st,Shaders::T_Shadow  );

  if(mat.frames.size()>0 || type==Animated)
    useSharedUbo = false; else
    useSharedUbo = true;

  textureInShadowPass = (mat.alpha==Material::AlphaTest);

  for(auto& i:uboMat) {
    UboMaterial zero;
    i = device.ubo<UboMaterial>(&zero,1);
    }

  if(useSharedUbo) {
    uboShared.alloc(*this);
    uboSetCommon(uboShared);
    }
  }

ObjectsBucket::~ObjectsBucket() {
  }

std::unique_ptr<ObjectsBucket> ObjectsBucket::mkBucket(const Material& mat, const ProtoMesh* anim, VisualObjects& owner,
                                                       const SceneGlobals& scene, const Type type) {

  if(mat.frames.size()>0 || type==Animated)
    return std::unique_ptr<ObjectsBucket>(new ObjectsBucketDyn(mat,anim,owner,scene,type));
  if(type==Landscape)
    return std::unique_ptr<ObjectsBucket>(new ObjectsBucketLnd(mat,anim,owner,scene,type));
  return std::unique_ptr<ObjectsBucket>(new ObjectsBucket(mat,anim,owner,scene,type));
  }

const Material& ObjectsBucket::material() const {
  return mat;
  }

ObjectsBucket::Object& ObjectsBucket::implAlloc(const VboType type, const Bounds& bounds,
                                                const SkeletalStorage::AnimationId* anim) {
  Object* v = nullptr;
  for(size_t i=0; i<CAPACITY; ++i) {
    auto& vx = val[i];
    if(vx.isValid)
      continue;
    v = &vx;
    break;
    }

  if(valSz==0)
    owner.resetIndex();

  ++valSz;
  v->vbo        = nullptr;
  v->vboA       = nullptr;
  v->ibo        = nullptr;
  v->timeShift  = uint64_t(0-scene.tickCount);
  v->visibility = owner.visGroup.get();
  v->visibility.setBounds(bounds);
  v->visibility.setObject(&visSet,size_t(std::distance(val,v)));
  v->skiningAni = anim;
  v->isValid    = true;

  return *v;
  }

void ObjectsBucket::implFree(const size_t objId) {
  auto& v = val[objId];
  if(v.blas!=nullptr)
    owner.resetTlas();

  v.visibility = VisibilityGroup::Token();
  v.isValid    = false;
  v.vbo        = nullptr;
  for(size_t i=0;i<Resources::MaxFramesInFlight;++i)
    v.vboM[i] = nullptr;
  v.vboA    = nullptr;
  v.ibo     = nullptr;
  v.blas = nullptr;
  valSz--;
  visSet.erase(objId);

  if(valSz==0)
    owner.resetIndex();
  }

void ObjectsBucket::uboSetCommon(Descriptors& v) {
  for(uint8_t i=0; i<Resources::MaxFramesInFlight; ++i) {
    auto& t   = *mat.tex;
    auto& ubo = v.ubo[i][SceneGlobals::V_Main];

    if(!ubo.isEmpty()) {
      ubo.set(L_Diffuse, t);
      ubo.set(L_Shadow0, *scene.shadowMap[0],Resources::shadowSampler());
      ubo.set(L_Shadow1, *scene.shadowMap[1],Resources::shadowSampler());
      ubo.set(L_Scene,   scene.uboGlobalPf[i][SceneGlobals::V_Main]);
      if(objType!=ObjectsBucket::Pfx)
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
      if(objType!=ObjectsBucket::Pfx)
        uboSh.set(L_Material, uboMat[i]);
      if(morphAnim!=nullptr) {
        uboSh.set(L_MorphId, morphAnim->morphIndex  );
        uboSh.set(L_Morph,   morphAnim->morphSamples);
        }
      }
    }
  }

void ObjectsBucket::uboSetDynamic(Descriptors& v, Object& obj, uint8_t fId) {
  auto& ubo = v.ubo[fId][SceneGlobals::V_Main];

  if(mat.frames.size()!=0) {
    auto frame = size_t((obj.timeShift+scene.tickCount)/mat.texAniFPSInv);
    auto t = mat.frames[frame%mat.frames.size()];
    ubo.set(L_Diffuse, *t);
    if(pShadow!=nullptr && textureInShadowPass) {
      for(size_t lay=SceneGlobals::V_Shadow0; lay<=SceneGlobals::V_ShadowLast; ++lay) {
        auto& uboSh = v.ubo[fId][lay];
        uboSh.set(L_Diffuse, *t);
        }
      }
    }

  if(obj.skiningAni!=nullptr/* && !obj.ubo.uboIsReady[fId]*/) {
    // obj.ubo.uboIsReady[fId] = true;
    obj.skiningAni->bind(ubo,L_Skinning,fId);
    if(pShadow!=nullptr) {
      for(size_t lay=SceneGlobals::V_Shadow0; lay<=SceneGlobals::V_ShadowLast; ++lay) {
        auto& uboSh = v.ubo[fId][lay];
        obj.skiningAni->bind(uboSh,L_Skinning,fId);
        }
      }
    }
  }

ObjectsBucket::Descriptors& ObjectsBucket::objUbo(size_t objId) {
  return uboShared;
  }

void ObjectsBucket::setupUbo() {
  uboSetCommon(uboShared);
  }

void ObjectsBucket::invalidateUbo() {
  if(useSharedUbo) {
    } else {
    // for(auto& v:val) {
    //   }
    }
  }

void ObjectsBucket::resetVis() {
  visSet.reset();
  }

void ObjectsBucket::fillTlas(std::vector<RtInstance>& inst) {
  for(size_t i=0; i<valSz; ++i) {
    auto& v = val[i];
    if(v.blas==nullptr)
      continue;
    RtInstance ix;
    ix.mat  = v.pos;
    ix.blas = v.blas;
    inst.push_back(ix);
    }
  }

void ObjectsBucket::preFrameUpdate(uint8_t fId) {
  if(mat.texAniMapDirPeriod.x==0 && mat.texAniMapDirPeriod.y==0 && mat.alpha!=Material::Water)
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

  ubo.waveAnim = 2.f*float(M_PI)*float(scene.tickCount%3000)/3000.f;
  ubo.waveMaxAmplitude = mat.waveMaxAmplitude;
  uboMat[fId].update(&ubo,0,1);
  }

size_t ObjectsBucket::alloc(const Tempest::VertexBuffer<Vertex>&  vbo,
                            const Tempest::IndexBuffer<uint32_t>& ibo,
                            const Tempest::AccelerationStructure* blas,
                            size_t iboOffset, size_t iboLen,
                            const Bounds& bounds) {
  Object* v = &implAlloc(VboType::VboVertex,bounds,nullptr);
  v->vbo       = &vbo;
  v->ibo       = &ibo;
  v->iboOffset = iboOffset;
  v->iboLength = iboLen;
  if(blas!=nullptr && !blas->isEmpty() && !mat.isGhost && mat.alpha==Material::Solid &&
     (objType==Type::Landscape || objType==Type::Static)) {
    v->blas = blas;
    owner.resetTlas();
    }
  return size_t(std::distance(val,v));
  }

size_t ObjectsBucket::alloc(const Tempest::VertexBuffer<VertexA>& vbo,
                            const Tempest::IndexBuffer<uint32_t>& ibo,
                            size_t iboOffset, size_t iboLen,
                            const SkeletalStorage::AnimationId& anim,
                            const Bounds& bounds) {
  Object* v = &implAlloc(VboType::VboVertexA,bounds,&anim);
  v->vboA      = &vbo;
  v->ibo       = &ibo;
  v->iboOffset = iboOffset;
  v->iboLength = iboLen;
  return size_t(std::distance(val,v));
  }

size_t ObjectsBucket::alloc(const Tempest::VertexBuffer<ObjectsBucket::Vertex>* vbo[], const Bounds& bounds) {
  Object* v = &implAlloc(VboType::VboMorph,bounds,nullptr);
  for(size_t i=0; i<Resources::MaxFramesInFlight; ++i)
    v->vboM[i] = vbo[i];
  v->visibility.setAlwaysVis(true);
  return size_t(std::distance(val,v));
  }

void ObjectsBucket::free(const size_t objId) {
  implFree(objId);
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
  const size_t  indSz = visSet.count(c);
  const size_t* index = visSet.index(c);
  if(indSz==0)
    return;

  UboPush pushBlock = {};
  size_t  pushSz    = (morphAnim!=nullptr) ? sizeof(UboPush) : sizeof(UboPushBase);
  if(objType==Pfx)
    pushSz = 0;

  cmd.setUniforms(shader, uboShared.ubo[fId][c]);

  for(size_t i=0; i<indSz; ++i) {
    auto& v = val[index[i]];

    updatePushBlock(pushBlock,v);
    cmd.setUniforms(shader, &pushBlock, pushSz);

    switch(vboType) {
      case VboType::NoVbo:
        break;
      case VboType::VboVertex:
        cmd.draw(*v.vbo, *v.ibo, v.iboOffset, v.iboLength);
        break;
      case VboType::VboVertexA:
        cmd.draw(*v.vboA,*v.ibo, v.iboOffset, v.iboLength);
        break;
      case VboType::VboMorph:
        if(v.vboM[fId]!=nullptr)
          cmd.draw(*v.vboM[fId]);
        break;
      case VboType::VboMorphGpu:
        cmd.draw(*v.vbo, *v.ibo, v.iboOffset, v.iboLength);
        break;
      }
    }
  }

void ObjectsBucket::draw(size_t id, Tempest::Encoder<Tempest::CommandBuffer>& p, uint8_t fId) {
  auto& v = val[id];
  if(v.vbo==nullptr || pMain==nullptr)
    return;

  UboPush pushBlock = {};
  updatePushBlock(pushBlock,v);

  auto& ubo = objUbo(id);
  uboSetDynamic(ubo,v,fId);

  size_t pushSz = (morphAnim!=nullptr) ? sizeof(UboPush) : sizeof(UboPushBase);
  if(objType==Pfx)
    pushSz = 0;

  p.setUniforms(*pMain,ubo.ubo[fId][SceneGlobals::V_Main],&pushBlock,pushSz);
  switch(vboType) {
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
    case VboType::VboMorphGpu:
      p.draw(*v.vbo, *v.ibo, v.iboOffset, v.iboLength);
      break;
    }
  }

void ObjectsBucket::setObjMatrix(size_t i, const Matrix4x4& m) {
  auto& v = val[i];
  v.visibility.setObjMatrix(m);
  v.pos = m;
  if(v.blas!=nullptr) {
    owner.resetTlas();
    }
  }

void ObjectsBucket::setBounds(size_t i, const Bounds& b) {
  val[i].visibility.setBounds(b);
  }

void ObjectsBucket::startMMAnim(size_t i, std::string_view anim, float intensity, uint64_t timeUntil) {
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

void ObjectsBucket::setFatness(size_t i, float f) {
  auto&  v  = val[i];
  v.fatness = f;
  }

void ObjectsBucket::setWind(size_t i, ZenLoad::AnimMode m, float intensity) {
  auto& v = val[i];
  v.wind          = m;
  v.windIntensity = intensity;
  }

bool ObjectsBucket::isSceneInfoRequired() const {
  return mat.isGhost || mat.alpha==Material::Water || mat.alpha==Material::Ghost;
  }

void ObjectsBucket::updatePushBlock(ObjectsBucket::UboPush& push, ObjectsBucket::Object& v) {
  push.pos     = v.pos;
  push.fatness = v.fatness*0.5f;
  if(v.wind!=ZenLoad::AnimMode::NONE && scene.zWindEnabled) {
    float shift = push.pos[3][0]*scene.windDir.x + push.pos[3][2]*scene.windDir.y;

    static const uint64_t preiod = scene.windPeriod;
    float a = float(scene.tickCount%preiod)/float(preiod);
    a = a*2.f-1.f;
    a = std::cos(float(a*M_PI) + shift*0.0001f);

    switch(v.wind) {
      case ZenLoad::AnimMode::WIND:
        // tree
        // a *= v.windIntensity;
        a *= 0.03f;
        break;
      case ZenLoad::AnimMode::WIND2:
        // grass
        // a *= v.windIntensity;
        a *= 0.0005f;
        break;
      case ZenLoad::AnimMode::NONE:
      default:
        // error
        a *= 0.f;
        break;
      }
    push.pos[1][0] += scene.windDir.x*a;
    push.pos[1][2] += scene.windDir.y*a;
    }

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


void ObjectsBucketLnd::drawCommon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId,
                                  const Tempest::RenderPipeline& shader, SceneGlobals::VisCamera c) {
  const size_t  indSz = visSet.count(c);
  const size_t* index = visSet.index(c);
  if(indSz==0)
    return;

  UboPushBase pushBlock  = {};
  pushBlock.pos.identity();
  cmd.setUniforms(shader, uboShared.ubo[fId][c], &pushBlock, sizeof(UboPushBase));

  for(size_t i=0; i<indSz; ++i) {
    auto& v = val[index[i]];
    cmd.draw(*v.vbo, *v.ibo, v.iboOffset, v.iboLength);
    }
  }


void ObjectsBucketDyn::preFrameUpdate(uint8_t fId) {
  ObjectsBucket::preFrameUpdate(fId);

  for(uint8_t ic=0; ic<SceneGlobals::V_Count; ++ic) {
    const auto    c     = SceneGlobals::VisCamera(ic);
    const size_t  indSz = visSet.count(c);
    const size_t* index = visSet.index(c);
    for(size_t i=0; i<indSz; ++i) {
      auto& v = val[index[i]];
      uboSetDynamic(uboObj[index[i]],v,fId);
      }
    }
  }

ObjectsBucket::Object& ObjectsBucketDyn::implAlloc(const VboType type, const Bounds& bounds,
                                                   const SkeletalStorage::AnimationId* anim) {
  auto& obj = ObjectsBucket::implAlloc(type,bounds,anim);

  const size_t id = size_t(std::distance(val,&obj));
  uboObj[id].alloc(*this);
  uboSetCommon(uboObj[id]);

  return obj;
  }

void ObjectsBucketDyn::implFree(const size_t objId) {
  for(uint8_t i=0; i<Resources::MaxFramesInFlight; ++i)
    for(uint8_t lay=SceneGlobals::V_Shadow0; lay<SceneGlobals::V_Count; ++lay) {
      auto& set = uboObj[objId].ubo[i][lay];
      owner.recycle(std::move(set));
      }
  ObjectsBucket::implFree(objId);
  }

ObjectsBucket::Descriptors& ObjectsBucketDyn::objUbo(size_t objId) {
  return uboObj[objId];
  }

void ObjectsBucketDyn::setupUbo() {
  ObjectsBucket::setupUbo();

  for(auto& v:uboObj) {
    if(!v.ubo[0][SceneGlobals::V_Main].isEmpty())
      uboSetCommon(v);
    }
  }

void ObjectsBucketDyn::drawCommon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId, const Tempest::RenderPipeline& shader, SceneGlobals::VisCamera c) {
  const size_t  indSz = visSet.count(c);
  const size_t* index = visSet.index(c);
  if(indSz==0)
    return;

  UboPush pushBlock  = {};
  size_t  pushSz = (morphAnim!=nullptr) ? sizeof(UboPush) : sizeof(UboPushBase);
  if(objType==Pfx)
    pushSz = 0;

  for(size_t i=0; i<indSz; ++i) {
    auto& v = val[index[i]];

    updatePushBlock(pushBlock,v);
    cmd.setUniforms(shader, uboObj[index[i]].ubo[fId][c], &pushBlock, pushSz);

    switch(vboType) {
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
      case VboType::VboMorphGpu:
        cmd.draw(*v.vbo, *v.ibo, v.iboOffset, v.iboLength);
        break;
      }
    }
  }
