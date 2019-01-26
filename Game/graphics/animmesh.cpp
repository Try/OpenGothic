#include "animmesh.h"

#include <Tempest/Log>

AnimMesh::AnimMesh(const ZenLoad::zCModelMeshLib &library) {
  static_assert(sizeof(Vertex)==sizeof(ZenLoad::SkeletalVertex),"invalid vertex format");

  for(auto& m:library.getAttachments()) {
    ZenLoad::PackedMesh stat;
    m.second.packMesh(stat, 1.f);
    attach.emplace_back(stat);
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
    n.parentId = (src.parentIndex==uint16_t(-1) ? size_t(-1) : 0);
    std::memcpy(&n.transform,&src.transformLocal,sizeof(n.transform));
    }

  for(auto& i:nodes)
    if(i.parentId<nodes.size())
      nodes[i.parentId].hasChild=true;

  size_t subCount=0;
  for(auto& i:nodes)
    if(i.attachId<attach.size())
      subCount+=attach[i.attachId].mesh.sub.size();
  submeshId.resize(subCount);

  subCount=0;
  for(auto& i:nodes) {
    i.submeshIdB = subCount;
    if(i.attachId<attach.size()) {
      auto& att = attach[i.attachId];
      for(size_t r=0;r<att.mesh.sub.size();++r){
        if(att.mesh.sub[r].texture==nullptr) {
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
  submeshId.resize(subCount);

  auto tr = library.getRootNodeTranslation();
  rootTr = {{tr.x,tr.y,tr.z}};
  }

/*
AnimMesh::AnimMesh(const ZenLoad::zCModelMeshLib &library) {
  ZenLoad::PackedMesh pm;
  for(auto& m:library.getAttachments()) {
    m.second.packMesh(pm, 1.f);
    }

  attach.emplace_back(pm);
  submeshId.resize(attach[0].mesh.sub.size());
  auto&  att   = attach[0];
  size_t count = 0;
  for(size_t r=0;r<att.mesh.sub.size();++r){
    if(att.mesh.sub[r].texture==nullptr) {
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
  }*/

AnimMesh::AnimMesh(const ZenLoad::PackedMesh &pm) {
  attach.emplace_back(pm);
  submeshId.resize(attach[0].mesh.sub.size());
  auto&  att   = attach[0];
  size_t count = 0;
  for(size_t r=0;r<att.mesh.sub.size();++r){
    if(att.mesh.sub[r].texture==nullptr) {
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
  }
