#include "skeleton.h"
#include "staticobjects.h"

#include <Tempest/Log>
#include "rendererstorage.h"

StaticObjects::StaticObjects(const RendererStorage &storage)
  :storage(storage),uboGlobalPf(storage.device) {
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

  chunksSt.emplace_back(ObjectsBucket<UboSt,Resources::Vertex>(mat,storage.uboObjLayout(),device));
  return chunksSt.back();
  }

ObjectsBucket<StaticObjects::UboDn,Resources::VertexA> &StaticObjects::getBucketDn(const Tempest::Texture2d *mat) {
  auto& device=storage.device;

  for(auto& i:chunksDn)
    if(&i.texture()==mat)
      return i;

  chunksDn.emplace_back(ObjectsBucket<UboDn,Resources::VertexA>(mat,storage.uboObjLayout(),device));
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

StaticObjects::Mesh StaticObjects::get(const StaticMesh &mesh) {
  std::unique_ptr<Item[]> dat(new Item[mesh.sub.size()]);
  for(size_t i=0;i<mesh.sub.size();++i){
    dat[i] = implGet(mesh,mesh.sub[i].texture,mesh.sub[i].ibo);
    }
  return Mesh(nullptr,std::move(dat),mesh.sub.size());
  }

StaticObjects::Mesh StaticObjects::get(const ProtoMesh& mesh) {
  const size_t skinnedCount=mesh.skinedNodesCount();
  std::unique_ptr<Item[]> dat(new Item[mesh.submeshId.size()+skinnedCount]);

  size_t count=0;
  for(auto& m:mesh.submeshId) {
    auto& att = mesh.attach[m.id];
    auto& s   = att.sub[m.subId];

    if(s.texture!=nullptr) {
      dat[count] = implGet(att,s.texture,s.ibo);
      ++count;
      } else {
      Tempest::Log::e("no texture?!");
      }
    }

  for(auto& skin:mesh.skined){
    for(auto& m:skin.sub){
      if(m.texture!=nullptr) {
        dat[count] = implGet(skin,m.texture,m.ibo);
        ++count;
        } else {
        Tempest::Log::e("no texture?!");
        }
      }
    }
  return Mesh(&mesh,std::move(dat),count);
  }

void StaticObjects::updateUbo(uint32_t imgId) {
  for(auto& i:chunksSt)
    i.updateUbo(imgId);
  for(auto& i:chunksDn)
    i.updateUbo(imgId);

  uboGlobalPf.update(uboGlobal,imgId);
  }

void StaticObjects::commitUbo(uint32_t imgId) {
  auto& device=storage.device;

  for(auto& i:chunksSt){
    auto& frame=i.pf[imgId];
    if(!frame.uboChanged)
      continue;
    frame.uboChanged=false;

    size_t sz = i.obj.byteSize();
    if(frame.uboData.size()!=sz)
      frame.uboData = device.loadUbo(i.obj.data(),sz); else
      frame.uboData.update(i.obj.data(),0,sz);

    frame.ubo.set(0,uboGlobalPf[imgId],0,sizeof(UboGlobal));
    frame.ubo.set(1,frame.uboData,0,i.obj.elementSize());
    frame.ubo.set(2,i.texture());
    }

  for(auto& i:chunksDn){
    auto& frame=i.pf[imgId];
    if(!frame.uboChanged)
      continue;
    frame.uboChanged=false;

    size_t sz = i.obj.byteSize();
    if(frame.uboData.size()!=sz)
      frame.uboData = device.loadUbo(i.obj.data(),sz); else
      frame.uboData.update(i.obj.data(),0,sz);

    frame.ubo.set(0,uboGlobalPf[imgId],0,sizeof(UboGlobal));
    frame.ubo.set(1,frame.uboData,0,i.obj.elementSize());
    frame.ubo.set(2,i.texture());
    }
  }

void StaticObjects::draw(Tempest::CommandBuffer &cmd, uint32_t imgId) {
  for(auto& c:chunksSt)
    c.draw(cmd,storage.pObject,imgId);
  for(auto& c:chunksDn)
    c.draw(cmd,storage.pAnim,imgId);
  }

void StaticObjects::Mesh::setObjMatrix(const Tempest::Matrix4x4 &mt) {
  if(ani!=nullptr){
    auto mat=mt;
    for(size_t i=0;i<subCount;++i)
      sub[i].setObjMatrix(mat);
    mat.translate(ani->rootTr[0],ani->rootTr[1],ani->rootTr[2]);
    setObjMatrix(*ani,mt,size_t(-1));
    } else {
    for(size_t i=0;i<subCount;++i)
      sub[i].setObjMatrix(mt);
    }
  }

void StaticObjects::Mesh::setSkeleton(const Skeleton *sk) {
  for(size_t i=0;i<subCount;++i)
    sub[i].setSkeleton(sk);
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
  if(sk!=nullptr)
    return;
  for(size_t i=0;i<sk->tr.size();++i)
    skel[i] = sk->tr[i];
  }
