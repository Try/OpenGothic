#include "meshobjects.h"

#include <Tempest/Log>

#include "skeleton.h"
#include "pose.h"
#include "attachbinder.h"
#include "light.h"
#include "rendererstorage.h"

MeshObjects::MeshObjects(const RendererStorage &storage)
  :storage(storage),storageSt(storage.device),storageDn(storage.device),uboGlobalPf{storage.device,storage.device} {
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

bool MeshObjects::needToUpdateCommands() const {
  for(auto& i:chunksSt)
    if(i.needToUpdateCommands())
      return true;
  for(auto& i:chunksDn)
    if(i.needToUpdateCommands())
      return true;
  return false;
  }

void MeshObjects::setAsUpdated() {
  for(auto& i:chunksSt)
    i.setAsUpdated();
  for(auto& i:chunksDn)
    i.setAsUpdated();
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

MeshObjects::Item MeshObjects::implGet(const StaticMesh &mesh, const Tempest::Texture2d *mat,
                                           const Tempest::IndexBuffer<uint32_t>& ibo) {
  auto&        bucket = getBucketSt(mat);
  const size_t id     = bucket.alloc(mesh.vbo,ibo);
  return Item(bucket,id);
  }

MeshObjects::Item MeshObjects::implGet(const AnimMesh &mesh, const Tempest::Texture2d *mat,
                                           const Tempest::IndexBuffer<uint32_t> &ibo) {
  auto&        bucket = getBucketDn(mat);
  const size_t id     = bucket.alloc(mesh.vbo,ibo);
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
  for(size_t i=0;i<mesh.sub.size();++i){
    dat[i] = implGet(mesh,mesh.sub[i].texture,mesh.sub[i].ibo);
    }
  return Mesh(nullptr,std::move(dat),mesh.sub.size());
  }

MeshObjects::Mesh MeshObjects::get(const ProtoMesh& mesh, int32_t texVar, int32_t teethTex, int32_t bodyColor) {
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
      else if(s.texName=="HUM_MOUTH_V0.TGA"){
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

void MeshObjects::updateUbo(uint32_t imgId) {
  storageSt.updateUbo(imgId);
  storageDn.updateUbo(imgId);

  uboGlobalPf[0].update(uboGlobal,imgId);

  auto ubo2 = uboGlobal;
  ubo2.shadowView = shadowView1;
  uboGlobalPf[1].update(ubo2,imgId);
  }

void MeshObjects::commitUbo(uint32_t imgId,const Tempest::Texture2d& shadowMap) {
  auto& device=storage.device;

  storageSt.commitUbo(device,imgId);
  storageDn.commitUbo(device,imgId);

  for(auto& i:chunksSt){
    auto& ubo = i.uboMain(imgId);
    ubo.set(0,uboGlobalPf[0][imgId],0,1);
    ubo.set(1,storageSt[imgId],0,1);
    ubo.set(2,i.texture());
    ubo.set(3,shadowMap);
    }
  for(auto& i:chunksDn){
    auto& ubo = i.uboMain(imgId);
    ubo.set(0,uboGlobalPf[0][imgId],0,1);
    ubo.set(1,storageDn[imgId],0,1);
    ubo.set(2,i.texture());
    ubo.set(3,shadowMap);
    }

  for(int layer=0;layer<2;++layer) {
    for(auto& i:chunksSt){
      auto& ubo = i.uboShadow(imgId,layer);
      ubo.set(0,uboGlobalPf[layer][imgId],0,1);
      ubo.set(1,storageSt[imgId],0,1);
      ubo.set(2,i.texture());
      ubo.set(3,Resources::fallbackTexture());
      }
    for(auto& i:chunksDn){
      auto& ubo = i.uboShadow(imgId,layer);
      ubo.set(0,uboGlobalPf[layer][imgId],0,1);
      ubo.set(1,storageDn[imgId],0,1);
      ubo.set(2,i.texture());
      ubo.set(3,Resources::fallbackTexture());
      }
    }
  }

void MeshObjects::reserve(size_t stat, size_t dyn) {
  storageSt.reserve(stat);
  storageDn.reserve(dyn);
  }

void MeshObjects::draw(Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint32_t imgId) {
  for(auto& c:chunksSt)
    c.draw(cmd,storage.pObject,imgId);
  for(auto& c:chunksDn)
    c.draw(cmd,storage.pAnim,imgId);
  }

void MeshObjects::drawShadow(Tempest::Encoder<Tempest::CommandBuffer> &cmd, uint32_t imgId, int layer) {
  for(auto& c:chunksSt)
    c.drawShadow(cmd,storage.pObjectSh,imgId,layer);
  for(auto& c:chunksDn)
    c.drawShadow(cmd,storage.pAnimSh,imgId,layer);
  }

void MeshObjects::Mesh::setAttachPoint(const Skeleton *sk, const char *defBone) {
  skeleton = sk;

  if(ani!=nullptr && skeleton!=nullptr)
    binder=Resources::bindMesh(*ani,*skeleton,defBone);

  for(size_t i=0;i<subCount;++i)
    sub[i].setSkeleton(sk);
  }

void MeshObjects::Mesh::setSkeleton(const Pose &p,const Tempest::Matrix4x4& obj) {
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

const char* MeshObjects::Mesh::attachPoint() const {
  if(binder==nullptr)
    return nullptr;
  return binder->defBone.c_str();
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
