#include "protomesh.h"

#include <Tempest/Log>

ProtoMesh::ProtoMesh(const ZenLoad::zCModelMeshLib &library, const std::string &fname) {
  for(auto& m:library.getAttachments()) {
    ZenLoad::PackedMesh stat;
    m.second.packMesh(stat, 1.f);
    attach.emplace_back(stat);
    attach.back().name = m.first;
    }

  nodes.resize(library.getNodes().size());
  for(size_t i=0;i<nodes.size();++i){
    Node& n   = nodes[i];
    auto& src = library.getNodes()[i];
    for(size_t r=0;r<attach.size();++r)
      if(library.getAttachments()[r].first==src.name){
        n.attachId = r;
        break;
        }
    n.parentId = (src.parentIndex==uint16_t(-1) ? size_t(-1) : src.parentIndex);
    std::memcpy(reinterpret_cast<void*>(&n.transform),reinterpret_cast<const void*>(&src.transformLocal),sizeof(n.transform));
    }

  for(auto& i:nodes)
    if(i.parentId<nodes.size())
      nodes[i.parentId].hasChild=true;

  size_t subCount=0;
  for(auto& i:nodes)
    if(i.attachId<attach.size()) {
      attach[i.attachId].hasNode = true;
      subCount+=attach[i.attachId].sub.size();
      }
  for(auto& a:attach)
    if(!a.hasNode)
      subCount+=a.sub.size();
  submeshId.resize(subCount);

  subCount=0;
  for(auto& i:nodes) {
    i.submeshIdB = subCount;
    if(i.attachId<attach.size()) {
      auto& att = attach[i.attachId];
      for(size_t r=0;r<att.sub.size();++r){
        if(att.sub[r].texture==nullptr) {
          if(!att.sub[r].texName.empty())
            Tempest::Log::e("no texture?!");
          continue;
          }
        submeshId[subCount].id    = i.attachId;
        submeshId[subCount].subId = r;
        subCount++;
        }
      }
    i.submeshIdE = subCount;
    }
  firstFreeAttach=subCount;
  for(size_t i=0;i<attach.size();++i){
    auto& att = attach[i];
    if(att.hasNode)
      continue;

    for(size_t r=0;r<att.sub.size();++r){
      if(att.sub[r].texture==nullptr) {
        if(!att.sub[r].texName.empty())
          Tempest::Log::e("no texture?!");
        continue;
        }
      submeshId[subCount].id    = i;
      submeshId[subCount].subId = r;
      subCount++;
      }
    }
  submeshId.resize(subCount);

  for(size_t i=0;i<library.getMeshes().size();++i){
    ZenLoad::PackedSkeletalMesh pack;
    auto& mesh = library.getMeshes()[i];
    mesh.packMesh(pack,1.f);
    skined.emplace_back(pack);
    }

  for(size_t i=0;i<library.getNodes().size();++i) {
    auto& n=library.getNodes()[i];
    if(n.name.find("ZS_POS")==0){
      Pos p;
      p.name = n.name;
      p.node = i;
      std::memcpy(reinterpret_cast<void*>(&p.transform),reinterpret_cast<const void*>(&n.transformLocal),sizeof(p.transform));
      pos.push_back(p);
      }
    }
  setupScheme(fname);

  //auto tr = library.getRootNodeTranslation();
  //rootTr  = {{tr.x,tr.y,tr.z}};

  //bboxCol[0] = library.getBBoxCollisionMax();
  //bboxCol[1] = library.getBBoxCollisionMin();
  }

ProtoMesh::ProtoMesh(const ZenLoad::PackedMesh &pm, const std::string& fname) {
  attach.emplace_back(pm);
  submeshId.resize(attach[0].sub.size());
  auto&  att   = attach[0];
  size_t count = 0;
  for(size_t r=0;r<att.sub.size();++r) {
    if(att.sub[r].texture==nullptr) {
      if(!att.sub[r].texName.empty())
        Tempest::Log::e("no texture?!");
      continue;
      }
    submeshId[count].id    = 0;
    submeshId[count].subId = r;
    count++;
    }
  submeshId.resize(count);

  nodes.emplace_back();
  nodes.back().attachId   = 0;
  nodes.back().submeshIdB = 0;
  nodes.back().submeshIdE = submeshId.size();
  nodes.back().transform.identity();

  bbox[0] = Tempest::Vec3(pm.bbox[0].x,pm.bbox[0].y,pm.bbox[0].z);
  bbox[1] = Tempest::Vec3(pm.bbox[1].x,pm.bbox[1].y,pm.bbox[1].z);
  setupScheme(fname);
  }

ProtoMesh::ProtoMesh(const std::string& fname, std::vector<Resources::Vertex> vbo, std::vector<uint32_t> ibo) {
  attach.emplace_back(fname,std::move(vbo),std::move(ibo));
  submeshId.resize(attach[0].sub.size());
  auto&  att   = attach[0];
  size_t count = 0;
  for(size_t r=0;r<att.sub.size();++r) {
    if(att.sub[r].texture==nullptr) {
      if(!att.sub[r].texName.empty())
        Tempest::Log::e("no texture?!");
      continue;
      }
    submeshId[count].id    = 0;
    submeshId[count].subId = r;
    count++;
    }
  submeshId.resize(count);

  nodes.emplace_back();
  nodes.back().attachId   = 0;
  nodes.back().submeshIdB = 0;
  nodes.back().submeshIdE = submeshId.size();
  nodes.back().transform.identity();

  //bbox[0] = Tempest::Vec3(pm.bbox[0].x,pm.bbox[0].y,pm.bbox[0].z);
  //bbox[1] = Tempest::Vec3(pm.bbox[1].x,pm.bbox[1].y,pm.bbox[1].z);
  setupScheme("");
  }

size_t ProtoMesh::skinedNodesCount() const {
  size_t ret=0;
  for(auto& i:skined)
    ret+=i.sub.size();
  return ret;
  }

Tempest::Matrix4x4 ProtoMesh::mapToRoot(size_t n) const {
  Tempest::Matrix4x4 m;
  m.identity();

  while(n<nodes.size()){
    auto& nx = nodes[n];
    auto  mx = nx.transform;
    mx.mul(m);
    m = mx;
    n = nx.parentId;
    }
  return m;
  }

void ProtoMesh::setupScheme(const std::string &s) {
  auto sep = s.find("_");
  if(sep!=std::string::npos) {
    scheme = s.substr(0,sep);
    return;
    }
  sep = s.rfind(".");
  if(sep!=std::string::npos) {
    scheme = s.substr(0,sep);
    return;
    }
  scheme = s;
  }
