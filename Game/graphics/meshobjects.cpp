#include "meshobjects.h"

#include <Tempest/Log>

#include "skeleton.h"
#include "pose.h"
#include "attachbinder.h"
#include "light.h"
#include "objectsbucket.h"
#include "rendererstorage.h"
#include "bounds.h"

MeshObjects::MeshObjects(const RendererStorage &storage)
  :storage(storage),uboGlobalPf{storage.device,storage.device} {
  uboGlobal.lightDir={{1,1,-1}};
  float l=0;
  for(auto i:uboGlobal.lightDir)
    l+=(i*i);
  l = std::sqrt(l);
  for(auto& i:uboGlobal.lightDir)
    i/=l;

  uboGlobal.modelView.identity();
  uboGlobal.shadowView.identity();
  }

MeshObjects::~MeshObjects() {
  }

bool MeshObjects::needToUpdateCommands(uint8_t fId) const {
  if(storageSt.needToRealloc(fId) || storageDn.needToRealloc(fId))
    return true;

  for(auto& i:chunksSt)
    if(i.needToUpdateCommands(fId))
      return true;
  for(auto& i:chunksDn)
    if(i.needToUpdateCommands(fId))
      return true;
  return false;
  }

void MeshObjects::setAsUpdated(uint8_t fId) {
  for(auto& i:chunksSt)
    i.setAsUpdated(fId);
  for(auto& i:chunksDn)
    i.setAsUpdated(fId);
  }

void MeshObjects::setModelView(const Tempest::Matrix4x4 &m, const Tempest::Matrix4x4 *sh, size_t shCount) {
  assert(shCount==2);

  uboGlobal.modelView  = m;
  uboGlobal.shadowView = sh[0];
  shadowView1          = sh[1];
  }

void MeshObjects::setLight(const Light &l,const Tempest::Vec3& ambient) {
  auto  d = l.dir();
  auto& c = l.color();

  uboGlobal.lightDir = {-d[0],-d[1],-d[2]};
  uboGlobal.lightCl  = {c.x,c.y,c.z,0.f};
  uboGlobal.lightAmb = {ambient.x,ambient.y,ambient.z,0.f};
  }

ObjectsBucket<MeshObjects::UboSt,Resources::Vertex> &MeshObjects::getBucketSt(const Tempest::Texture2d *mat) {
  auto& device=storage.device;

  for(auto& i:chunksSt)
    if(&i.texture()==mat)
      return i;

  chunksSt.emplace_back(mat,storage.uboObjLayout(),device,storageSt);
  return chunksSt.back();
  }

ObjectsBucket<MeshObjects::UboDn,Resources::VertexA> &MeshObjects::getBucketDn(const Tempest::Texture2d *mat) {
  auto& device=storage.device;

  for(auto& i:chunksDn)
    if(&i.texture()==mat)
      return i;

  chunksDn.emplace_back(mat,storage.uboObjLayout(),device,storageDn);
  return chunksDn.back();
  }

MeshObjects::Item MeshObjects::implGet(const StaticMesh &mesh, const StaticMesh::SubMesh& s,
                                       int32_t texVar, int32_t teethTex, int32_t bodyColor) {
  const Tempest::Texture2d* tex=s.texture;
  if(teethTex!=0 || bodyColor!=0 || texVar!=0){
    if(s.texName=="HUM_TEETH_V0.TGA"){
      tex=solveTex(s.texture,s.texName,teethTex,bodyColor);
      }
    else if(s.texName=="HUM_MOUTH_V0.TGA"){
      tex=solveTex(s.texture,s.texName,teethTex,bodyColor);
      }
    else if(s.texName.find_first_of("VC")!=std::string::npos){
      tex=solveTex(s.texture,s.texName,texVar,bodyColor);
      }
    }

  if(tex==nullptr) {
    if(!s.texName.empty())
      Tempest::Log::e("texture not found: \"",s.texName,"\"");
    return MeshObjects::Item();
    }
  return implGet(mesh,tex,s.ibo);
  }

MeshObjects::Item MeshObjects::implGet(const StaticMesh &mesh, const Tempest::Texture2d *mat,
                                       const Tempest::IndexBuffer<uint32_t>& ibo) {
  if(mat==nullptr) {
    Tempest::Log::e("no texture?!");
    return MeshObjects::Item();
    }
  auto&        bucket = getBucketSt(mat);
  const size_t id     = bucket.alloc(mesh.vbo,ibo,mesh.bbox);
  return Item(bucket,id);
  }

MeshObjects::Item MeshObjects::implGet(const AnimMesh &mesh, const Tempest::Texture2d *mat,
                                       const Tempest::IndexBuffer<uint32_t> &ibo) {
  if(mat==nullptr) {
    Tempest::Log::e("no texture?!");
    return MeshObjects::Item();
    }
  auto&        bucket = getBucketDn(mat);
  const size_t id     = bucket.alloc(mesh.vbo,ibo,mesh.bbox);
  return Item(bucket,id);
  }

