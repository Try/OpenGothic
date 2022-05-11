#include "packedmesh.h"

#include <Tempest/Log>
#include <fstream>
#include <algorithm>

#include "graphics/bounds.h"

using namespace Tempest;

void PackedMesh::Meshlet::flush(std::vector<WorldVertex>& vertices, SubMesh& sub, const ZenLoad::zCMesh& mesh) {
  if(indSz==0)
    return;
  auto& vbo = mesh.getVertices();  // xyz
  auto& uv  = mesh.getFeatures();  // uv, normal

  size_t vboSz = vertices.size();
  size_t iboSz = sub.indices.size();
  vertices   .resize(vboSz+MaxVert);
  sub.indices.resize(iboSz+MaxInd );

  for(size_t i=0; i<vertSz; ++i) {
    WorldVertex vx = {};
    auto& v     = uv [vert[i].second];
    vx.Position = vbo[vert[i].first];
    vx.Normal   = v.vertNormal;
    vx.TexCoord = ZMath::float2(v.uv[0], v.uv[1]);
    vx.Color    = v.lightStat;
    vertices[vboSz+i] = vx;
    }
  for(size_t i=vertSz; i<MaxVert; ++i) {
    vertices[vboSz+i] = WorldVertex();
    }

  for(size_t i=0; i<indSz; ++i) {
    sub.indices[iboSz+i] = uint32_t(vboSz)+indexes[i];
    }
  for(size_t i=indSz; i<MaxInd; ++i) {
    // padd with degenerated triangles
    sub.indices[iboSz+i] = uint32_t(vboSz);
    }
  }

bool PackedMesh::Meshlet::insert(const Vert& a, const Vert& b, const Vert& c, uint8_t matchHint) {
  if(indSz+3>MaxInd)
    return false;

  uint8_t ea = MaxVert, eb = MaxVert, ec = MaxVert;
  for(uint8_t i=0; i<vertSz; ++i) {
    if(vert[i]==a)
      ea = i;
    if(vert[i]==b)
      eb = i;
    if(vert[i]==c)
      ec = i;
    }

  if(vertSz>0 && matchHint==2) {
    if((ea==MaxVert && eb==MaxVert) || (ea==MaxVert && ec==MaxVert) || (eb==MaxVert && ec==MaxVert)) {
      return false;
      }
    }

  uint8_t vSz = vertSz;
  if(ea==MaxVert) {
    ea = vSz;
    ++vSz;
    }
  if(eb==MaxVert) {
    eb = vSz;
    ++vSz;
    }
  if(ec==MaxVert) {
    ec = vSz;
    ++vSz;
    }

  if(vSz>MaxVert)
    return false;

  indexes[indSz+0] = ea;
  indexes[indSz+1] = eb;
  indexes[indSz+2] = ec;
  indSz           += 3;

  vert[ea] = a;
  vert[eb] = b;
  vert[ec] = c;
  vertSz   = vSz;

  return true;
  }

void PackedMesh::Meshlet::clear() {
  vertSz = 0;
  indSz  = 0;
  }

void PackedMesh::Meshlet::updateBounds(const ZenLoad::zCMesh& mesh) {
  auto& vbo = mesh.getVertices();  // xyz
  bounds.r = 0;
  for(size_t i=0; i<vertSz; ++i)
    for(size_t r=i+1; r<vertSz; ++r) {
      auto a = vbo[vert[i].first];
      auto b = vbo[vert[r].first];
      a.x -= b.x;
      a.y -= b.y;
      a.z -= b.z;
      float d = a.x*a.x + a.y*a.y + a.z*a.z;
      if(bounds.r<d) {
        bounds.at = Vec3(b.x,b.y,b.z)+Vec3(a.x,a.y,a.z)*0.5f;
        bounds.r  = d;
        }
      }
  bounds.r = std::sqrt(bounds.r)*0.5f;
  }

bool PackedMesh::Meshlet::canMerge(const Meshlet& other) const {
  if(vertSz+other.vertSz>MaxVert)
    return false;
  if(indSz+other.indSz>MaxInd)
    return false;
  return (bounds.at-other.bounds.at).quadLength() < std::pow(bounds.r+other.bounds.r,2.f);
  }

