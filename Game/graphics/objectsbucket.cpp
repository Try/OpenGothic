#include "objectsbucket.h"
#include "pose.h"
#include "sceneglobals.h"
#include "skeleton.h"

#include "graphics/dynamic/painter3d.h"
#include "rendererstorage.h"

using namespace Tempest;

ObjectsBucket::ObjectsBucket(const Material& mat, const SceneGlobals& scene, const Type type)
  :scene(scene), mat(mat), shaderType(type) {
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
  v->storageSt = storageSt.alloc();

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
    auto& u = storageSt.element(v.storageSt);
    setupLights(u);
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
  v->storageSk = storageSk.alloc();
  return std::distance(val.data(),v);
  }

void ObjectsBucket::free(const size_t objId) {
  freeList.push_back(objId);
  auto& v = val[objId];
  if(v.storageSt!=size_t(-1))
    storageSt.free(v.storageSt);
  if(v.storageSk!=size_t(-1))
    storageSk.free(v.storageSk);
  v.vbo  = nullptr;
  v.vboA = nullptr;
  v.ibo  = nullptr;
  }

void ObjectsBucket::draw(Painter3d& p, uint8_t fId) {
  if(pMain==nullptr)
    return;

  storageSt.commitUbo(scene.storage.device,fId);
  storageSk.commitUbo(scene.storage.device,fId);

  for(auto& i:val) {
    if(i.ibo==nullptr)
      continue;
    if(!p.isVisible(i.bounds))
      continue;
    auto& ubo = i.ubo[fId];
    ubo.set(3,storageSt[fId],i.storageSt,1);
    if(i.storageSk!=size_t(-1))
      ubo.set(4,storageSk[fId],i.storageSk,1);
    if(shaderType==Static)
      p.draw(*pMain,ubo,*i.vbo, *i.ibo); else
      p.draw(*pMain,ubo,*i.vboA,*i.ibo);
    }
  }

void ObjectsBucket::drawShadow(Painter3d& p, uint8_t fId, int layer) {
  if(pShadow==nullptr)
    return;

  storageSt.commitUbo(scene.storage.device,fId);
  storageSk.commitUbo(scene.storage.device,fId);

  for(auto& i:val) {
    if(i.ibo==nullptr)
      continue;
    if(!p.isVisible(i.bounds))
      continue;
    auto& ubo = i.uboSh[fId][layer];
    ubo.set(3,storageSt[fId],i.storageSt,1);
    if(i.storageSk!=size_t(-1))
      ubo.set(4,storageSk[fId],i.storageSk,1);
    if(shaderType==Static)
      p.draw(*pShadow,ubo,*i.vbo, *i.ibo); else
      p.draw(*pShadow,ubo,*i.vboA,*i.ibo);
    }
  }

void ObjectsBucket::draw(size_t id, Painter3d& p, uint32_t fId) {
  auto& v = val[id];
  if(v.vbo==nullptr || pMain==nullptr)
    return;
  storageSt.commitUbo(scene.storage.device,uint8_t(fId));
  storageSk.commitUbo(scene.storage.device,uint8_t(fId));

  auto& ubo = v.ubo[fId];
  ubo.set(0,*mat.tex);
  ubo.set(1,Resources::fallbackTexture(),Sampler2d::nearest());
  ubo.set(2,scene.uboGlobalPf[fId][0],0,1);
  ubo.set(3,storageSt[fId],v.storageSt,1);
  p.draw(scene.storage.pObject, ubo, *v.vbo, *v.ibo);
  }

void ObjectsBucket::setObjMatrix(size_t i, const Matrix4x4& m) {
  auto& v = val[i];
  v.bounds.setObjMatrix(m);
  auto& ubo = storageSt.element(v.storageSt);
  ubo.pos = m;

  setupLights(ubo);
  storageSt.markAsChanged();
  }

void ObjectsBucket::setSkeleton(size_t i, const Skeleton* sk) {
  if(shaderType!=Animated)
    return;
  if(sk==nullptr)
    return;
  auto& v    = val[i];
  auto& skel = storageSk.element(v.storageSk);

  for(size_t i=0;i<sk->tr.size();++i)
    skel.skel[i] = sk->tr[i];

  storageSk.markAsChanged();
  }

void ObjectsBucket::setSkeleton(size_t i, const Pose& p) {
  if(shaderType!=Animated)
    return;
  auto& v    = val[i];
  auto& skel = storageSk.element(v.storageSk);

  std::memcpy(&skel.skel[0],p.tr.data(),p.tr.size()*sizeof(p.tr[0]));
  storageSk.markAsChanged();
  }

void ObjectsBucket::setBounds(size_t i, const Bounds& b) {
  val[i].bounds = b;
  }

void ObjectsBucket::setupLights(UboObject& ubo) {
  //TODO: cpu perfomance
  auto& m = ubo.pos;
  Vec3 pos = Vec3(m.at(3,0),m.at(3,1),m.at(3,2));

  float R = 600;
  auto b = std::lower_bound(scene.lights.begin(),scene.lights.end(), pos.x-R ,[](const Light& a, float b){
    return a.position().x<b;
    });
  auto e = std::upper_bound(scene.lights.begin(),scene.lights.end(), pos.x+R ,[](float a,const Light& b){
    return a<b.position().x;
    });

  auto dist = std::distance(b,e);(void)dist;

  const Light* light   = nullptr;
  float        curDist = R*R;

  for(auto i=b;i!=e;++i) {
    auto& l = *i;
    float dist = (l.position()-pos).quadLength();
    if(dist<curDist) {
      light   = &l;
      curDist = dist;
      }
    }
  if(light!=nullptr) {
    ubo.light[0].pos   = light->position();
    ubo.light[0].color = light->color();
    ubo.light[0].range = light->range();
    }
  }