const Tempest::Texture2d *MeshObjects::solveTex(const Tempest::Texture2d *def, const std::string &format, int32_t v, int32_t c) {
  if(format.find_first_of("VC")!=std::string::npos){
    auto ntex = Resources::loadTexture(format,v,c);
    if(ntex!=nullptr)
      return ntex;
    }
  return def;
  }

MeshObjects::Mesh MeshObjects::get(const StaticMesh &mesh) {
  std::unique_ptr<Item[]> dat(new Item[mesh.sub.size()]);
  size_t count=0;
  for(size_t i=0;i<mesh.sub.size();++i){
    Item it = implGet(mesh,mesh.sub[i].texture,mesh.sub[i].ibo);
    if(it.isEmpty())
      continue;
    dat[count] = std::move(it);
    ++count;
    }
  return Mesh(nullptr,std::move(dat),count);
  }

MeshObjects::Mesh MeshObjects::get(const StaticMesh& mesh,
                                   int32_t headTexVar, int32_t teethTex, int32_t bodyColor) {
  std::unique_ptr<Item[]> dat(new Item[mesh.sub.size()]);
  size_t count=0;
  for(size_t i=0;i<mesh.sub.size();++i){
    Item it = implGet(mesh,mesh.sub[i],headTexVar,teethTex,bodyColor);
    if(it.isEmpty())
      continue;
    dat[count] = std::move(it);
    ++count;
    }
  return Mesh(nullptr,std::move(dat),count);
  }

MeshObjects::Mesh MeshObjects::get(const ProtoMesh& mesh, int32_t texVar, int32_t teethTex, int32_t bodyColor) {
  const size_t skinnedCount=mesh.skinedNodesCount();
  std::unique_ptr<Item[]> dat(new Item[mesh.submeshId.size()+skinnedCount]);

  size_t count=0;
  for(auto& m:mesh.submeshId) {
    auto& att = mesh.attach[m.id];
    auto& s   = att.sub[m.subId];
    Item  it  = implGet(att,s,texVar,teethTex,bodyColor);
    if(it.isEmpty())
      continue;
    dat[count] = std::move(it);
    count++;
    }

  for(auto& skin:mesh.skined){
    for(auto& m:skin.sub){
      const Tempest::Texture2d* tex=solveTex(m.texture,m.texName,texVar,bodyColor);
      if(tex!=nullptr) {
        dat[count] = implGet(skin,tex,m.ibo);
        ++count;
        } else {
        if(!m.texName.empty())
          Tempest::Log::e("texture not found: \"",m.texName,"\"");
        }
      }
    }
  return Mesh(&mesh,std::move(dat),count);
  }

void MeshObjects::commitUbo(uint8_t fId,const Tempest::Texture2d& shadowMap) {
  auto& device=storage.device;

  const bool reallocSt = storageSt.commitUbo(device,fId);
  const bool reallocDn = storageDn.commitUbo(device,fId);

  uboGlobalPf[0].update(uboGlobal,fId);
  auto ubo2 = uboGlobal;
  ubo2.shadowView = shadowView1;
  uboGlobalPf[1].update(ubo2,fId);


  for(auto& i:chunksSt){
    if(!reallocSt && !i.needToUpdateCommands(fId))
      continue;
    auto& ubo = i.uboMain(fId);
    ubo.set(0,uboGlobalPf[0][fId],0,1);
    ubo.set(1,storageSt[fId],0,1);
    ubo.set(2,i.texture());
    ubo.set(3,shadowMap,Resources::shadowSampler());
    }
  for(auto& i:chunksDn){
    if(!reallocDn && !i.needToUpdateCommands(fId))
      continue;
    auto& ubo = i.uboMain(fId);
    ubo.set(0,uboGlobalPf[0][fId],0,1);
    ubo.set(1,storageDn[fId],0,1);
    ubo.set(2,i.texture());
    ubo.set(3,shadowMap,Resources::shadowSampler());
    }

  for(int layer=0;layer<2;++layer) {
    for(auto& i:chunksSt){
      if(!reallocSt && !i.needToUpdateCommands(fId))
        continue;
      auto& ubo = i.uboShadow(fId,layer);
      ubo.set(0,uboGlobalPf[layer][fId],0,1);
      ubo.set(1,storageSt[fId],0,1);
      ubo.set(2,i.texture());
      ubo.set(3,Resources::fallbackTexture(),Tempest::Sampler2d::nearest());
      }
    for(auto& i:chunksDn){
      if(!reallocDn && !i.needToUpdateCommands(fId))
        continue;
      auto& ubo = i.uboShadow(fId,layer);
      ubo.set(0,uboGlobalPf[layer][fId],0,1);
      ubo.set(1,storageDn[fId],0,1);
      ubo.set(2,i.texture(),Tempest::Sampler2d::nearest());
      ubo.set(3,Resources::fallbackTexture(),Tempest::Sampler2d::nearest());
      }
    }
  }