bool PackedMesh::Meshlet::hasIntersection(const Meshlet& other) const {
  return (bounds.at-other.bounds.at).quadLength() < std::pow(bounds.r+other.bounds.r,2.f);
  }

void PackedMesh::Meshlet::merge(const Meshlet& other) {
  for(uint8_t i=0; i<other.indSz; ++i) {
    uint8_t index  = vertSz+other.indexes[i];
    indexes[indSz] = index;
    vert[index]    = other.vert[other.indexes[i]];
    ++indSz;
    }
  vertSz += other.vertSz;
  }


PackedMesh::PackedMesh(const ZenLoad::zCMesh& mesh, PkgType type) {
  if(type==PK_VisualLnd || type==PK_Visual) {
    packMeshlets(mesh);
    return;
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

void PackedMesh::pack(const ZenLoad::zCMesh& mesh, PkgType type) {
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
  if(type!=PK_Physic && type!=PK_PhysicZoned) {
    vertices.reserve(ibo.size()/2);
    }

  for(size_t i=0; i<ibo.size(); ++i) {
    size_t id    = size_t(mid[i/3]);

    if(id!=prevTriId) {
      matId     = submeshIndex(mesh,index,ibo[i],id,type);
      prevTriId = id;
      }

    if(matId>=subMeshes.size())
      continue;
    auto& s = subMeshes[matId];

    std::pair<uint32_t,uint32_t> index={ibo[i], ((type==PK_Physic || type==PK_PhysicZoned) ? 0 : uv[i])};
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

void PackedMesh::addSector(std::string_view s, uint8_t group) {
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

void PackedMesh::packMeshlets(const ZenLoad::zCMesh& mesh) {
  auto& ibo  = mesh.getIndices();
  auto& feat = mesh.getFeatureIndices();
  auto& mat  = mesh.getTriangleMaterialIndices();

  for(size_t mId=0; mId<mesh.getMaterials().size(); ++mId) {
    std::vector<Meshlet> meshlets;
    Meshlet activeMeshlets[16];

    for(size_t i=0; i<ibo.size(); i+=3) {
      if(mat[i/3]!=int16_t(mId))
        continue;
      auto a = std::make_pair(ibo[i+0],feat[i+0]);
      auto b = std::make_pair(ibo[i+1],feat[i+1]);
      auto c = std::make_pair(ibo[i+2],feat[i+2]);

      bool added = false;
      for(size_t r=0; r<MaxMeshlets; ++r) {
        auto& meshlet = activeMeshlets[r];
        if(meshlet.insert(a,b,c,2)) {
          added = true;
          break;
          }
        }
      if(added)
        continue;

      for(size_t r=0; r<MaxMeshlets; ++r) {
        auto& meshlet = activeMeshlets[r];
        if(meshlet.insert(a,b,c,1)) {
          added = true;
          break;
          }
        }
      if(added)
        continue;

      size_t biggest = 0;
      for(size_t r=0; r<MaxMeshlets; ++r) {
        if(activeMeshlets[r].indSz>activeMeshlets[biggest].indSz) {
          biggest = r;
          }
        }

      auto& meshlet = activeMeshlets[biggest];
      meshlets.push_back(std::move(meshlet));
      meshlet.clear();
      meshlet.insert(a,b,c,0);
      }

    for(auto& meshlet:activeMeshlets)
      if(meshlet.indSz>0)
        meshlets.push_back(std::move(meshlet));
    postProcess(mesh,mId,meshlets);
    }
  }

void PackedMesh::postProcess(const ZenLoad::zCMesh& mesh, size_t matId, std::vector<Meshlet>& meshlets) {
  if(meshlets.size()<=1) {
    SubMesh sub;
    sub.material = mesh.getMaterials()[matId];
    for(auto& m:meshlets)
      m.flush(vertices,sub,mesh);
    subMeshes.push_back(std::move(sub));
    return;
    }

  std::vector<Meshlet*> ind(meshlets.size());
  for(size_t i=0; i<meshlets.size(); ++i) {
    meshlets[i].updateBounds(mesh);
    ind[i] = &meshlets[i];
    }

  {
  float    d = -1;
  Meshlet* a = nullptr;
  Meshlet* b = nullptr;
  for(size_t i=0; i<meshlets.size(); ++i)
    for(size_t r=i+1; r<meshlets.size(); ++r) {
      auto dx = meshlets[i].bounds.at - meshlets[r].bounds.at;
      if(dx.quadLength()>d) {
        a = &meshlets[i];
        b = &meshlets[r];
        }
      }
  Vec3 orig = a->bounds.at;
  Vec3 axis = Vec3::normalize(b->bounds.at - a->bounds.at);
  std::sort(ind.begin(),ind.end(),[=](const Meshlet* l, const Meshlet* r) {
    float lx = Vec3::dotProduct(axis, l->bounds.at - orig);
    float rx = Vec3::dotProduct(axis, r->bounds.at - orig);
    return lx<rx;
    });
  }

  for(size_t i=0; i<ind.size(); ++i) {
    auto mesh = ind[i];
    for(size_t r=i+1; r<ind.size(); ++r) {
      if(!mesh->canMerge(*ind[r]) || !mesh->hasIntersection(*ind[r]))
        break;
      mesh->merge(*ind[r]);
      ++i;
      ind[r] = nullptr;
      }
    }

  size_t n = 0;
  for(size_t i=0; i<ind.size();++i) {
    if(ind[i]!=nullptr) {
      ind[n] = ind[i];
      ++n;
      }
    }
  ind.resize(n);
  postProcessP2(mesh,matId,ind);
  }

void PackedMesh::postProcessP2(const ZenLoad::zCMesh& mesh, size_t matId, std::vector<Meshlet*>& meshlets) {
  if(meshlets.size()==0)
    return;

  SubMesh sub;
  sub.material = mesh.getMaterials()[matId];

  auto prev = meshlets[0];
  for(size_t i=0; i<meshlets.size(); ++i) {
    auto meshlet = meshlets[i];
    if(prev!=nullptr && !meshlet->hasIntersection(*prev)) {
      subMeshes.push_back(std::move(sub));
      sub = SubMesh();
      sub.material = mesh.getMaterials()[matId];
      }
    meshlet->flush(vertices,sub,mesh);
    prev = meshlet;
    }

  subMeshes.push_back(std::move(sub));
  }

void PackedMesh::debug(std::ostream &out) const {
  for(auto& i:vertices) {
    out << "v  " << i.Position.x << " " << i.Position.y << " " << i.Position.z << std::endl;
    out << "vn " << i.Normal.x   << " " << i.Normal.y   << " " << i.Normal.z   << std::endl;
    out << "vt " << i.TexCoord.x << " " << i.TexCoord.y  << std::endl;
    }

  for(auto& s:subMeshes) {
    out << "o " << s.material.matName << std::endl;
    for(size_t i=0; i<s.indices.size(); i+=3) {
      const uint32_t* tri = &s.indices[i];
      out << "f " << 1+tri[0] << " " << 1+tri[1] << " " << 1+tri[2] << std::endl;
      }
    }

  }

std::pair<Vec3, Vec3> PackedMesh::bbox() const {
  Vec3 bbox[2] = {};
  if(vertices.size()==0)
    return std::make_pair(bbox[0],bbox[1]);

  bbox[0].x = vertices[0].Position.x;
  bbox[0].y = vertices[0].Position.y;
  bbox[0].z = vertices[0].Position.z;
  bbox[1] = bbox[0];

  for(size_t i=1; i<vertices.size(); ++i) {
    bbox[0].x = std::min(bbox[0].x, vertices[i].Position.x);
    bbox[0].y = std::min(bbox[0].y, vertices[i].Position.y);
    bbox[0].z = std::min(bbox[0].z, vertices[i].Position.z);

    bbox[1].x = std::max(bbox[1].x, vertices[i].Position.x);
    bbox[1].y = std::max(bbox[1].y, vertices[i].Position.y);
    bbox[1].z = std::max(bbox[1].z, vertices[i].Position.z);
    }

  return std::make_pair(bbox[0],bbox[1]);
  }
