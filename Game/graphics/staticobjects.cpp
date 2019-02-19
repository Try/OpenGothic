#include "pose.h"
#include "skeleton.h"
#include "staticobjects.h"

#include <Tempest/Log>

#include "attachbinder.h"
#include "rendererstorage.h"

StaticObjects::StaticObjects(const RendererStorage &storage)
  :storage(storage),storageSt(storage.device),storageDn(storage.device),uboGlobalPf(storage.device) {
  uboGlobal.modelView.identity();
  uboGlobal.lightDir={{1,1,-1}};
  float l=0;
  for(auto i:uboGlobal.lightDir)
    l+=(i*i);
  l = std::sqrt(l);
  for(auto& i:uboGlobal.lightDir)
    i/=l;
  }

bool StaticObjects::needToUpdateCommands() const {
  return nToUpdate;
  }

void StaticObjects::setAsUpdated() {
  nToUpdate=false;
  }

void StaticObjects::setModelView(const Tempest::Matrix4x4 &m) {
  uboGlobal.modelView = m;
  }

ObjectsBucket<StaticObjects::UboSt,Resources::Vertex> &StaticObjects::getBucketSt(const Tempest::Texture2d *mat) {
  auto& device=storage.device;

  for(auto& i:chunksSt)
    if(&i.texture()==mat)
      return i;

  chunksSt.emplace_back(ObjectsBucket<UboSt,Resources::Vertex>(mat,storage.uboObjLayout(),device,storageSt));
  return chunksSt.back();
  }

ObjectsBucket<StaticObjects::UboDn,Resources::VertexA> &StaticObjects::getBucketDn(const Tempest::Texture2d *mat) {
  auto& device=storage.device;

  for(auto& i:chunksDn)
    if(&i.texture()==mat)
      return i;

  chunksDn.emplace_back(ObjectsBucket<UboDn,Resources::VertexA>(mat,storage.uboObjLayout(),device,storageDn));
  return chunksDn.back();
  }

StaticObjects::Item StaticObjects::implGet(const StaticMesh &mesh, const Tempest::Texture2d *mat,
                                           const Tempest::IndexBuffer<uint32_t>& ibo) {
  nToUpdate=true;

  auto&        bucket = getBucketSt(mat);
  const size_t id     = bucket.alloc(mesh.vbo,ibo);
  return Item(bucket,id);
  }

StaticObjects::Item StaticObjects::implGet(const AnimMesh &mesh, const Tempest::Texture2d *mat,
                                           const Tempest::IndexBuffer<uint32_t> &ibo) {
  nToUpdate=true;

  auto&        bucket = getBucketDn(mat);
  const size_t id     = bucket.alloc(mesh.vbo,ibo);
  return Item(bucket,id);
  }

const Tempest::Texture2d *StaticObjects::solveTex(const Tempest::Texture2d *def, const std::string &format, int32_t v, int32_t c) {
  if(format.find_first_of("VC")!=std::string::npos){
    auto ntex = Resources::loadTexture(format,v,c);
    if(ntex!=nullptr)
      return ntex;
    }
  return def;
  }

StaticObjects::Mesh StaticObjects::get(const StaticMesh &mesh) {
  std::unique_ptr<Item[]> dat(new Item[mesh.sub.size()]);
  for(size_t i=0;i<mesh.sub.size();++i){
    dat[i] = implGet(mesh,mesh.sub[i].texture,mesh.sub[i].ibo);
    }
  return Mesh(nullptr,std::move(dat),mesh.sub.size());
  }

