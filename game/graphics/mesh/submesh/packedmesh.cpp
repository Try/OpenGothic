#include "packedmesh.h"

#include <Tempest/Log>

#include <algorithm>

#include "graphics/bounds.h"

using namespace Tempest;

PackedMesh::PackedMesh(const ZenLoad::zCMesh& mesh, PkgType type) {
  mesh.getBoundingBox(bbox[0],bbox[1]);
  if(type==PK_Visual || type==PK_VisualLnd) {
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
  if(type==PK_VisualLnd)
    landRepack();
  }

void PackedMesh::pack(const ZenLoad::zCMesh& mesh,PkgType type) {
  auto& vbo = mesh.getVertices();
  auto& uv  = mesh.getFeatureIndices();
  auto& ibo = mesh.getIndices();

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

  struct Hash final {
    size_t operator() (const std::pair<uint32_t,uint32_t>& v) const noexcept {
      return v.first^v.second;
      }
    };
  std::unordered_map<std::pair<uint32_t,uint32_t>,size_t,Hash> icache;
  auto& mid = mesh.getTriangleMaterialIndices();

  for(size_t i=0;i<ibo.size();++i) {
    size_t id    = size_t(mid[i/3]);

    if(id!=prevTriId) {
      matId     = submeshIndex(mesh,index,ibo[i],id,type);
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
                                size_t /*vindex*/, size_t mat, PkgType type) {
  size_t id = mat;
  switch(type) {
    case PK_Visual:
    case PK_VisualLnd:
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
          return size_t(std::distance<const SubMesh*>(&subMeshes.front(),&mesh));
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

void PackedMesh::landRepack() {
  std::vector<SubMesh> m;

  for(auto& i:subMeshes) {
    if(i.indices.size()==0)
      continue;
    split(m,i);
    }

  subMeshes = std::move(m);
  }

void PackedMesh::split(std::vector<SubMesh>& out, SubMesh& src) {
  static bool avoidMicroMeshes = true;
  if(avoidMicroMeshes && src.indices.size()<2048*3) {
    if(src.indices.size()>0)
      out.push_back(std::move(src));
    return;
    }

  Bounds bbox;
  bbox.assign(vertices,src.indices);
  Vec3 sz = bbox.bboxTr[1]-bbox.bboxTr[0];

  static const float blockSz = 40*100;
  if(sz.x*sz.y*sz.z<blockSz*blockSz*blockSz) {
    if(src.indices.size()>0)
      out.push_back(std::move(src));
    return;
    }

  int axis = 0;
  if(sz.y>sz.x && sz.y>sz.z)
    axis = 1;
  if(sz.z>sz.x && sz.z>sz.y)
    axis = 2;

  for(int pass=0; pass<3; ++pass) {
    SubMesh left, right;
    left .material = src.material;
    right.material = src.material;

    for(size_t i=0; i<src.indices.size(); i+=3) {
      auto& a = vertices[src.indices[i+0]].Position;
      auto& b = vertices[src.indices[i+1]].Position;
      auto& c = vertices[src.indices[i+2]].Position;

      Vec3 at = {a.x+b.x+c.x, a.y+b.y+c.y, a.z+b.z+c.z};
      at/=3;

      bool  cond = false;
      switch(axis) {
        case 0:
          cond = at.x < bbox.midTr.x;
          break;
        case 1:
          cond = at.y < bbox.midTr.y;
          break;
        case 2:
          cond = at.z < bbox.midTr.z;
          break;
        }
      SubMesh& dest = cond ? left : right;
      size_t i0 = dest.indices.size();
      dest.indices.resize(dest.indices.size()+3);

      for(size_t r=0;r<3;++r)
        dest.indices[i0+r] = src.indices[i+r];
      }
    if((left.indices.size()==0 || right.indices.size()==0) && pass!=2) {
      axis = (axis+1)%3;
      continue;
      }

    if(left.indices.size()==0 || right.indices.size()==0) {
      out.push_back(std::move(src));
      return;
      }
    src.indices.clear();
    split(out,left);
    split(out,right);
    return;
    }
  }

