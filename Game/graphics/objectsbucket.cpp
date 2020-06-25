#include "objectsbucket.h"

#include <Tempest/Log>

#include "pose.h"
#include "sceneglobals.h"
#include "skeleton.h"

#include "graphics/dynamic/painter3d.h"
#include "utils/workers.h"
#include "rendererstorage.h"

using namespace Tempest;

ObjectsBucket::ObjectsBucket(const Material& mat, const SceneGlobals& scene, Storage& storage, const Type type)
  :scene(scene), storage(storage), mat(mat), shaderType(type) {
  static_assert(sizeof(UboObject)<=256, "UboObject is way too big");
  static_assert(sizeof(UboPush)  <=128, "UboPush is way too big");
  static_assert(sizeof(UboPushLt)<=128, "UboPushLt is way too big");

  switch(mat.alpha) {
    case Material::AlphaTest:
      if(shaderType==Animated) {
        pMain   = &scene.storage.pAnimAt;
        pLight  = &scene.storage.pAnimAtLt;
        pShadow = &scene.storage.pAnimAtSh;
        } else {
        pMain   = &scene.storage.pObjectAt;
        pLight  = &scene.storage.pObjectAtLt;
        pShadow = &scene.storage.pObjectAtSh;
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
    case Material::AdditiveLight:
    case Material::Multiply:
    case Material::Multiply2:
    case Material::Solid:
      if(shaderType==Animated) {
        pMain   = &scene.storage.pAnim;
        pLight  = &scene.storage.pAnimLt;
        pShadow = &scene.storage.pAnimSh;
        } else {
        pMain   = &scene.storage.pObject;
        pLight  = &scene.storage.pObjectLt;
        pShadow = &scene.storage.pObjectSh;
        }
      break;
    case Material::InvalidAlpha:
    case Material::LastGothic:
    case Material::FirstOpenGothic:
    case Material::Last:
      break;
    }
  }

ObjectsBucket::~ObjectsBucket() {
  }

const Material& ObjectsBucket::material() const {
  return mat;
  }

ObjectsBucket::Object& ObjectsBucket::implAlloc(const Tempest::IndexBuffer<uint32_t>& ibo, const Bounds& bounds) {
  Object* v = nullptr;
  if(freeList.size()>0) {
    v = &val[freeList.back()];
    freeList.pop_back();
    } else {
    val.emplace_back();
    v = &val.back();
    index.resize(val.size());
    }

  v->vbo       = nullptr;
  v->vboA      = nullptr;
  v->ibo       = &ibo;
  v->bounds    = bounds;
  v->storageSt = storage.st.alloc();

  for(size_t i=0;i<Resources::MaxFramesInFlight;++i) {
    v->uboBit[i] = 0;
    for(auto& b:v->uboBitSh[i])
      b = 0;
    }

  if(v->ubo[0].isEmpty()) {
    for(size_t i=0;i<Resources::MaxFramesInFlight;++i) {
      if(pMain!=nullptr)
        v->ubo[i] = scene.storage.device.uniforms(pMain->layout());
      if(pShadow!=nullptr) {
        for(size_t lay=0;lay<Resources::ShadowLayers;++lay)
          v->uboSh[i][lay] = scene.storage.device.uniforms(pShadow->layout());
        }
      }
    uboSetCommon(*v);
    }
  return *v;
  }

void ObjectsBucket::uboSetCommon(ObjectsBucket::Object& v) {
  for(size_t i=0;i<Resources::MaxFramesInFlight;++i) {
    auto& t   = *mat.tex;
    auto& ubo = v.ubo[i];

    if(pMain!=nullptr) {
      ubo.set(0,t);
      ubo.set(1,*scene.shadowMap,Resources::shadowSampler());
      ubo.set(2,scene.uboGlobalPf[i][0],0,1);
      }

    if(pShadow!=nullptr) {
      for(size_t lay=0;lay<Resources::ShadowLayers;++lay) {
        auto& uboSh = v.uboSh[i][lay];

        if(material().alpha==Material::ApphaFunc::Solid)
          uboSh.set(0,Resources::fallbackBlack(),Sampler2d::nearest()); else
          uboSh.set(0,t);
        uboSh.set(1,Resources::fallbackTexture(),Tempest::Sampler2d::nearest());
        uboSh.set(2,scene.uboGlobalPf[i][lay],0,1);
        }
      }
    }
  }

bool ObjectsBucket::groupVisibility(Painter3d& p) {
  if(shaderType!=Static)
    return true;

  if(allBounds.r<=0) {
    Tempest::Vec3 bbox[2] = {};
    bool          fisrt=true;
    for(size_t i=0;i<val.size();++i) {
      if(val[i].ibo==nullptr)
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

void ObjectsBucket::setupUbo() {
  for(auto& v:val) {
    if(v.storageSt==size_t(-1))
      continue;
    setupLights(v,true);
    }

  for(uint8_t fId=0;fId<Resources::MaxFramesInFlight;++fId) {
    for(auto& i:val) {
      auto& ubo = i.ubo[fId];
      if(pMain!=nullptr) {
        ubo.set(1,*scene.shadowMap,Resources::shadowSampler());
        ubo.set(2,scene.uboGlobalPf[fId][0],0,1);
        }

      if(pShadow!=nullptr) {
        for(size_t lay=0;lay<Resources::ShadowLayers;++lay) {
          auto& uboSh = i.uboSh[fId][lay];
          uboSh.set(1,Resources::fallbackTexture(),Tempest::Sampler2d::nearest());
          uboSh.set(2,scene.uboGlobalPf[fId][lay],0,1);
          }
        }
      }
    }

  setupPerFrameUbo();
  }

void ObjectsBucket::setupPerFrameUbo() {
  for(uint8_t fId=0;fId<Resources::MaxFramesInFlight;++fId) {
    for(auto& i:val) {
      i.uboBit[fId] = 0;
      for(auto& b:i.uboBitSh[fId])
        b = 0;
      }
    }
  }

void ObjectsBucket::visibilityPass(Painter3d& p) {
  index.clear();
  if(!groupVisibility(p))
    return;

  for(auto& v:val) {
    if(v.ibo==nullptr)
      continue;
    if(!p.isVisible(v.bounds))
      continue;
    index.push_back(&v);
    }
  }

size_t ObjectsBucket::alloc(const Tempest::VertexBuffer<Vertex>&  vbo,
                            const Tempest::IndexBuffer<uint32_t>& ibo,
                            const Bounds& bounds) {
  Object* v = &implAlloc(ibo,bounds);
  v->vbo = &vbo;
  return std::distance(val.data(),v);
  }

size_t ObjectsBucket::alloc(const Tempest::VertexBuffer<VertexA>& vbo,
                            const Tempest::IndexBuffer<uint32_t>& ibo,
                            const Bounds& bounds) {
  Object* v = &implAlloc(ibo,bounds);
  v->vboA = &vbo;
  v->storageSk = storage.sk.alloc();
  return std::distance(val.data(),v);
  }

void ObjectsBucket::free(const size_t objId) {
  freeList.push_back(objId);
  auto& v = val[objId];
  if(v.storageSt!=size_t(-1))
    storage.st.free(v.storageSt);
  if(v.storageSk!=size_t(-1))
    storage.sk.free(v.storageSk);
  v.vbo  = nullptr;
  v.vboA = nullptr;
  v.ibo  = nullptr;
  }

void ObjectsBucket::draw(Tempest::Encoder<Tempest::CommandBuffer>& p, uint8_t fId) {
  if(pMain==nullptr)
    return;

  UboPush pushBlock;
  for(auto pv:index) {
    auto& v = *pv;

    auto& ubo = v.ubo[fId];
    setUbo(v.uboBit[fId],ubo,3,storage.st[fId],v.storageSt,1);
    if(v.storageSk!=size_t(-1))
      setUbo(v.uboBit[fId],ubo,4,storage.sk[fId],v.storageSk,1);

    const size_t cnt = v.lightCnt;
    for(size_t r=0; r<cnt && r<LIGHT_INLINE; ++r) {
      pushBlock.light[r].pos   = v.light[r]->position();
      pushBlock.light[r].color = v.light[r]->color();
      pushBlock.light[r].range = v.light[r]->range();
      }
    for(size_t r=cnt;r<LIGHT_INLINE;++r) {
      pushBlock.light[r].range = 0;
      }
    p.setUniforms(*pMain,&pushBlock,sizeof(pushBlock));
    p.setUniforms(*pMain,ubo);
    if(shaderType==Animated)
      p.draw(*v.vboA,*v.ibo); else
      p.draw(*v.vbo, *v.ibo);
    }
  }

void ObjectsBucket::drawLight(Tempest::Encoder<Tempest::CommandBuffer>& p, uint8_t fId) {
  if(pLight==nullptr)
    return;

  UboPushLt pushBlock;
  for(auto pv:index) {
    auto& v = *pv;
    if(v.lightCnt<=LIGHT_INLINE)
      continue;

    auto& ubo = v.ubo[fId];
    p.setUniforms(*pLight,ubo);
    for(size_t i=LIGHT_INLINE; i<v.lightCnt; i+=LIGHT_BLOCK) {
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
      if(shaderType==Animated)
        p.draw(*v.vboA,*v.ibo); else
        p.draw(*v.vbo, *v.ibo);
      }
    }
  }

void ObjectsBucket::drawShadow(Tempest::Encoder<Tempest::CommandBuffer>& p, uint8_t fId, int layer) {
  if(pShadow==nullptr)
    return;

  for(auto pv:index) {
    auto& v = *pv;

    auto& ubo = v.uboSh[fId][layer];
    setUbo(v.uboBitSh[fId][layer],ubo,3,storage.st[fId],v.storageSt,1);
    if(v.storageSk!=size_t(-1))
      setUbo(v.uboBitSh[fId][layer],ubo,4,storage.sk[fId],v.storageSk,1);

    p.setUniforms(*pShadow,ubo);
    if(shaderType==Animated)
      p.draw(*v.vboA,*v.ibo); else
      p.draw(*v.vbo, *v.ibo);
    }
  }

void ObjectsBucket::draw(size_t id, Painter3d& p, uint8_t fId) {
  auto& v = val[id];
  if(v.vbo==nullptr || pMain==nullptr)
    return;

  storage.st.commitUbo(scene.storage.device,fId);
  storage.sk.commitUbo(scene.storage.device,fId);

  auto& ubo = v.ubo[fId];
  ubo.set(0,*mat.tex);
  ubo.set(1,Resources::fallbackTexture(),Sampler2d::nearest());
  ubo.set(2,scene.uboGlobalPf[fId][0],0,1);
  ubo.set(3,storage.st[fId],v.storageSt,1);
  p.draw(*pMain, ubo, *v.vbo, *v.ibo);
  }

void ObjectsBucket::setObjMatrix(size_t i, const Matrix4x4& m) {
  auto& v = val[i];
  v.bounds.setObjMatrix(m);
  auto& ubo = storage.st.element(v.storageSt);
  ubo.pos = m;

  if(shaderType==Static) {
    allBounds.r = 0;
    }

  setupLights(v,false);
  storage.st.markAsChanged(v.storageSt);
  }

void ObjectsBucket::setSkeleton(size_t i, const Pose& p) {
  if(shaderType!=Animated)
    return;
  auto& v    = val[i];
  auto& skel = storage.sk.element(v.storageSk);

  std::memcpy(&skel.skel[0],p.tr.data(),p.tr.size()*sizeof(p.tr[0]));
  storage.sk.markAsChanged(v.storageSk);
  }

void ObjectsBucket::setBounds(size_t i, const Bounds& b) {
  val[i].bounds = b;
  }

void ObjectsBucket::setupLights(Object& val, bool noCache) {
  int cx = int(val.bounds.midTr.x/20.f);
  int cy = int(val.bounds.midTr.y/20.f);
  int cz = int(val.bounds.midTr.z/20.f);

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

template<class T>
void ObjectsBucket::setUbo(uint8_t& bit, Tempest::Uniforms& ubo, uint8_t layoutBind,
                           const Tempest::UniformBuffer<T>& vbuf, size_t offset, size_t size) {
  const uint8_t flg = uint8_t(uint8_t(1)<<layoutBind);
  if(bit & flg)
    return;
  bit = uint8_t(bit | flg);
  ubo.set(layoutBind,vbuf,offset,size);
  }

void ObjectsBucket::setUbo(uint8_t& bit, Uniforms& ubo, uint8_t layoutBind,
                           const Texture2d& tex, const Sampler2d& smp) {
  const uint8_t flg = uint8_t(uint8_t(1)<<layoutBind);
  if(bit & flg)
    return;
  bit = uint8_t(bit | flg);
  ubo.set(layoutBind,tex,smp);
  }

bool ObjectsBucket::Storage::commitUbo(Device& device, uint8_t fId) {
  return st.commitUbo(device,fId) |
         sk.commitUbo(device,fId);
  }
