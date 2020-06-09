#include "objectsbucket.h"
#include "pose.h"
#include "skeleton.h"

#include "graphics/dynamic/painter3d.h"
#include "rendererstorage.h"

using namespace Tempest;

ObjectsBucket::ObjectsBucket(const Texture2d* tex, const RendererStorage& storage, const Type type)
  :storage(storage), tex(tex), shaderType(type) {
  }

ObjectsBucket::~ObjectsBucket() {
  }

const Texture2d& ObjectsBucket::texture() const {
  return *tex;
  }

ObjectsBucket::Object& ObjectsBucket::implAlloc(const Tempest::IndexBuffer<uint32_t>& ibo,
                                                const Bounds& bounds,
                                                const UniformsLayout& layout,
                                                const UniformsLayout& layoutSh) {
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
    v->ubo[0]   = storage.device.uniforms(layout);
    v->ubo[1]   = storage.device.uniforms(layout);

    v->uboSh[0] = storage.device.uniforms(layoutSh);
    v->uboSh[1] = storage.device.uniforms(layoutSh);

    uboSetCommon(*v);
    }
  return *v;
  }

void ObjectsBucket::uboSetCommon(ObjectsBucket::Object& v) {
  for(size_t i=0;i<Resources::MaxFramesInFlight;++i) {
    auto& t = texture();
    v.ubo[i].set(0,t);
    if(t.format()==TextureFormat::DXT1)
      v.uboSh[i].set(0,Resources::fallbackBlack(),Sampler2d::nearest()); else
      v.uboSh[i].set(0,t);
    }
  }

size_t ObjectsBucket::alloc(const Tempest::VertexBuffer<Vertex>&  vbo,
                            const Tempest::IndexBuffer<uint32_t>& ibo,
                            const Bounds& bounds) {
  Object* v = &implAlloc(ibo,bounds,storage.uboObjLayout(),storage.uboObjLayout());
  v->vbo = &vbo;
  return std::distance(val.data(),v);
  }

size_t ObjectsBucket::alloc(const Tempest::VertexBuffer<VertexA>& vbo,
                            const Tempest::IndexBuffer<uint32_t>& ibo,
                            const Bounds& bounds) {
  Object* v = &implAlloc(ibo,bounds,storage.uboAniLayout(),storage.uboAniLayout());
  v->vboA = &vbo;
  v->storageSk = storageSk.alloc();
  return std::distance(val.data(),v);
  }

void ObjectsBucket::free(const size_t objId) {
  freeList.push_back(objId);
  auto& v = val[objId];
  storageSt.free(v.storageSt);
  if(v.vboA!=nullptr)
    storageSk.free(v.storageSk);
  v.vbo  = nullptr;
  v.vboA = nullptr;
  v.ibo  = nullptr;
  }

void ObjectsBucket::draw(Painter3d& p, uint8_t fId, const Tempest::UniformBuffer<UboGlobal>& uboGlobal, const Texture2d& shadowMap) {
  storageSt.commitUbo(storage.device,uint8_t(fId));
  storageSk.commitUbo(storage.device,uint8_t(fId));

  for(auto& i:val) {
    if(i.ibo==nullptr)
      continue;
    if(!p.isVisible(i.bounds))
      continue;
    auto& ubo = i.ubo[fId];
    //ubo.set(0,texture());
    ubo.set(1,shadowMap,Resources::shadowSampler());
    ubo.set(2,uboGlobal,0,1);
    ubo.set(3,storageSt[fId],i.storageSt,1);
    if(shaderType==Animated)
      ubo.set(4,storageSk[fId],i.storageSk,1);
    if(shaderType==Static)
      p.draw(storage.pObject,ubo,*i.vbo, *i.ibo); else
      p.draw(storage.pAnim,  ubo,*i.vboA,*i.ibo);
    }
  }

void ObjectsBucket::drawShadow(Painter3d& p, uint8_t fId, const Tempest::UniformBuffer<UboGlobal>& uboGlobal, int layer) {
  if(layer!=0)
    return;
  storageSt.commitUbo(storage.device,fId);
  storageSk.commitUbo(storage.device,fId);

  for(auto& i:val) {
    if(i.ibo==nullptr)
      continue;
    if(!p.isVisible(i.bounds))
      continue;
    auto& ubo = i.uboSh[fId];
    //ubo.set(0,texture());
    ubo.set(1,Resources::fallbackTexture(),Tempest::Sampler2d::nearest());
    ubo.set(2,uboGlobal,0,1);
    ubo.set(3,storageSt[fId],i.storageSt,1);
    if(shaderType==Animated)
      ubo.set(4,storageSk[fId],i.storageSk,1);
    if(shaderType==Static)
      p.draw(storage.pObjectSh,ubo,*i.vbo, *i.ibo); else
      p.draw(storage.pAnimSh,  ubo,*i.vboA,*i.ibo);
    }
  }

void ObjectsBucket::draw(size_t id, Tempest::Encoder<CommandBuffer>& cmd,
                         const RenderPipeline& pipeline, uint32_t fId) {
  //auto& frame = pf[imgId];
  auto& di    = val[id];
  if(di.vbo==nullptr)
    return;
  // uint32_t offset = uint32_t(di.ubo);
  //
  // cmd.setUniforms(pipeline,di.ubo[fId],1,&offset);
  // cmd.draw(*di.vbo,*di.ibo);
  }

void ObjectsBucket::setObjMatrix(size_t i, const Matrix4x4& m) {
  auto& v = val[i];
  storageSt.element(v.storageSt) = m;
  v.bounds.setObjMatrix(m);
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
