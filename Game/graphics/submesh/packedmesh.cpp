#include "packedmesh.h"

#include <Tempest/Log>

#include <algorithm>

using namespace Tempest;

PackedMesh::PackedMesh(const ZenLoad::zCMesh& mesh, PkgType type) {
  mesh.getBoundingBox(bbox[0],bbox[1]);
  if(type==PK_Visual) {
    subMeshes.resize(mesh.getMaterials().size());
    for(size_t i=0;i<subMeshes.size();++i)
      subMeshes[i].material = mesh.getMaterials()[i];
    }

  if(type==PK_Physic) {
    subMeshes.resize(ZenLoad::MaterialGroup::NUM_MAT_GROUPS);
    for(size_t i=0;i<subMeshes.size();++i)
      subMeshes[i].material.matGroup = uint8_t(i);
    }

  if(type==PK_PhysicZoned) {
    subMeshes.resize(ZenLoad::MaterialGroup::NUM_MAT_GROUPS);
    for(size_t i=0;i<subMeshes.size();++i)
      subMeshes[i].material.matGroup = uint8_t(i);

    for(size_t i=0;i<mesh.getMaterials().size();++i) {
      auto& m = mesh.getMaterials()[i];
      if(m.matName.find(':')!=std::string::npos) {
        if(m.noCollDet)
          continue;
        addSector(m.matName,m.matGroup);
        }
      }
    }

  pack(mesh,type);
  }

void PackedMesh::pack(const ZenLoad::zCMesh& mesh,PkgType type) {
  auto& vbo = mesh.getVertices();
  auto& uv  = mesh.getFeatureIndices();
  auto& ibo = mesh.getIndices();

  struct Hash final {
    size_t operator() (const std::pair<uint32_t,uint32_t>& v) const noexcept {
      return v.first^v.second;
      }
    };

  vertices.reserve(vbo.size());

  size_t prevTriId = size_t(-1);
  size_t matId     = 0;

  std::vector<SubMesh*> index;
  if(type==PK_PhysicZoned) {
    for(size_t i=ZenLoad::MaterialGroup::NUM_MAT_GROUPS;i<subMeshes.size();++i)
      index.push_back(&subMeshes[i]);
    std::sort(index.begin(),index.end(),[](const SubMesh* l,const SubMesh* r){
      return compare(l->material,r->material);
      });
    }

  std::unordered_map<std::pair<uint32_t,uint32_t>,size_t,Hash> icache;
  auto& mid = mesh.getTriangleMaterialIndices();

  for(size_t i=0;i<ibo.size();++i) {
    size_t id    = size_t(mid[i/3]);

    if(id!=prevTriId) {
      matId     = submeshIndex(mesh,index,id,type);
      prevTriId = id;
      }

    if(matId>=subMeshes.size())
      continue;
    auto& s = subMeshes[matId];

    std::pair<uint32_t,uint32_t> index={ibo[i], (type==PK_Physic ? 0 : uv[i])};
    auto r = icache.find(index);
    if(r!=icache.end()) {
      s.indices.push_back(uint32_t(r->second));
      } else {
      auto&       v  = mesh.getFeatures()[index.second];
      WorldVertex vx = {};

      vx.Position = vbo[index.first];
      vx.Normal   = v.vertNormal;
      vx.TexCoord = ZMath::float2(v.uv[0], v.uv[1]);
      vx.Color    = v.lightStat;

      size_t val = vertices.size();
      vertices.emplace_back(vx);
      s.indices.push_back(uint32_t(val));
      icache[index] = uint32_t(val);
      }
    }
  }

size_t PackedMesh::submeshIndex(const ZenLoad::zCMesh& mesh,std::vector<SubMesh*>& index,
                                size_t triangleId,PkgType type) const {
  size_t id  = triangleId;
  switch(type) {
    case PK_Visual:
      return id;
    case PK_Physic: {
      const auto&  mat = mesh.getMaterials()[id];
      if(mat.noCollDet)
        return size_t(-1);
      return size_t(mat.matGroup);
      }
    case PK_PhysicZoned: {
      const auto&  mat = mesh.getMaterials()[id];
      auto l = std::lower_bound(index.begin(),index.end(),&mat,[](const SubMesh* l,const ZenLoad::zCMaterialData* r){
        return compare(l->material,*r);
        });
      auto u = std::upper_bound(index.begin(),index.end(),&mat,[](const ZenLoad::zCMaterialData* l,const SubMesh* r){
        return compare(*l,r->material);
        });
      for(auto i=l;i!=u;++i) {
        const auto& mesh = **i;
        if(mesh.material.matGroup==mat.matGroup && mesh.material.matName==mat.matName) {
          return size_t(std::distance(&subMeshes.front(),&mesh));
          }
        }
      if(mat.noCollDet)
        return size_t(-1);
      return size_t(mat.matGroup);
      }
    }
  return 0;
  }

void PackedMesh::addSector(const std::string& s, uint8_t group) {
  for(auto& i:subMeshes)
    if(i.material.matName==s && i.material.matGroup==group)
      return;

  subMeshes.emplace_back();
  auto& m = subMeshes.back();

  m.material.matName  = s;
  m.material.matGroup = group;
  }

bool PackedMesh::compare(const ZenLoad::zCMaterialData& l, const ZenLoad::zCMaterialData& r) {
  if(l.matGroup<r.matGroup)
    return true;
  if(l.matGroup>r.matGroup)
    return false;
  return l.matName<r.matName;
  }