void MeshObjects::reserve(size_t stat, size_t dyn) {
  storageSt.reserve(stat);
  storageDn.reserve(dyn);
  }

void MeshObjects::draw(Painter3d& painter, uint32_t fId) {
  for(auto& c:chunksSt)
    c.draw(painter,storage.pObject,fId);
  for(auto& c:chunksDn)
    c.draw(painter,storage.pAnim,fId);
  }

void MeshObjects::drawShadow(Painter3d& painter, uint32_t fId, int layer) {
  for(auto& c:chunksSt)
    c.drawShadow(painter,storage.pObjectSh,fId,layer);
  for(auto& c:chunksDn)
    c.drawShadow(painter,storage.pAnimSh,fId,layer);
  }

void MeshObjects::invalidateCmd() {
  for(auto& c:chunksSt)
    c.invalidateCmd();
  for(auto& c:chunksDn)
    c.invalidateCmd();
  }

void MeshObjects::Mesh::setSkeleton(const Skeleton *sk) {
  skeleton = sk;
  if(ani!=nullptr && skeleton!=nullptr)
    binder=Resources::bindMesh(*ani,*skeleton);
  for(size_t i=0;i<subCount;++i)
    sub[i].setSkeleton(sk);
  }

void MeshObjects::Mesh::setPose(const Pose &p,const Tempest::Matrix4x4& obj) {
  for(size_t i=0;i<subCount;++i)
    sub[i].setPose(p);

  if(binder!=nullptr){
    for(size_t i=0;i<binder->bind.size();++i){
      auto id=binder->bind[i];
      if(id>=p.tr.size())
        continue;
      auto mat=obj;
      mat.translate(ani->rootTr[0],ani->rootTr[1],ani->rootTr[2]);
      mat.mul(p.tr[id]);
      sub[i].setObjMatrix(mat);
      }
    }
  }

Tempest::Vec3 MeshObjects::Mesh::translate() const {
  if(ani==nullptr)
    return Tempest::Vec3();
  return Tempest::Vec3(ani->rootTr[0],ani->rootTr[1],ani->rootTr[2]);
  }

MeshObjects::Mesh::Mesh(MeshObjects::Mesh &&other) {
  *this = std::move(other);
  }

MeshObjects::Mesh &MeshObjects::Mesh::operator =(MeshObjects::Mesh &&other) {
  std::swap(sub,      other.sub);
  std::swap(subCount, other.subCount);
  std::swap(ani,      other.ani);
  std::swap(skeleton, other.skeleton);
  std::swap(binder,   other.binder);
  return *this;
  }

void MeshObjects::Mesh::setObjMatrix(const Tempest::Matrix4x4 &mt) {
  if(ani!=nullptr){
    auto mat=mt;
    mat.translate(ani->rootTr[0],ani->rootTr[1],ani->rootTr[2]);
    for(size_t i=0;i<subCount;++i)
      sub[i].setObjMatrix(mat);
    setObjMatrix(*ani,mt,size_t(-1));
    } else {
    for(size_t i=0;i<subCount;++i)
      sub[i].setObjMatrix(mt);
    }
  if(binder!=nullptr){
    for(size_t i=0;i<binder->bind.size();++i){
      auto id=binder->bind[i];
      if(id>=skeleton->tr.size())
        continue;
      auto mat=mt;
      mat.translate(ani->rootTr[0],ani->rootTr[1],ani->rootTr[2]);
      mat.mul(skeleton->tr[id]);
      sub[i].setObjMatrix(mat);
      }
    }
  }

void MeshObjects::Mesh::setObjMatrix(const ProtoMesh &ani, const Tempest::Matrix4x4 &mt,size_t parent) {
  for(size_t i=0;i<ani.nodes.size();++i)
    if(ani.nodes[i].parentId==parent) {
      auto mat=mt;
      mat.mul(ani.nodes[i].transform);

      auto& node=ani.nodes[i];
      for(size_t r=node.submeshIdB;r<node.submeshIdE;++r)
        sub[r].setObjMatrix(mat);

      if(ani.nodes[i].hasChild)
        setObjMatrix(ani,mat,i);
      }
  }

void MeshObjects::UboDn::setSkeleton(const Skeleton *sk) {
  if(sk==nullptr)
    return;
  for(size_t i=0;i<sk->tr.size();++i)
    skel[i] = sk->tr[i];
  }

void MeshObjects::UboDn::setSkeleton(const Pose& p) {
  std::memcpy(&skel[0],p.tr.data(),p.tr.size()*sizeof(skel[0]));
  }

const Tempest::Texture2d& MeshObjects::Node::texture() const {
  return it->texture();
  }

void MeshObjects::Node::draw(Tempest::Encoder<Tempest::CommandBuffer> &cmd,const Tempest::RenderPipeline &pipeline, uint32_t imgId) const {
  it->draw(cmd,pipeline,imgId);
  }
