#include "objectsbucket.h"

#include <Tempest/Log>

#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/pfx/pfxbucket.h"
#include "sceneglobals.h"

#include "visualobjects.h"
#include "shaders.h"
#include "gothic.h"

using namespace Tempest;

static Matrix4x4 dummyMatrix() {
  auto mat = Matrix4x4::mkIdentity();
  mat.set(0,0, 0.f);
  mat.set(1,1, 0.f);
  mat.set(2,2, 0.f);
  // so bbox test will fail
  mat.set(3,1, -1000000.f);
  return mat;
  }

static RtScene::Category toRtCategory(ObjectsBucket::Type t) {
  switch (t) {
    case ObjectsBucket::LandscapeShadow: return RtScene::None;
    case ObjectsBucket::Landscape:       return RtScene::Landscape;
    case ObjectsBucket::Static:          return RtScene::Static;
    case ObjectsBucket::Movable:         return RtScene::Movable;
    case ObjectsBucket::Animated:        return RtScene::None;
    case ObjectsBucket::Pfx:             return RtScene::None;
    case ObjectsBucket::Morph:           return RtScene::None;
    }
  return RtScene::None;
  }

void ObjectsBucket::Item::setObjMatrix(const Tempest::Matrix4x4 &mt) {
  owner->setObjMatrix(id,mt);
  }

