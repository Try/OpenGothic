#include "packedmesh.h"

#include <Tempest/Log>
#include <fstream>
#include <algorithm>

#include "graphics/bounds.h"

using namespace Tempest;

void PackedMesh::Meshlet::flush(std::vector<WorldVertex>& vertices,
                                std::vector<uint32_t>&     indices,
                                SubMesh& sub, const ZenLoad::zCMesh& mesh) {
  if(indSz==0)
    return;
  auto& vbo = mesh.getVertices();  // xyz
  auto& uv  = mesh.getFeatures();  // uv, normal

  size_t vboSz = vertices.size();
  size_t iboSz = indices.size();
  vertices.resize(vboSz+MaxVert);
  indices .resize(iboSz+MaxInd );

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
    indices[iboSz+i] = uint32_t(vboSz)+indexes[i];
    }
  for(size_t i=indSz; i<MaxInd; ++i) {
    // padd with degenerated triangles
    indices[iboSz+i] = uint32_t(vboSz);
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
  indSz           += 3u;

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
  vertSz = vertSz+other.vertSz;
  }


PackedMesh::PackedMesh(const ZenLoad::zCMesh& mesh, PkgType type) {
  if(type==PK_VisualLnd || type==PK_Visual) {
    packMeshlets(mesh);
    return;
    }

  if(type==PK_Physic) {
    packPhysics(mesh,type);
    return;
    }
  }

void PackedMesh::packPhysics(const ZenLoad::zCMesh& mesh, PkgType type) {
  auto& vbo = mesh.getVertices();
  auto& ibo = mesh.getIndices();
  vertices.reserve(vbo.size());

  std::unordered_map<uint32_t,size_t> icache;
  auto& mid = mesh.getTriangleMaterialIndices();
  auto& mat = mesh.getMaterials();

  for(size_t i=0; i<ZenLoad::MaterialGroup::NUM_MAT_GROUPS; ++i) {
    SubMesh sub;
    sub.material.matName  = "";
    sub.material.matGroup = ZenLoad::MaterialGroup(i);
    sub.iboOffset         = indices.size();

    for(size_t r=0; r<ibo.size(); ++r) {
      auto& m = mat[size_t(mid[r/3u])];
      if(m.matGroup!=i || m.noCollDet)
        continue;
      if(m.matName.find(':')!=std::string::npos)
        continue; // named material - add them later

      uint32_t index = ibo[r];
      auto     rx    = icache.find(index);
      if(rx!=icache.end()) {
        indices.push_back(uint32_t(rx->second));
        } else {
        WorldVertex vx = {};
        vx.Position = vbo[index];

        size_t val = vertices.size();
        vertices.emplace_back(vx);
        indices.push_back(uint32_t(val));
        icache[index] = uint32_t(val);
        }
      }

    sub.iboLength = indices.size()-sub.iboOffset;
    subMeshes.emplace_back(std::move(sub));
    }

  for(size_t i=0; i<mat.size(); ++i) {
    auto& m = mat[i];
    if(m.noCollDet)
      continue;
    if(m.matName.find(':')==std::string::npos)
      continue; // only named materials

    SubMesh sub;
    sub.material.matName  = m.matName;
    sub.material.matGroup = m.matGroup;
    sub.iboOffset         = indices.size();

    for(size_t r=0; r<ibo.size(); ++r) {
      if(size_t(mid[r/3u])!=i)
        continue;
      uint32_t index = ibo[r];
      auto     rx    = icache.find(index);
      if(rx!=icache.end()) {
        indices.push_back(uint32_t(rx->second));
        } else {
        WorldVertex vx = {};
        vx.Position = vbo[index];

        size_t val = vertices.size();
        vertices.emplace_back(vx);
        indices.push_back(uint32_t(val));
        icache[index] = uint32_t(val);
        }
      }
    sub.iboLength = indices.size()-sub.iboOffset;
    if(sub.iboLength>0)
      subMeshes.emplace_back(std::move(sub));
    }
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
    postProcessP1(mesh,mId,meshlets);
    }
  }

void PackedMesh::postProcessP1(const ZenLoad::zCMesh& mesh, size_t matId, std::vector<Meshlet>& meshlets) {
  if(meshlets.size()<=1) {
    SubMesh sub;
    sub.material  = mesh.getMaterials()[matId];
    sub.iboOffset = indices.size();
    for(auto& m:meshlets)
      m.flush(vertices,indices,sub,mesh);
    sub.iboLength = indices.size()-sub.iboOffset;
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
  sub.material  = mesh.getMaterials()[matId];
  sub.iboOffset = indices.size();

  auto prev = meshlets[0];
  for(size_t i=0; i<meshlets.size(); ++i) {
    auto meshlet = meshlets[i];
    if(prev!=nullptr && !meshlet->hasIntersection(*prev)) {
      sub.iboLength = indices.size()-sub.iboOffset;
      subMeshes.push_back(std::move(sub));
      sub = SubMesh();
      sub.material  = mesh.getMaterials()[matId];
      sub.iboOffset = indices.size();
      }
    meshlet->flush(vertices,indices,sub,mesh);
    prev = meshlet;
    }
  sub.iboLength = indices.size()-sub.iboOffset;

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
    for(size_t i=0; i<s.iboLength; i+=3) {
      const uint32_t* tri = &indices[s.iboOffset+i];
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
