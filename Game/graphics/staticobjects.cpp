#include "staticobjects.h"

#include <Tempest/Log>
#include "rendererstorage.h"

StaticObjects::StaticObjects(const RendererStorage &storage)
  :storage(storage) {
  auto& device=storage.device;

  pf.reset(new PerFrame[device.maxFramesInFlight()]);
  for(size_t i=0;i<device.maxFramesInFlight();++i) {
    pf[i].uboData = device.loadUbo(&uboGlobal,sizeof(uboGlobal));
    }

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

StaticObjects::Obj StaticObjects::get(const StaticMesh &mesh, const Tempest::Texture2d *mat, const Tempest::IndexBuffer<uint32_t>& ibo) {  
  auto& device=storage.device;

  for(auto& i:chunks)
    if(i.tex==mat)
      return implGet(i,mesh,ibo);

  chunks.emplace_back(Chunk(mat,device.caps().minUboAligment,device.maxFramesInFlight()));
  auto& last = chunks.back();
  last.pf.reset(new PerFrame[device.maxFramesInFlight()]);

  for(size_t i=0;i<device.maxFramesInFlight();++i)
    last.pf[i].ubo = device.uniforms(storage.uboObjLayout());
  return implGet(last,mesh,ibo);
  }

StaticObjects::Mesh StaticObjects::get(const StaticMesh &mesh) {
  std::unique_ptr<Obj[]> dat(new Obj[mesh.sub.size()]);
  for(size_t i=0;i<mesh.sub.size();++i){
    dat[i] = get(mesh,mesh.sub[i].texture,mesh.sub[i].ibo);
    }
  return Mesh(nullptr,std::move(dat),mesh.sub.size());
  }

StaticObjects::Mesh StaticObjects::get(const AnimMesh &mesh) {
  const size_t skinnedCount=mesh.skinedNodesCount();
  std::unique_ptr<Obj[]> dat(new Obj[mesh.submeshId.size()+skinnedCount]);

  size_t count=0;
  for(auto& m:mesh.submeshId){
    auto& att = mesh.attach[m.id].mesh;
    auto& s   = att.sub[m.subId];

    if(s.texture!=nullptr) {
      dat[count] = get(att,s.texture,s.ibo);
      ++count;
      } else {
      Tempest::Log::e("no texture?!");
      }
    }

  for(auto& skin:mesh.skined){
    for(auto& m:skin.mesh.sub){
      if(m.texture!=nullptr) {
        dat[count] = get(skin.mesh,m.texture,m.ibo);
        ++count;
        } else {
        Tempest::Log::e("no texture?!");
        }
      }
    }
  return Mesh(&mesh,std::move(dat),count);
  }

void StaticObjects::setMatrix(uint32_t imgId) {
  auto& device=storage.device;

  for(auto& i:chunks){
    auto& frame=i.pf[imgId];
    size_t sz = i.obj.byteSize();
    if(frame.uboData.size()!=sz)
      frame.uboData = device.loadUbo(i.obj.data(),sz); else
      frame.uboData.update(i.obj.data(),0,sz);
    }
  auto& frame=pf[imgId];
  frame.uboData.update(&uboGlobal,0,sizeof(uboGlobal));
  }

void StaticObjects::commitUbo(uint32_t imgId) {
  auto& device=storage.device;

  for(auto& i:chunks){
    auto& frame=i.pf[imgId];
    if(!frame.uboChanged)
      continue;
    frame.uboChanged=false;

    size_t sz = i.obj.byteSize();
    if(frame.uboData.size()!=sz)
      frame.uboData = device.loadUbo(i.obj.data(),sz); else
      frame.uboData.update(i.obj.data(),0,sz);

    frame.ubo.set(0,this->pf[imgId].uboData,0,sizeof(uboGlobal));
    frame.ubo.set(1,frame.uboData,0,i.obj.elementSize());
    frame.ubo.set(2,*i.tex);
    }

  auto& frame=pf[imgId];
  frame.uboData.update(&uboGlobal,0,sizeof(uboGlobal));
  }

void StaticObjects::draw(Tempest::CommandBuffer &cmd,uint32_t imgId, const StaticObjects::Obj &obj) {
  auto&    frame =obj.owner->pf[imgId];
  uint32_t offset=obj.id*obj.owner->obj.elementSize();

  cmd.setUniforms(storage.pObject,frame.ubo,1,&offset);
  cmd.draw(obj.mesh->vbo,*obj.owner->data[obj.id].ibo);
  }

void StaticObjects::draw(Tempest::CommandBuffer &cmd, uint32_t imgId) {
  for(auto& c:chunks) {
    auto& frame = c.pf[imgId];
    for(size_t i=0;i<c.data.size();++i){
      auto&    obj    = c.data[i];
      uint32_t offset = i*c.obj.elementSize();

      cmd.setUniforms(storage.pObject,frame.ubo,1,&offset);
      cmd.draw(*obj.vbo,*obj.ibo);
      }
    }
  }

StaticObjects::Obj StaticObjects::implGet(StaticObjects::Chunk &owner,const StaticMesh& mesh, const Tempest::IndexBuffer<uint32_t>& ibo) {
  nToUpdate=true;

  if(owner.freeList.size()>0){
    owner.markAsChanged();
    size_t id=owner.freeList.back();
    owner.data[id].vbo = &mesh.vbo;
    owner.data[id].ibo = &ibo;
    owner.freeList.pop_back();
    return Obj(owner,id,&mesh);
    }

  owner.obj .resize(owner.obj.size()+1);
  owner.data.resize(owner.obj.size());
  owner.markAsChanged();

  owner.data.back().vbo = &mesh.vbo;
  owner.data.back().ibo = &ibo;
  return Obj(owner,owner.obj.size()-1,&mesh);
  }

StaticObjects::Obj::~Obj() {
  if(owner)
    owner->free(*this);
  }

void StaticObjects::Obj::setObjMatrix(const Tempest::Matrix4x4 &mt) {
  for(uint32_t i=0;i<owner->pfSize;++i)
    owner->pf[i].uboChanged=true;
  owner->obj[id].obj=mt;
  }

const Tempest::Matrix4x4 &StaticObjects::Obj::objMatrix() {
  return owner->obj[id].obj;
  }

void StaticObjects::Chunk::free(const StaticObjects::Obj &obj) {
  markAsChanged();
  this->obj[obj.id].obj = Tempest::Matrix4x4();
  freeList.emplace_back(obj.id);
  }

void StaticObjects::Chunk::markAsChanged() {
  for(uint32_t i=0;i<pfSize;++i)
    pf[i].uboChanged=true;
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
  }

void StaticObjects::Mesh::setObjMatrix(const AnimMesh &ani, const Tempest::Matrix4x4 &mt,size_t parent) {
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