void ObjectsBucket::Item::setAsGhost(bool g) {
  if(owner->mat.isGhost==g)
    return;

  auto m = owner->mat;
  m.isGhost = g;
  auto&  bucket = owner->owner.getBucket(owner->objType,m,owner->staticMesh,owner->animMesh,owner->instanceDesc);

  auto&  v      = owner->val[id];
  size_t idNext = size_t(-1);
  switch(owner->objType) {
    case Landscape:
    case LandscapeShadow: {
      idNext = bucket.alloc(*owner->staticMesh,v.iboOffset,v.iboLength,v.visibility.bounds(),owner->mat);
      break;
      }
    case Static:
    case Movable:
    case Morph: {
      idNext = bucket.alloc(*owner->staticMesh,v.iboOffset,v.iboLength,v.visibility.bounds(),owner->mat);
      break;
      }
    case Pfx: {
      idNext = bucket.alloc(v.visibility.bounds());
      break;
      }
    case Animated: {
      idNext = bucket.alloc(*owner->animMesh,v.iboOffset,v.iboLength,*v.skiningAni);
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
  for(uint8_t i=0; i<Resources::MaxFramesInFlight; ++i)
    setPfxData(v.pfx[i],i);

  oldOw->free(oldId);
  }

void ObjectsBucket::Item::setFatness(float f) {
  if(owner!=nullptr)
    owner->setFatness(id,f);
  }

void ObjectsBucket::Item::setWind(phoenix::animation_mode m, float intensity) {
  if(owner!=nullptr)
    owner->setWind(id,m,intensity);
  }

void ObjectsBucket::Item::startMMAnim(std::string_view anim, float intensity, uint64_t timeUntil) {
  if(owner!=nullptr)
    owner->startMMAnim(id,anim,intensity,timeUntil);
  }

void ObjectsBucket::Item::setPfxData(const Tempest::StorageBuffer* ssbo, uint8_t fId) {
  if(owner!=nullptr)
    owner->setPfxData(id,ssbo,fId);
  }

const Bounds& ObjectsBucket::Item::bounds() const {
  if(owner!=nullptr)
    return owner->bounds(id);
  static Bounds b;
  return b;
  }

Matrix4x4 ObjectsBucket::Item::position() const {
  if(owner!=nullptr)
    return owner->position(id);
  return Matrix4x4();
  }

const StaticMesh* ObjectsBucket::Item::mesh() const {
  if(owner!=nullptr)
    return owner->staticMesh;
  return nullptr;
  }

std::pair<uint32_t, uint32_t> ObjectsBucket::Item::meshSlice() const {
  if(owner!=nullptr)
    return owner->meshSlice(id);
  return std::pair<uint32_t, uint32_t>(0,0);
  }

const Material& ObjectsBucket::Item::material() const {
  if(owner!=nullptr)
    return owner->material(id);
  static Material m;
  return m;
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


ObjectsBucket::ObjectsBucket(const Type type, const Material& mat, VisualObjects& owner, const SceneGlobals& scene,
                             const StaticMesh* stMesh, const AnimMesh* anim, const Tempest::StorageBuffer* desc)
  :objType(type), owner(owner), scene(scene), mat(mat) {
  static_assert(sizeof(UboPush)<=128, "UboPush is way too big");

  instanceDesc        = desc;
  staticMesh          = stMesh;
  animMesh            = anim;

  useSharedUbo        = (mat.frames.size()==0);
  textureInShadowPass = (mat.alpha==Material::AlphaTest);
  usePositionsSsbo    = (type==Type::Static || type==Type::Movable || type==Type::Morph);
  useMeshlets         = (Gothic::options().doMeshShading && !mat.isTesselated() && (type!=Type::Pfx));

  pMain               = Shaders::inst().materialPipeline(mat,objType, isForwardShading() ? Shaders::T_Forward : Shaders::T_Deffered);
  pShadow             = Shaders::inst().materialPipeline(mat,objType, Shaders::T_Shadow);

  if(useSharedUbo) {
    uboShared.alloc(*this);
    bucketShared = allocBucketDesc(mat);
    uboSetCommon(uboShared,mat,bucketShared);
    }
  }

ObjectsBucket::~ObjectsBucket() {
  }

bool ObjectsBucket::isCompatible(const Type t, const Material& mat,
                                 const StaticMesh* st, const AnimMesh* ani,
                                 const Tempest::StorageBuffer* desc) const {
  auto type = sanitizeType(t,mat,st);
  if(objType!=type)
    return false;

  if(type==Pfx) {
    return mat==this->mat;
    }

  if(type==Landscape) {
    if(Gothic::options().doMeshShading) {
      return objType==type && mat.alpha==this->mat.alpha && desc==instanceDesc;
      }
    return mat==this->mat;
    }

  if(type==LandscapeShadow) {
    return false;
    }

  return this->mat==mat && instanceDesc==desc && staticMesh==st && animMesh==ani;
  }

std::unique_ptr<ObjectsBucket> ObjectsBucket::mkBucket(Type type, const Material& mat, VisualObjects& owner, const SceneGlobals& scene,
                                                       const StaticMesh* st, const AnimMesh* anim, const StorageBuffer* desc) {
  type = sanitizeType(type,mat,st);

  if(type==Landscape || type==LandscapeShadow)
    return std::unique_ptr<ObjectsBucket>(new ObjectsBucketDyn(type,mat,owner,scene,st,nullptr,desc));

  if(ObjectsBucket::isAnimated(mat) || type==Pfx)
    return std::unique_ptr<ObjectsBucket>(new ObjectsBucketDyn(type,mat,owner,scene,st,anim,nullptr));

  return std::unique_ptr<ObjectsBucket>(new ObjectsBucket(type,mat,owner,scene,st,anim,nullptr));
  }

ObjectsBucket::Type ObjectsBucket::sanitizeType(const Type t, const Material& mat, const StaticMesh* st) {
  auto type = t;
  if(type==Landscape && mat.texAniMapDirPeriod!=Tempest::Point(0,0))
    type = Static;
  if(type==Landscape && mat.alpha==Material::Water)
    type = Static;
  if(type==Landscape && mat.frames.size()>0)
    type = Static;

  if(st!=nullptr && st->morph.anim!=nullptr) {
    type = ObjectsBucket::Morph;
    }
  return type;
  }

const Material& ObjectsBucket::material() const {
  return mat;
  }

const void* ObjectsBucket::meshPointer() const {
  if(staticMesh!=nullptr)
    return staticMesh;
  return animMesh;
  }

ObjectsBucket::Object& ObjectsBucket::implAlloc(const Bounds& bounds, const Material& /*mat*/) {
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
  v->timeShift  = uint64_t(0-scene.tickCount);
  if(objType==Type::Landscape || objType==Type::Static)
    v->visibility = owner.visGroup.get(VisibilityGroup::G_Static);
  else if(objType==Type::LandscapeShadow) {
    v->visibility = owner.visGroup.get(VisibilityGroup::G_Default);
    }
  else if(objType==Type::Pfx)
    v->visibility = owner.visGroup.get(VisibilityGroup::G_AlwaysVis);
  else
    v->visibility = owner.visGroup.get(VisibilityGroup::G_Default);
  v->visibility.setBounds(bounds);
  v->visibility.setObject(&visSet,size_t(std::distance(val,v)));
  v->skiningAni = nullptr;
  v->isValid    = true;
  reallocObjPositions();

  return *v;
  }

void ObjectsBucket::postAlloc(Object& obj, size_t /*objId*/) {
  if(objType!=Pfx && useMeshlets) {
    assert(obj.iboOffset%PackedMesh::MaxInd==0);
    assert(obj.iboLength%PackedMesh::MaxInd==0);
    }
  invalidateInstancing();
  }

void ObjectsBucket::implFree(const size_t objId) {
  auto& v = val[objId];
  if(v.blas!=nullptr)
    owner.notifyTlas(material(objId), toRtCategory(objType));

  v.visibility = VisibilityGroup::Token();
  v.isValid    = false;
  for(size_t i=0;i<Resources::MaxFramesInFlight;++i)
    v.pfx[i] = nullptr;
  v.blas = nullptr;
  valSz--;
  visSet.erase(objId);

  if(valSz==0) {
    owner.resetIndex();
    objPositions = InstanceStorage::Id();
    } else {
    objPositions.set(dummyMatrix(),objId);
    }
  invalidateInstancing();
  }

ObjectsBucket::Bucket ObjectsBucket::allocBucketDesc(const Material& mat) {
  auto& device = Resources::device();

  BucketDesc ubo;
  ubo.texAniMapDirPeriod = mat.texAniMapDirPeriod;
  ubo.waveMaxAmplitude   = mat.waveMaxAmplitude;
  ubo.alphaWeight        = mat.alphaWeight;
  if(staticMesh!=nullptr) {
    auto& bbox     = staticMesh->bbox.bbox;
    ubo.bboxRadius = staticMesh->bbox.rConservative;
    ubo.bbox[0]    = Vec4(bbox[0].x,bbox[0].y,bbox[0].z,0.f);
    ubo.bbox[1]    = Vec4(bbox[1].x,bbox[1].y,bbox[1].z,0.f);
    }
  if(animMesh!=nullptr) {
    auto& bbox     = animMesh->bbox.bbox;
    ubo.bboxRadius = animMesh->bbox.rConservative;
    ubo.bbox[0]    = Vec4(bbox[0].x,bbox[0].y,bbox[0].z,0.f);
    ubo.bbox[1]    = Vec4(bbox[1].x,bbox[1].y,bbox[1].z,0.f);
    }

  return device.ubo<BucketDesc>(BufferHeap::Device,ubo);
  }

void ObjectsBucket::uboSetCommon(Descriptors& v, const Material& mat, const Bucket& bucket) {
  for(uint8_t fId=0; fId<Resources::MaxFramesInFlight; ++fId) {
    for(size_t lay=SceneGlobals::V_Shadow0; lay<SceneGlobals::V_Count; ++lay) {
      auto& ubo = v.ubo[fId][lay];
      if(ubo.isEmpty())
        continue;
      ubo.set(L_Scene, scene.uboGlobal[lay]);
      if(useMeshlets && (objType==Type::Landscape || objType==Type::LandscapeShadow)) {
        ubo.set(L_MeshDesc, *instanceDesc);
        }
      if(objType!=ObjectsBucket::Landscape && objType!=ObjectsBucket::LandscapeShadow && objType!=ObjectsBucket::Pfx) {
        ubo.set(L_Bucket, bucket);
        }
      if(useMeshlets) {
        if(staticMesh!=nullptr) {
          ubo.set(L_Vbo, staticMesh->vbo);
          ubo.set(L_Ibo, staticMesh->ibo8);
          } else {
          ubo.set(L_Vbo, animMesh->vbo);
          ubo.set(L_Ibo, animMesh->ibo8);
          }
        }
      if(lay==SceneGlobals::V_Main || textureInShadowPass) {
        ubo.set(L_Diffuse, *mat.tex);
        }
      if(lay==SceneGlobals::V_Main && isShadowmapRequired()) {
        ubo.set(L_Shadow0,  *scene.shadowMap[0],Resources::shadowSampler());
        ubo.set(L_Shadow1,  *scene.shadowMap[1],Resources::shadowSampler());
        }
      if(objType==ObjectsBucket::Morph) {
        ubo.set(L_MorphId,  *staticMesh->morph.index  );
        ubo.set(L_Morph,    *staticMesh->morph.samples);
        }
      if(lay==SceneGlobals::V_Main && isSceneInfoRequired()) {
        auto smp = Sampler::bilinear();
        smp.setClamping(ClampMode::MirroredRepeat);
        ubo.set(L_SceneClr, *scene.sceneColor, smp);

        smp = Sampler::nearest();
        smp.setClamping(ClampMode::MirroredRepeat);
        ubo.set(L_GDepth, *scene.sceneDepth, smp);
        }
      if(lay==SceneGlobals::V_Main && useMeshlets) {
        auto smp = Sampler::nearest();
        smp.setClamping(ClampMode::ClampToEdge);
        ubo.set(L_HiZ, *scene.hiZ, smp);
        }
      }
    uboSetSkeleton(v,fId);
    }
  }

void ObjectsBucket::uboSetSkeleton(Descriptors& v, uint8_t fId) {
  auto& ssbo = owner.instanceSsbo();
  if(ssbo.byteSize()==0 || objType==Type::Landscape || objType==Type::LandscapeShadow || objType==Type::Pfx)
    return;

  for(size_t lay=SceneGlobals::V_Shadow0; lay<SceneGlobals::V_Count; ++lay) {
    auto& ubo = v.ubo[fId][lay];
    if(!ubo.isEmpty())
      ubo.set(L_Matrix, ssbo);
    }
  }

void ObjectsBucket::uboSetDynamic(Descriptors& v, Object& obj, uint8_t fId) {
  auto& ubo = v.ubo[fId][SceneGlobals::V_Main];

  if(mat.frames.size()!=0 && mat.texAniFPSInv!=0) {
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
  }

ObjectsBucket::Descriptors& ObjectsBucket::objUbo(size_t objId) {
  return uboShared;
  }

void ObjectsBucket::prepareUniforms() {
  uboSetCommon(uboShared,mat,bucketShared);
  }

void ObjectsBucket::invalidateUbo(uint8_t fId) {
  if(useSharedUbo)
    uboSetSkeleton(uboShared,fId);
  }

void ObjectsBucket::fillTlas(RtScene& out) {
  for(size_t i=0; i<CAPACITY; ++i) {
    auto& v = val[i];
    if(!v.isValid || v.blas==nullptr)
      continue;
    out.addInstance(v.pos, *v.blas, mat, *staticMesh, v.iboOffset, v.iboLength, toRtCategory(objType));
    }
  }

void ObjectsBucket::preFrameUpdate(uint8_t fId) {
  if(!windAnim || !scene.zWindEnabled)
    return;

  bool upd[CAPACITY] = {};
  for(uint8_t ic=0; ic<SceneGlobals::V_Count; ++ic) {
    const auto    c     = SceneGlobals::VisCamera(ic);
    const size_t  indSz = visSet.count(c);
    const size_t* index = visSet.index(c);
    for(size_t i=0; i<indSz; ++i)
      upd[index[i]] = true;
    }

  for(size_t i=0; i<CAPACITY; ++i) {
    auto& v = val[i];
    if(upd[i] && v.wind!=phoenix::animation_mode::none) {
      auto  pos   = v.pos;
      float shift = v.pos[3][0]*scene.windDir.x + v.pos[3][2]*scene.windDir.y;

      static const uint64_t period = scene.windPeriod;
      float a = float(scene.tickCount%period)/float(period);
      a = a*2.f-1.f;
      a = std::cos(float(a*M_PI) + shift*0.0001f);

      switch(v.wind) {
        case phoenix::animation_mode::wind:
          // tree. note: mods tent to bump Intensity to insane values
          if(v.windIntensity>0.f)
            a *= 0.03f; else
            a *= 0;
          break;
        case phoenix::animation_mode::wind2:
          // grass
          if(v.windIntensity<=1.0)
            a *= v.windIntensity * 0.1f; else
            a *= 0;
          break;
        case phoenix::animation_mode::none:
        default:
          // error
          a = 0.f;
          break;
        }
      pos[1][0] += scene.windDir.x*a;
      pos[1][2] += scene.windDir.y*a;
      objPositions.set(pos,i);
      }
    }
  }

size_t ObjectsBucket::alloc(const StaticMesh& mesh, size_t iboOffset, size_t iboLen,
                            const Bounds& bounds, const Material& mat) {
  Object* v = &implAlloc(bounds,mat);
  v->iboOffset = iboOffset;
  v->iboLength = iboLen;

  bool useBlas = toRtCategory(objType)!=RtScene::None;
  if(mat.isGhost)
    useBlas = false;
  if(mat.alpha!=Material::Solid && mat.alpha!=Material::AlphaTest && mat.alpha!=Material::Transparent)
    useBlas = false;

  if(useBlas) {
    if(auto b = mesh.blas(iboOffset, iboLen)) {
      v->blas = b;
      owner.notifyTlas(mat, toRtCategory(objType));
      }
    }
  postAlloc(*v,size_t(std::distance(val,v)));
  return size_t(std::distance(val,v));
  }

size_t ObjectsBucket::alloc(const AnimMesh& mesh, size_t iboOffset, size_t iboLen,
                            const InstanceStorage::Id& anim) {
  Object* v = &implAlloc(mesh.bbox,mat);
  v->iboOffset  = iboOffset;
  v->iboLength  = iboLen;
  v->skiningAni = &anim;
  postAlloc(*v,size_t(std::distance(val,v)));
  return size_t(std::distance(val,v));
  }

size_t ObjectsBucket::alloc(const Bounds& bounds) {
  Object* v = &implAlloc(bounds,mat);
  v->visibility.setGroup(VisibilityGroup::G_AlwaysVis);
  postAlloc(*v,size_t(std::distance(val,v)));
  return size_t(std::distance(val,v));
  }

void ObjectsBucket::free(const size_t objId) {
  implFree(objId);
  }

void ObjectsBucket::draw(Encoder<CommandBuffer>& cmd, uint8_t fId) {
  if(pMain==nullptr)
    return;
  drawCommon(cmd,fId,*pMain,SceneGlobals::V_Main,false);
  }

void ObjectsBucket::drawGBuffer(Encoder<CommandBuffer>& cmd, uint8_t fId) {
  if(pMain==nullptr)
    return;
  drawCommon(cmd,fId,*pMain,SceneGlobals::V_Main,false);
  }

void ObjectsBucket::drawShadow(Encoder<CommandBuffer>& cmd, uint8_t fId, int layer) {
  if(pShadow==nullptr)
    return;
  drawCommon(cmd,fId,*pShadow,SceneGlobals::VisCamera(SceneGlobals::V_Shadow0+layer),false);
  }

void ObjectsBucket::drawHiZ(Tempest::Encoder<Tempest::CommandBuffer>& /*cmd*/, uint8_t /*fId*/) {
  }

void ObjectsBucket::drawCommon(Encoder<CommandBuffer>& cmd, uint8_t fId, const RenderPipeline& shader,
                               SceneGlobals::VisCamera c, bool isHiZPass) {
  const size_t  indSz = visSet.count(c);
  const size_t* index = visSet.index(c);
  if(indSz==0)
    return;

  if(useMeshlets) {
    if(objType==Landscape && c==SceneGlobals::V_Shadow1 && !textureInShadowPass)
      return;
    if(objType==LandscapeShadow && c!=SceneGlobals::V_Shadow1)
      return;
    }

  if(instancingType==Normal)
    visSet.sort(c);
  else if(instancingType==Aggressive)
    visSet.minmax(c);

  cmd.setUniforms(shader, uboShared.ubo[fId][c]);
  UboPush pushBlock = {};
  for(size_t i=0; i<indSz; ++i) {
    auto  id = index[i];
    auto& v  = val[id];

    switch(objType) {
      case Landscape:
      case LandscapeShadow: {
        if(useMeshlets) {
          assert(0);
          } else {
          cmd.draw(staticMesh->vbo, staticMesh->ibo, v.iboOffset, v.iboLength);
          }
        break;
        }
      case Pfx: {
        // cmd.draw(*v.vboM[fId]);
        if(v.pfx[fId]!=nullptr)
          cmd.draw(6, 0, v.pfx[fId]->byteSize()/sizeof(PfxBucket::PfxState));
        break;
        }
      case Static:
      case Movable:
      case Morph:
      case Animated: {
        uint32_t instance = 0;
        if(objType!=Animated)
          instance = objPositions.offsetId()+uint32_t(id); else
          instance = v.skiningAni->offsetId();

        uint32_t cnt   = applyInstancing(i,index,indSz);
        size_t   uboSz = (objType==Morph ? sizeof(UboPush) : sizeof(UboPushBase));

        updatePushBlock(pushBlock,v,instance,cnt);
        if(useMeshlets) {
          cmd.setUniforms(shader, &pushBlock, uboSz);
          cmd.dispatchMeshThreads(cnt);
          // cmd.dispatchMesh(uint32_t(v.iboLength/PackedMesh::MaxInd), cnt);
          } else {
          cmd.setUniforms(shader, &pushBlock, uboSz);
          if(objType!=Animated)
            cmd.draw(staticMesh->vbo, staticMesh->ibo, v.iboOffset, v.iboLength, instance, cnt); else
            cmd.draw(animMesh  ->vbo, animMesh  ->ibo, v.iboOffset, v.iboLength, instance, cnt);
          }
        break;
        }
      }
    }
  }

void ObjectsBucket::setObjMatrix(size_t i, const Matrix4x4& m) {
  auto& v = val[i];
  v.visibility.setObjMatrix(m);
  v.pos = m;

  if(objPositions.size()>0)
    objPositions.set(m,i);

  if(v.blas!=nullptr)
    owner.notifyTlas(material(i), toRtCategory(objType));
  }

void ObjectsBucket::setBounds(size_t i, const Bounds& b) {
  val[i].visibility.setBounds(b);
  }

void ObjectsBucket::startMMAnim(size_t i, std::string_view animName, float intensity, uint64_t timeUntil) {
  if(staticMesh==nullptr || staticMesh->morph.anim==nullptr)
    return;
  auto&  anim = *staticMesh->morph.anim;
  auto&  v    = val[i];
  size_t id   = size_t(-1);
  for(size_t i=0; i<anim.size(); ++i)
    if(anim[i].name==animName) {
      id = i;
      break;
      }

  if(id==size_t(-1))
    return;

  auto& m = anim[id];
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
    if(anim[i.id].layer!=m.layer)
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
  auto& v = val[i];
  v.fatness = f*0.5f;
  invalidateInstancing();
  }

void ObjectsBucket::setWind(size_t i, phoenix::animation_mode m, float intensity) {
  if(intensity!=0 && m==phoenix::animation_mode::none) {
    m = phoenix::animation_mode::wind2;
    }

  auto& v = val[i];
  v.wind          = m;
  v.windIntensity = intensity;
  reallocObjPositions();
  }

void ObjectsBucket::setPfxData(size_t i, const Tempest::StorageBuffer* ssbo, uint8_t fId) {
  assert(ssbo==nullptr);
  }

bool ObjectsBucket::isAnimated(const Material& mat) {
  if(mat.frames.size()>0)
    return true;
  if(mat.texAniMapDirPeriod!=Point() || mat.waveMaxAmplitude!=0)
    return true;
  if(mat.alpha==Material::Water)
    return true;
  return false;
  }

bool ObjectsBucket::isForwardShading() const {
  return !mat.isSolid();
  }

bool ObjectsBucket::isShadowmapRequired() const {
  return isForwardShading() && mat.alpha!=Material::AdditiveLight &&
         mat.alpha!=Material::Multiply && mat.alpha!=Material::Multiply2;
  }

bool ObjectsBucket::isSceneInfoRequired() const {
  return mat.isSceneInfoRequired();
  }

void ObjectsBucket::updatePushBlock(ObjectsBucket::UboPush& push, ObjectsBucket::Object& v, uint32_t instance, uint32_t instanceCount) {
  push.meshletBase        = uint32_t(v.iboOffset/PackedMesh::MaxInd);
  push.meshletPerInstance = int32_t (v.iboLength/PackedMesh::MaxInd);
  push.firstInstance      = instance;
  push.instanceCount      = instanceCount;
  push.fatness            = v.fatness;

  if(objType==Morph) {
    for(size_t i=0; i<Resources::MAX_MORPH_LAYERS; ++i) {
      auto&    ani  = v.morphAnim[i];
      auto&    anim = (*staticMesh->morph.anim)[ani.id];
      uint64_t time = (scene.tickCount-ani.timeStart);

      float    alpha     = float(time%anim.tickPerFrame)/float(anim.tickPerFrame);
      float    intensity = ani.intensity;

      if(scene.tickCount>ani.timeUntil)
        intensity = 0;

      const uint32_t samplesPerFrame = uint32_t(anim.samplesPerFrame);
      push.morph[i].indexOffset = uint32_t(anim.index);
      push.morph[i].sample0     = uint32_t((time/anim.tickPerFrame+0)%anim.numFrames)*samplesPerFrame;
      push.morph[i].sample1     = uint32_t((time/anim.tickPerFrame+1)%anim.numFrames)*samplesPerFrame;
      push.morph[i].alpha       = uint16_t(alpha*uint16_t(-1));
      push.morph[i].intensity   = uint16_t(intensity*uint16_t(-1));
      }
    }
  }

void ObjectsBucket::reallocObjPositions() {
  if(usePositionsSsbo) {
    windAnim = false;
    for(size_t i=0; i<CAPACITY; ++i) {
      auto& vx = val[i];
      if(vx.isValid && vx.wind!=phoenix::animation_mode::none) {
        windAnim = true;
        break;
        }
      }

    size_t valLen = 1;
    for(size_t i=CAPACITY; i>1; --i)
      if(val[i-1].isValid) {
        valLen = i;
        break;
        }

    auto size = valLen*sizeof(Matrix4x4);
    if(objPositions.size()!=size) {
      objPositions = owner.alloc(size);
      for(size_t i=0; i<valLen; ++i)
        objPositions.set(val[i].pos,i);
      }
    }
  }

void ObjectsBucket::invalidateInstancing() {
  instancingType = Normal;
  if(!useSharedUbo || objType==Pfx) {
    instancingType = NoInstancing;
    return;
    }
  Object* pref = nullptr;
  for(size_t i=0; instancingType!=NoInstancing && i<CAPACITY; ++i) {
    auto& vx = val[i];
    if(!vx.isValid)
      continue;
    if(pref==nullptr)
      pref = &vx;

    auto& ref = *pref;
    if(vx.iboOffset!=ref.iboOffset || vx.iboLength!=ref.iboLength)
      instancingType = NoInstancing;
    if(vx.fatness!=ref.fatness)
      instancingType = NoInstancing;
    if(vx.skiningAni!=ref.skiningAni)
      instancingType = NoInstancing;
    }

  if(instancingType==Normal && pref!=nullptr) {
    if(staticMesh!=nullptr && staticMesh->ibo.size()<=PackedMesh::MaxInd){
      instancingType = Aggressive;
      }
    if(useMeshlets && animMesh==nullptr) {
      instancingType = Aggressive;
      }
    }
  }

uint32_t ObjectsBucket::applyInstancing(size_t& i, const size_t* index, size_t indSz) const {
  if(instancingType==NoInstancing) {
    return 1;
    }
  if(instancingType==Aggressive) {
    assert(i==0);
    auto ret = uint32_t(index[indSz-1]-index[0]+1);
    i = indSz;
    return ret;
    }
  auto     id = index[i];
  uint32_t cnt = 1;
  while(i+1<indSz) {
    if(index[i+1]!=id+cnt)
      break;
    ++cnt;
    ++i;
    }
  return cnt;
  }

const Bounds& ObjectsBucket::bounds(size_t i) const {
  return val[i].visibility.bounds();
  }

Matrix4x4 ObjectsBucket::position(size_t i) const {
  return val[i].pos;
  }

const Material& ObjectsBucket::material(size_t i) const {
  return mat;
  }

std::pair<uint32_t, uint32_t> ObjectsBucket::meshSlice(size_t i) const {
  return std::pair<uint32_t, uint32_t>(val[i].iboOffset, val[i].iboLength);
  }


ObjectsBucketDyn::ObjectsBucketDyn(const Type type, const Material& mat, VisualObjects& owner, const SceneGlobals& scene,
                                   const StaticMesh* st, const AnimMesh* anim, const Tempest::StorageBuffer* desc)
  :ObjectsBucket(type,mat,owner,scene,st,anim,desc) {
  if(useMeshlets && objType==Type::LandscapeShadow) {
    auto& device = Resources::device();
    pHiZ = &Shaders::inst().lndPrePass;
    for(uint8_t i=0; i<Resources::MaxFramesInFlight; ++i) {
      auto& ubo = uboHiZ.ubo[i][SceneGlobals::V_Shadow1];
      ubo = device.descriptors(pHiZ->layout());
      if(ubo.isEmpty())
        continue;
      ubo.set(L_MeshDesc, *instanceDesc);
      ubo.set(L_Vbo,      staticMesh->vbo);
      ubo.set(L_Ibo,      staticMesh->ibo8);
      ubo.set(L_Scene,    scene.uboGlobal[SceneGlobals::V_Main]);

      auto smp = Sampler::nearest();
      smp.setClamping(ClampMode::ClampToEdge);
      ubo.set(L_HiZ,      *scene.hiZ, smp);
      }
    }
  }

void ObjectsBucketDyn::preFrameUpdate(uint8_t fId) {
  ObjectsBucket::preFrameUpdate(fId);

  if(!hasDynMaterials)
    return;

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

ObjectsBucket::Object& ObjectsBucketDyn::implAlloc(const Bounds& bounds, const Material& m) {
  auto& obj = ObjectsBucket::implAlloc(bounds,m);

  const size_t id = size_t(std::distance(val,&obj));
  uboObj   [id].alloc(*this);
  mat      [id] = m;
  bucketObj[id] = allocBucketDesc(mat[id]);

  uboSetCommon(uboObj[id],mat[id],bucketObj[id]);
  invalidateDyn();
  return obj;
  }

void ObjectsBucketDyn::implFree(const size_t objId) {
  for(uint8_t i=0; i<Resources::MaxFramesInFlight; ++i)
    for(uint8_t lay=SceneGlobals::V_Shadow0; lay<SceneGlobals::V_Count; ++lay) {
      auto& set = uboObj[objId].ubo[i][lay];
      Resources::recycle(std::move(set));
      }
  ObjectsBucket::implFree(objId);
  invalidateDyn();
  }

ObjectsBucket::Descriptors& ObjectsBucketDyn::objUbo(size_t objId) {
  return uboObj[objId];
  }

void ObjectsBucketDyn::setPfxData(size_t id, const Tempest::StorageBuffer* ssbo, uint8_t fId) {
  if(ssbo->byteSize()==0)
    ssbo = nullptr;

  auto& v   = val[id];
  auto& obj = uboObj[id];

  v.pfx[fId] = ssbo;
  if(ssbo==nullptr || ssbo->byteSize()==0)
    return;

  auto& ubo = obj.ubo[fId][SceneGlobals::V_Main];
  if(objType==ObjectsBucket::Pfx) {
    ubo.set(L_Pfx, *v.pfx[fId]);
    if(pShadow!=nullptr) {
      for(size_t lay=SceneGlobals::V_Shadow0; lay<=SceneGlobals::V_ShadowLast; ++lay) {
        auto& uboSh = obj.ubo[fId][lay];
        uboSh.set(L_Pfx, *v.pfx[fId]);
        }
      }
    }
  }

const Material& ObjectsBucketDyn::material(size_t i) const {
  return mat[i];
  }

void ObjectsBucketDyn::prepareUniforms() {
  ObjectsBucket::prepareUniforms();

  for(size_t i=0; i<CAPACITY; ++i) {
    uboSetCommon(uboObj[i],mat[i],bucketObj[i]);
    }

  if(pHiZ!=nullptr) {
    for(uint8_t i=0; i<Resources::MaxFramesInFlight; ++i) {
      auto& ubo = uboHiZ.ubo[i][SceneGlobals::V_Shadow1];
      if(ubo.isEmpty())
        continue;
      auto smp = Sampler::nearest();
      smp.setClamping(ClampMode::ClampToEdge);
      ubo.set(L_HiZ,      *scene.hiZ, smp);
      }
    }
  }

void ObjectsBucketDyn::invalidateUbo(uint8_t fId) {
  ObjectsBucket::invalidateUbo(fId);

  for(auto& v:uboObj)
    uboSetSkeleton(v,fId);
  }

void ObjectsBucketDyn::fillTlas(RtScene& out) {
  for(size_t i=0; i<CAPACITY; ++i) {
    auto& v = val[i];
    if(!v.isValid || v.blas==nullptr)
      continue;
    out.addInstance(v.pos, *v.blas, mat[i], *staticMesh, v.iboOffset, v.iboLength, toRtCategory(objType));
    }
  }

void ObjectsBucketDyn::invalidateDyn() {
  hasDynMaterials = false;
  for(size_t i=0; i<CAPACITY; ++i) {
    auto& v = val[i];
    if(!v.isValid)
      continue;
    hasDynMaterials |= (mat[i].frames.size()>0);
    }
  }

void ObjectsBucketDyn::drawCommon(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId,
                                  const Tempest::RenderPipeline& shader,
                                  SceneGlobals::VisCamera c, bool isHiZPass) {
  const size_t  indSz = visSet.count(c);
  const size_t* index = visSet.index(c);
  if(indSz==0)
    return;

  if(useMeshlets && !textureInShadowPass) {
    const bool isShadow = (c==SceneGlobals::V_Shadow1 || isHiZPass);
    if(objType==Landscape && isShadow)
      return;
    if(objType==LandscapeShadow && !isShadow)
      return;
    }

  UboPush pushBlock  = {};
  for(size_t i=0; i<indSz; ++i) {
    auto  id = index[i];
    auto& v  = val[id];

    switch(objType) {
      case Landscape:
      case LandscapeShadow: {
        pushBlock.meshletBase        = uint32_t(v.iboOffset/PackedMesh::MaxInd);
        pushBlock.meshletPerInstance =  int32_t(v.iboLength/PackedMesh::MaxInd);
        cmd.setUniforms(shader, uboObj[id].ubo[fId][c], &pushBlock, sizeof(uint32_t)*2);
        if(useMeshlets) {
          cmd.dispatchMeshThreads(v.iboLength/PackedMesh::MaxInd);
          } else {
          cmd.draw(staticMesh->vbo, staticMesh->ibo, v.iboOffset, v.iboLength);
          }
        break;
        }
      case Pfx: {
        if(v.pfx[fId]!=nullptr) {
          cmd.setUniforms(shader, uboObj[id].ubo[fId][c]);
          cmd.draw(6, 0, v.pfx[fId]->byteSize()/sizeof(PfxBucket::PfxState));
          }
        break;
        }
      case Static:
      case Movable:
      case Morph:
      case Animated:  {
        uint32_t instance = 0;
        if(objType!=Animated)
          instance = objPositions.offsetId()+uint32_t(id); else
          instance = v.skiningAni->offsetId();

        size_t uboSz = (objType==Morph ? sizeof(UboPush) : sizeof(UboPushBase));
        updatePushBlock(pushBlock,v,instance,1);
        if(useMeshlets) {
          cmd.setUniforms(shader, uboObj[id].ubo[fId][c], &pushBlock, uboSz);
          cmd.dispatchMeshThreads(1);
          // cmd.dispatchMesh(uint32_t(v.iboLength/PackedMesh::MaxInd), 1);
          } else {
          cmd.setUniforms(shader, uboObj[id].ubo[fId][c], &pushBlock, uboSz);
          if(objType!=Animated)
            cmd.draw(staticMesh->vbo, staticMesh->ibo, v.iboOffset, v.iboLength, instance, 1); else
            cmd.draw(animMesh  ->vbo, animMesh  ->ibo, v.iboOffset, v.iboLength, instance, 1);
          }
        break;
        }
      }
    }
  }

void ObjectsBucketDyn::drawHiZ(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t fId) {
  if(pHiZ==nullptr || objType!=LandscapeShadow || !useMeshlets)
    return;

  cmd.setUniforms(*pHiZ, uboHiZ.ubo[fId][SceneGlobals::V_Shadow1]);
  for(size_t i=0; i<valSz; ++i) {
    auto& v = val[i];
    if(!v.isValid)
      continue;

    UboPush pushBlock = {};
    pushBlock.meshletBase        = uint32_t(v.iboOffset/PackedMesh::MaxInd);
    pushBlock.meshletPerInstance =  int32_t(v.iboLength/PackedMesh::MaxInd);
    cmd.setUniforms(*pHiZ, &pushBlock, sizeof(uint32_t)*2);

    cmd.dispatchMeshThreads(v.iboLength/PackedMesh::MaxInd);
    }
  }
