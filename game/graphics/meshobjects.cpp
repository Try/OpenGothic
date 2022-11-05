#include "meshobjects.h"

#include <Tempest/Log>

#include "graphics/mesh/skeleton.h"
#include "graphics/mesh/pose.h"
#include "graphics/mesh/attachbinder.h"
#include "graphics/mesh/protomesh.h"

#include "visualobjects.h"

MeshObjects::MeshObjects(VisualObjects& parent)
  :parent(parent) {
  }

MeshObjects::~MeshObjects() {
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
  return parent.get(mesh,mat,s.iboOffset,s.iboLength,staticDraw);
  }

const Tempest::Texture2d *MeshObjects::solveTex(const Tempest::Texture2d *def, const std::string &format, int32_t v, int32_t c) {
  if(format.find_first_of("VC")!=std::string::npos){
    auto ntex = Resources::loadTexture(format,v,c);
    if(ntex!=nullptr)
      return ntex;
    }
  return def;
  }

void MeshObjects::Mesh::setSkeleton(const Skeleton *sk) {
  skeleton = sk;
  if(proto!=nullptr && skeleton!=nullptr)
    binder=Resources::bindMesh(*proto,*skeleton);
  }

void MeshObjects::Mesh::setPose(const Tempest::Matrix4x4& obj, const Pose &p) {
  if(anim!=nullptr)
    anim->set(p.transform());
  implSetObjMatrix(obj,p.transform());
  }

void MeshObjects::Mesh::setAsGhost(bool g) {
  for(size_t i=0;i<subCount;++i)
    sub[i].setAsGhost(g);
  }

void MeshObjects::Mesh::setFatness(float f) {
  for(size_t i=0;i<subCount;++i)
    sub[i].setFatness(f);
  }

void MeshObjects::Mesh::setWind(phoenix::animation_mode m, float intensity) {
  for(size_t i=0;i<subCount;++i)
    sub[i].setWind(m,intensity);
  }

void MeshObjects::Mesh::startMMAnim(std::string_view anim, float intensity, uint64_t timeUntil) {
  for(size_t i=0;i<subCount;++i)
    sub[i].startMMAnim(anim,intensity,timeUntil);
  }

Bounds MeshObjects::Mesh::bounds() const {
  if(subCount==0)
    return Bounds();
  auto b = node(0).bounds();
  for(size_t i=1; i<subCount; ++i)
    b.assign(b,sub[i].bounds());
  return b;
  }

const PfxEmitterMesh* MeshObjects::Mesh::toMeshEmitter() const {
  if(auto p = proto)
    return Resources::loadEmiterMesh(p->fname);
  return nullptr;
  }

MeshObjects::Mesh::Mesh(MeshObjects::Mesh &&other) {
  *this = std::move(other);
  }

MeshObjects::Mesh::~Mesh() {
  }

MeshObjects::Mesh::Mesh(MeshObjects& owner, const StaticMesh& mesh, int32_t version, bool staticDraw) {
  sub.reset(new Item[mesh.sub.size()]);
  subCount = 0;
  for(size_t i=0;i<mesh.sub.size();++i) {
    // NOTE: light dragon hunter armour
    if(mesh.sub[i].texName.find_first_of("VC")==std::string::npos && version!=0)
      continue;
    Item it = owner.implGet(mesh,mesh.sub[i],0,0,version,staticDraw);
    if(it.isEmpty())
      continue;
    sub[subCount] = std::move(it);
    ++subCount;
    }
  }

MeshObjects::Mesh::Mesh(MeshObjects& owner, const StaticMesh& mesh,
                        int32_t texVar, int32_t teethTex, int32_t bodyColor) {
  sub.reset(new Item[mesh.sub.size()]);
  subCount = 0;
  for(size_t i=0;i<mesh.sub.size();++i){
    Item it = owner.implGet(mesh,mesh.sub[i],texVar,teethTex,bodyColor,false);
    if(it.isEmpty())
      continue;
    sub[subCount] = std::move(it);
    ++subCount;
    }
  }

MeshObjects::Mesh::Mesh(MeshObjects& owner, const ProtoMesh& mesh,
                        int32_t texVar, int32_t teethTex, int32_t bodyColor, bool staticDraw)
  :proto(&mesh) {
  const size_t skinnedCount = mesh.skinedNodesCount();
  sub.reset(new Item[mesh.submeshId.size()+skinnedCount]);
  subCount = 0;

  for(auto& m:mesh.submeshId) {
    auto& att = mesh.attach[m.id];
    auto& sub = att.sub[m.subId];
    Item  it  = owner.implGet(att,sub,texVar,teethTex,bodyColor,staticDraw);
    if(it.isEmpty())
      continue;
    this->sub[subCount] = std::move(it);
    subCount++;
    }

  size_t bonesCount = 0;
  for(size_t i=0; i<mesh.skined.size(); ++i)
    bonesCount = std::max(bonesCount,mesh.skined[i].bonesCount);

  if(bonesCount>0)
    anim.reset(new MatrixStorage::Id(owner.parent.getMatrixes(Tempest::BufferHeap::Upload, bonesCount)));

  for(size_t i=0; i<mesh.skined.size(); ++i) {
    auto& skin = mesh.skined[i];
    for(auto& m:skin.sub) {
      Material mat = m.material;
      mat.tex=owner.solveTex(mat.tex,m.texName,texVar,bodyColor);
      if(mat.tex!=nullptr) {
        sub[subCount] = owner.parent.get(skin,mat,m.iboOffset,m.iboSize,*anim);
        ++subCount;
        } else {
        if(!m.texName.empty())
          Tempest::Log::e("texture not found: \"",m.texName,"\"");
        }
      }
    }

  if(mesh.morph.size()>0) {
    startMMAnim(mesh.morph[0].name,1,uint64_t(-1));
    }
  }

MeshObjects::Mesh &MeshObjects::Mesh::operator =(MeshObjects::Mesh &&other) {
  std::swap(sub,      other.sub);
  std::swap(subCount, other.subCount);
  std::swap(anim,     other.anim);
  std::swap(proto,    other.proto);
  std::swap(skeleton, other.skeleton);
  std::swap(binder,   other.binder);
  return *this;
  }

void MeshObjects::Mesh::setObjMatrix(const Tempest::Matrix4x4 &mt) {
  implSetObjMatrix(mt,skeleton==nullptr ? nullptr : skeleton->tr.data());
  }

void MeshObjects::Mesh::implSetObjMatrix(const Tempest::Matrix4x4& mt, const Tempest::Matrix4x4* tr) {
  const size_t binds = (binder==nullptr ? 0 : binder->bind.size());
  for(size_t i=0; i<binds; ++i) {
    auto id = binder->bind[i];
    sub[i].setObjMatrix(tr[id]);
    }
  for(size_t i=binds; i<subCount; ++i)
    sub[i].setObjMatrix(mt);
  }

void MeshObjects::Node::draw(Tempest::Encoder<Tempest::CommandBuffer>& p, uint8_t fId) const {
  it->draw(p,fId);
  }

const Bounds& MeshObjects::Node::bounds() const {
  return it->bounds();
  }