StaticObjects::Mesh StaticObjects::get(const ProtoMesh& mesh, int32_t texVar, int32_t teethTex, int32_t bodyColor) {
  const size_t skinnedCount=mesh.skinedNodesCount();
  std::unique_ptr<Item[]> dat(new Item[mesh.submeshId.size()+skinnedCount]);

  size_t count=0;
  for(auto& m:mesh.submeshId) {
    auto& att = mesh.attach[m.id];
    auto& s   = att.sub[m.subId];

    const Tempest::Texture2d* tex=s.texture;
    if(teethTex!=0 || bodyColor!=0 || texVar!=0){
      if(s.texName=="HUM_TEETH_V0.TGA"){
        tex=solveTex(s.texture,s.texName,teethTex,bodyColor);
        }
      else if(s.texName.find_first_of("VC")!=std::string::npos){
        tex=solveTex(s.texture,s.texName,texVar,bodyColor);
        }
      }

    if(tex!=nullptr) {
      dat[count] = implGet(att,tex,s.ibo);
      ++count;
      } else {
      Tempest::Log::e("no texture?!");
      }
    }

  for(auto& skin:mesh.skined){
    for(auto& m:skin.sub){
      const Tempest::Texture2d* tex=solveTex(m.texture,m.texName,texVar,bodyColor);
      if(tex!=nullptr) {
        dat[count] = implGet(skin,tex,m.ibo);
        ++count;
        } else {
        Tempest::Log::e("no texture?!");
        }
      }
    }
  return Mesh(&mesh,std::move(dat),count);
  }

void StaticObjects::updateUbo(uint32_t imgId) {
  storageSt.updateUbo(imgId);
  storageDn.updateUbo(imgId);

  uboGlobalPf.update(uboGlobal,imgId);
  }

void StaticObjects::commitUbo(uint32_t imgId) {
  auto& device=storage.device;

  storageSt.commitUbo(device,imgId);
  storageDn.commitUbo(device,imgId);

  for(auto& i:chunksSt){
    auto& ubo = i.getUbo(imgId);
    ubo.set(0,uboGlobalPf[imgId],0,sizeof(UboGlobal));
    ubo.set(1,storageSt[imgId],0,storageSt.elementSize());
    ubo.set(2,i.texture());
    }
  for(auto& i:chunksDn){
    auto& ubo = i.getUbo(imgId);
    ubo.set(0,uboGlobalPf[imgId],0,sizeof(UboGlobal));
    ubo.set(1,storageDn[imgId],0,storageDn.elementSize());
    ubo.set(2,i.texture());
    }
  }

void StaticObjects::reserve(size_t stat, size_t dyn) {
  storageSt.reserve(stat);
  storageDn.reserve(dyn);
  }

void StaticObjects::draw(Tempest::CommandBuffer &cmd, uint32_t imgId) {
  for(auto& c:chunksSt)
    c.draw(cmd,storage.pObject,imgId);
  for(auto& c:chunksDn)
    c.draw(cmd,storage.pAnim,imgId);
  }

void StaticObjects::Mesh::setSkeleton(const Skeleton *sk, const char *defBone) {
  skeleton = sk;

  if(ani!=nullptr && sk!=nullptr)
    binder=Resources::bindMesh(*ani,*sk,defBone);

  for(size_t i=0;i<subCount;++i)
    sub[i].setSkeleton(sk);
  }

void StaticObjects::Mesh::setSkeleton(const Pose &p,const Tempest::Matrix4x4& obj) {
  for(size_t i=0;i<subCount;++i)
    sub[i].setSkeleton(p);

  if(binder!=nullptr){
    for(size_t i=0;i<binder->bind.size();++i){
      auto id=binder->bind[i].boneId;
      if(id>=p.tr.size())
        continue;
      auto mat=obj;
      mat.translate(ani->rootTr[0],ani->rootTr[1],ani->rootTr[2]);
      mat.mul(p.tr[id]);
      sub[i].setObjMatrix(mat);
      }
    }
  }

void StaticObjects::Mesh::setObjMatrix(const Tempest::Matrix4x4 &mt) {
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
      auto id=binder->bind[i].boneId;
      if(id>=skeleton->tr.size())
        continue;
      auto mat=mt;
      mat.translate(ani->rootTr[0],ani->rootTr[1],ani->rootTr[2]);
      mat.mul(skeleton->tr[id]);
      sub[i].setObjMatrix(mat);
      }
    }
  }

void StaticObjects::Mesh::setObjMatrix(const ProtoMesh &ani, const Tempest::Matrix4x4 &mt,size_t parent) {
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

void StaticObjects::UboDn::setSkeleton(const Skeleton *sk) {
  if(sk==nullptr)
    return;
  for(size_t i=0;i<sk->tr.size();++i)
    skel[i] = sk->tr[i];
  }

void StaticObjects::UboDn::setSkeleton(const Pose& p) {
  std::memcpy(&skel[0],p.tr.data(),p.tr.size()*sizeof(skel[0]));
  }
