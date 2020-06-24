#include "meshobjects.h"

#include <Tempest/Log>

#include "skeleton.h"
#include "pose.h"
#include "attachbinder.h"
#include "light.h"
#include "objectsbucket.h"
#include "rendererstorage.h"
#include "bounds.h"
#include "sceneglobals.h"

MeshObjects::MeshObjects(const SceneGlobals& globals)
  :globals(globals) {
  }

MeshObjects::~MeshObjects() {
  }

ObjectsBucket& MeshObjects::getBucket(const Material& mat, ObjectsBucket::Type type) {
  for(auto& i:buckets)
    if(i.material()==mat && i.type()==type)
      return i;

  index.clear();
  if(type==ObjectsBucket::Type::Static)
    buckets.emplace_back(mat,globals,uboStatic,type); else
    buckets.emplace_back(mat,globals,uboDyn,   type);
  return buckets.back();
  }

MeshObjects::Item MeshObjects::implGet(const StaticMesh &mesh, const StaticMesh::SubMesh& s,
                                       int32_t texVar, int32_t teethTex, int32_t bodyColor,
                                       bool staticDraw) {
  Material mat = s.material;
  if(teethTex!=0 || bodyColor!=0 || texVar!=0){
    if(s.texName=="HUM_TEETH_V0.TGA"){
      mat.tex=solveTex(s.material.tex,s.texName,teethTex,bodyColor);
      }
    else if(s.texName=="HUM_MOUTH_V0.TGA"){
      mat.tex=solveTex(s.material.tex,s.texName,teethTex,bodyColor);
      }
    else if(s.texName.find_first_of("VC")!=std::string::npos){
      mat.tex=solveTex(s.material.tex,s.texName,texVar,bodyColor);
      }
    }

  if(mat.tex==nullptr) {
    if(!s.texName.empty())
      Tempest::Log::e("texture not found: \"",s.texName,"\"");
    return MeshObjects::Item();
    }
  return implGet(mesh,mat,s.ibo,staticDraw);
  }

MeshObjects::Item MeshObjects::implGet(const StaticMesh &mesh, const Material& mat,
                                       const Tempest::IndexBuffer<uint32_t>& ibo,
                                       bool staticDraw) {
  if(mat.tex==nullptr) {
    Tempest::Log::e("no texture?!");
    return MeshObjects::Item();
    }
  auto&        bucket = getBucket(mat,staticDraw ? ObjectsBucket::Static : ObjectsBucket::Movable);
  const size_t id     = bucket.alloc(mesh.vbo,ibo,mesh.bbox);
  return Item(bucket,id);
  }

MeshObjects::Item MeshObjects::implGet(const AnimMesh &mesh, const Material& mat,
                                       const Tempest::IndexBuffer<uint32_t> &ibo) {
  if(mat.tex==nullptr) {
    Tempest::Log::e("no texture?!");
    return MeshObjects::Item();
    }
  auto&        bucket = getBucket(mat,ObjectsBucket::Animated);
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

MeshObjects::Mesh MeshObjects::get(const StaticMesh &mesh, bool staticDraw) {
  std::unique_ptr<Item[]> dat(new Item[mesh.sub.size()]);
  size_t count=0;
  for(size_t i=0;i<mesh.sub.size();++i){
    Item it = implGet(mesh,mesh.sub[i].material,mesh.sub[i].ibo,staticDraw);
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
    Item it = implGet(mesh,mesh.sub[i],headTexVar,teethTex,bodyColor,false);
    if(it.isEmpty())
      continue;
    dat[count] = std::move(it);
    ++count;
    }
  return Mesh(nullptr,std::move(dat),count);
  }

MeshObjects::Mesh MeshObjects::get(const ProtoMesh& mesh, int32_t texVar, int32_t teethTex, int32_t bodyColor,
                                   bool staticDraw) {
  const size_t skinnedCount=mesh.skinedNodesCount();
  std::unique_ptr<Item[]> dat(new Item[mesh.submeshId.size()+skinnedCount]);

  size_t count=0;
  for(auto& m:mesh.submeshId) {
    auto& att = mesh.attach[m.id];
    auto& s   = att.sub[m.subId];
    Item  it  = implGet(att,s,texVar,teethTex,bodyColor,staticDraw);
    if(it.isEmpty())
      continue;
    dat[count] = std::move(it);
    count++;
    }

  for(auto& skin:mesh.skined) {
    for(auto& m:skin.sub){
      Material mat = m.material;
      mat.tex=solveTex(mat.tex,m.texName,texVar,bodyColor);
      if(mat.tex!=nullptr) {
        dat[count] = implGet(skin,mat,m.ibo);
        ++count;
        } else {
        if(!m.texName.empty())
          Tempest::Log::e("texture not found: \"",m.texName,"\"");
        }
      }
    }
  return Mesh(&mesh,std::move(dat),count);
  }

MeshObjects::Mesh MeshObjects::get(Tempest::VertexBuffer<Resources::Vertex>& vbo,
                                   Tempest::IndexBuffer<uint32_t>&           ibo,
                                   const Material& mat, const Bounds& bbox) {
  auto&        bucket = getBucket(mat,ObjectsBucket::Static);
  const size_t id     = bucket.alloc(vbo,ibo,bbox);

  std::unique_ptr<Item[]> dat(new Item[1]);
  dat[0] = Item(bucket,id);

  return Mesh(nullptr,std::move(dat),1);
  }

void MeshObjects::setupUbo() {
  for(auto& c:buckets)
    c.setupUbo();
  }

void MeshObjects::draw(Painter3d& painter, uint8_t fId) {
  commitUbo(fId);
  mkIndex();

  for(auto c:index)
    c->visibilityPass(painter);
  for(auto c:index)
    c->draw(painter,fId);
  for(auto c:index)
    c->drawLight(painter,fId);
  }

void MeshObjects::drawShadow(Painter3d& painter, uint8_t fId, int layer) {
  commitUbo(fId);
  mkIndex();

  for(auto c:index)
    c->visibilityPass(painter);
  for(auto c:index)
    c->drawShadow(painter,fId,layer);
  }

void MeshObjects::mkIndex() {
  if(index.size()!=0)
    return;
  index.reserve(buckets.size());
  index.resize(buckets.size());
  size_t id=0;
  for(auto& i:buckets) {
    index[id] = &i;
    ++id;
    }
  std::sort(index.begin(),index.end(),[](const ObjectsBucket* l,const ObjectsBucket* r){
    return l->material()<r->material();
    });
  }

void MeshObjects::commitUbo(uint8_t fId) {
  bool st = uboStatic.commitUbo(globals.storage.device,fId);
  bool dn = uboDyn   .commitUbo(globals.storage.device,fId);

  if(!st && !dn)
    return;
  for(auto& c:buckets)
    c.setupPerFrameUbo();
  }

void MeshObjects::Mesh::setSkeleton(const Skeleton *sk) {
  skeleton = sk;
  if(ani!=nullptr && skeleton!=nullptr)
    binder=Resources::bindMesh(*ani,*skeleton);
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

void MeshObjects::Node::draw(Painter3d& p, uint8_t fId) const {
  it->draw(p,fId);
  }
