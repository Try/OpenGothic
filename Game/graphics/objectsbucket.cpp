#include "objectsbucket.h"
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

  if(shaderType==Animated) {
    pMain   = &scene.storage.pAnim;
    pShadow = &scene.storage.pAnimSh;
    } else {
    switch(mat.alpha) {
      case Material::AlphaTest:
        pMain   = &scene.storage.pObjectAt;
        pShadow = &scene.storage.pObjectAtSh;
        break;
      case Material::Transparent:
        pMain   = &scene.storage.pObjectAlpha;
        pShadow = nullptr;
        break;
      case Material::AdditiveLight:
      case Material::Multiply:
      case Material::Multiply2:
      case Material::Solid:
        pMain   = &scene.storage.pObject;
        pShadow = &scene.storage.pObjectSh;
        break;
      case Material::InvalidAlpha:
      case Material::LastGothic:
      case Material::FirstOpenGothic:
      case Material::Last:
        break;
      }
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
    }

  v->vbo       = nullptr;
  v->vboA      = nullptr;
  v->ibo       = &ibo;
  v->bounds    = bounds;
  v->storageSt = storage.st.alloc();

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

void ObjectsBucket::setupUbo() {
  for(auto& v:val) {
    if(v.storageSt==size_t(-1))
      continue;
    auto& u = storage.st.element(v.storageSt);
    setupLights(v,u,true);
    }

  for(size_t fId=0;fId<Resources::MaxFramesInFlight;++fId) {
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

void ObjectsBucket::draw(Painter3d& p, uint8_t fId) {
  if(pMain==nullptr)
    return;

  for(auto& i:val) {
    if(i.ibo==nullptr)
      continue;
    if(!p.isVisible(i.bounds))
      continue;

    auto& ubo = i.ubo[fId];
    ubo.set(3,storage.st[fId],i.storageSt,1);
    if(i.storageSk!=size_t(-1))
      ubo.set(4,storage.sk[fId],i.storageSk,1);
    if(shaderType==Animated)
      p.draw(*pMain,ubo,*i.vboA,*i.ibo); else
      p.draw(*pMain,ubo,*i.vbo, *i.ibo);
    }
  }

void ObjectsBucket::drawShadow(Painter3d& p, uint8_t fId, int layer) {
  if(pShadow==nullptr)
    return;

  for(auto& i:val) {
    if(i.ibo==nullptr)
      continue;
    if(!p.isVisible(i.bounds))
      continue;

    auto& ubo = i.uboSh[fId][layer];
    ubo.set(3,storage.st[fId],i.storageSt,1);
    if(i.storageSk!=size_t(-1))
      ubo.set(4,storage.sk[fId],i.storageSk,1);
    if(shaderType==Animated)
      p.draw(*pShadow,ubo,*i.vboA,*i.ibo); else
      p.draw(*pShadow,ubo,*i.vbo, *i.ibo);
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

  setupLights(v,ubo,false);
  storage.st.markAsChanged();
  }

void ObjectsBucket::setSkeleton(size_t i, const Skeleton* sk) {
  return;

  if(shaderType!=Animated)
    return;
  if(sk==nullptr)
    return;
  auto& v    = val[i];
  auto& skel = storage.sk.element(v.storageSk);

  for(size_t i=0;i<sk->tr.size();++i)
    skel.skel[i] = sk->tr[i];

  storage.sk.markAsChanged();
  }

void ObjectsBucket::setSkeleton(size_t i, const Pose& p) {
  if(shaderType!=Animated)
    return;
  auto& v    = val[i];
  auto& skel = storage.sk.element(v.storageSk);

  std::memcpy(&skel.skel[0],p.tr.data(),p.tr.size()*sizeof(p.tr[0]));
  storage.sk.markAsChanged();
  }

void ObjectsBucket::setBounds(size_t i, const Bounds& b) {
  val[i].bounds = b;
  }

void ObjectsBucket::setupLights(Object& val, UboObject& ubo, bool noCache) {
  int cx = int(val.bounds.midTr.x/10.f);
  int cy = int(val.bounds.midTr.y/10.f);
  int cz = int(val.bounds.midTr.z/10.f);

  if(cx==val.lightCacheKey[0] &&
     cy==val.lightCacheKey[1] &&
     cz==val.lightCacheKey[2] &&
     !noCache)
    return;

  val.lightCacheKey[0] = cx;
  val.lightCacheKey[1] = cy;
  val.lightCacheKey[2] = cz;

  const Light* l[6] = {};
  const size_t cnt = scene.lights.get(val.bounds,l,6);

  for(size_t i=0;i<cnt;++i) {
    ubo.light[i].pos   = l[i]->position();
    ubo.light[i].color = l[i]->color();
    ubo.light[i].range = l[i]->range();
    }
  for(size_t i=cnt;i<6;++i) {
    ubo.light[i].range = 0;
    }
  }

bool ObjectsBucket::Storage::commitUbo(Device& device, uint8_t fId) {
  return st.commitUbo(device,fId) |
         sk.commitUbo(device,fId);
  }
