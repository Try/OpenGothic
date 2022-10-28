#include "packedmesh.h"

#include <Tempest/Log>
#include <fstream>
#include <algorithm>

#include "game/compatibility/phoenix.h"
#include "gothic.h"

using namespace Tempest;

void PackedMesh::Meshlet::flush(std::vector<Vertex>& vertices,
                                std::vector<uint32_t>& indices,
                                std::vector<Bounds>& instances,
                                SubMesh& sub,
                                const phoenix::mesh& mesh) {
  if(indSz==0)
    return;
  instances.push_back(bounds);

  auto& vbo = mesh.vertices;  // xyz
  auto& uv  = mesh.features;  // uv, normal

  size_t vboSz = vertices.size();
  size_t iboSz = indices.size();
  vertices.resize(vboSz+MaxVert);
  indices .resize(iboSz+MaxInd );

  for(size_t i=0; i<vertSz; ++i) {
    Vertex vx = {};
    auto& v     = uv [vert[i].second];

    vx.pos[0]  = vbo[vert[i].first].x;
    vx.pos[1]  = vbo[vert[i].first].y;
    vx.pos[2]  = vbo[vert[i].first].z;
    vx.norm[0] = v.normal.x;
    vx.norm[1] = v.normal.y;
    vx.norm[2] = v.normal.z;
    vx.uv[0]   = v.texture.x;
    vx.uv[1]   = v.texture.y;
    vx.color   = v.light;
    vertices[vboSz+i] = vx;
  }
  for(size_t i=vertSz; i<MaxVert; ++i) {
    Vertex vx = {};
    vertices[vboSz+i] = vx;
  }

  for(size_t i=0; i<indSz; ++i) {
    indices[iboSz+i] = uint32_t(vboSz)+indexes[i];
  }
  for(size_t i=indSz; i<MaxInd; ++i) {
    // padd with degenerated triangles
    indices[iboSz+i] = uint32_t(vboSz+indSz/3);
  }
}

void PackedMesh::Meshlet::flush(std::vector<Vertex>& vertices, std::vector<VertexA>& verticesA,
           std::vector<uint32_t>& indices, std::vector<uint32_t>* verticesId,
           SubMesh& sub, const std::vector<glm::vec3>& vboList,
           const std::vector<phoenix::wedge>& wedgeList,
           const std::vector<SkeletalData>* skeletal) {
  if(indSz==0)
    return;

  auto& vbo = vboList;    // xyz
  auto& uv  = wedgeList;  // uv, normal

  size_t vboSz = 0;
  size_t iboSz = indices.size();

  if(skeletal==nullptr) {
    vboSz = vertices.size();
    vertices.resize(vboSz+MaxVert);
  } else {
    vboSz = verticesA.size();
    verticesA.resize(vboSz+MaxVert);
  }

  indices.resize(iboSz+MaxInd );
  if(verticesId!=nullptr)
    verticesId->resize(vboSz+MaxVert);

  if(skeletal==nullptr) {
    for(size_t i=0; i<vertSz; ++i) {
      Vertex vx = {};
      auto& v     = uv [vert[i].second];
      vx.pos[0]   = vbo[vert[i].first].x;
      vx.pos[1]   = vbo[vert[i].first].y;
      vx.pos[2]   = vbo[vert[i].first].z;
      vx.norm[0] = v.normal.x;
      vx.norm[1] = v.normal.y;
      vx.norm[2] = v.normal.z;
      vx.uv[0]   = v.texture.x;
      vx.uv[1]   = v.texture.y;
      vx.color    = 0xFFFFFFFF;
      vertices[vboSz+i]   = vx;
      if(verticesId!=nullptr)
        (*verticesId)[vboSz+i] = vert[i].first;
    }
    for(size_t i=vertSz; i<MaxVert; ++i) {
      Vertex vx = {};
      vertices[vboSz+i] = vx;
      if(verticesId!=nullptr)
        (*verticesId)[vboSz+i] = uint32_t(-1);
    }
  } else {
    auto& sk = *skeletal;
    for(size_t i=0; i<vertSz; ++i) {
      VertexA vx = {};
      auto& v    = uv [vert[i].second];
      vx.norm[0] = v.normal.x;
      vx.norm[1] = v.normal.y;
      vx.norm[2] = v.normal.z;
      vx.uv[0]   = v.texture.x;
      vx.uv[1]   = v.texture.y;
      vx.color   = 0xFFFFFFFF;
      for(int r=0; r<4; ++r) {
        vx.pos    [r][0] = sk[vert[i].first].localPositions[r].x;
        vx.pos    [r][1] = sk[vert[i].first].localPositions[r].y;
        vx.pos    [r][2] = sk[vert[i].first].localPositions[r].z;
        vx.boneId [r]    = sk[vert[i].first].boneIndices[r];
        vx.weights[r]    = sk[vert[i].first].weights[r];
      }
      verticesA[vboSz+i]  = vx;
    }
    for(size_t i=vertSz; i<MaxVert; ++i) {
      VertexA vx = {};
      verticesA[vboSz+i] = vx;
    }
  }

  for(size_t i=0; i<indSz; ++i) {
    indices[iboSz+i] = uint32_t(vboSz)+indexes[i];
  }
  for(size_t i=indSz; i<MaxInd; ++i) {
    // padd with degenerated triangles
    indices[iboSz+i] = uint32_t(vboSz+indSz/3);
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

  if(vertSz>0) {
    if(matchHint==2) {
      if((ea==MaxVert && eb==MaxVert) || (ea==MaxVert && ec==MaxVert) || (eb==MaxVert && ec==MaxVert)) {
        return false;
        }
      }
    if(matchHint==1) {
      if(!(ea!=MaxVert || eb!=MaxVert || ec!=MaxVert))
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
  indSz            = uint8_t(indSz+3u);

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

void PackedMesh::Meshlet::updateBounds(const phoenix::mesh& mesh) {
  updateBounds(mesh.vertices);
}

void PackedMesh::Meshlet::updateBounds(const phoenix::proto_mesh& mesh) {
  updateBounds(mesh.positions);
}

void PackedMesh::Meshlet::updateBounds(const std::vector<glm::vec3>& vbo) {
  float dim = 0;
  for(size_t i=0; i<vertSz; ++i)
    for(size_t r=i+1; r<vertSz; ++r) {
      auto a = vbo[vert[i].first];
      auto b = vbo[vert[r].first];
      a.x -= b.x;
      a.y -= b.y;
      a.z -= b.z;
      float d = a.x*a.x + a.y*a.y + a.z*a.z;
      if(dim<d) {
        bounds.pos = Vec3(b.x,b.y,b.z)+Vec3(a.x,a.y,a.z)*0.5f;
        dim = d;
      }
    }
  bounds.r = 0;
  for(size_t i=0; i<vertSz; ++i) {
    auto a = vbo[vert[i].first];
    a.x -= bounds.pos.x;
    a.y -= bounds.pos.y;
    a.z -= bounds.pos.z;
    float d = (a.x*a.x + a.y*a.y + a.z*a.z);
    if(bounds.r<d)
      bounds.r = d;
  }
  bounds.r = std::sqrt(bounds.r);
}

bool PackedMesh::Meshlet::canMerge(const Meshlet& other) const {
  if(vertSz+other.vertSz>MaxVert)
    return false;
  if(indSz+other.indSz>MaxInd)
    return false;
  return true;
  }

bool PackedMesh::Meshlet::hasIntersection(const Meshlet& other) const {
  return (bounds.pos-other.bounds.pos).quadLength() < std::pow(bounds.r+other.bounds.r,2.f);
  }

float PackedMesh::Meshlet::qDistance(const Meshlet& other) const {
  return (bounds.pos-other.bounds.pos).quadLength();
  }

void PackedMesh::Meshlet::merge(const Meshlet& other) {
  for(uint8_t i=0; i<other.indSz; ++i) {
    uint8_t index  = uint8_t(vertSz+other.indexes[i]);
    indexes[indSz] = index;
    vert[index]    = other.vert[other.indexes[i]];
    ++indSz;
    }
  vertSz = uint8_t(vertSz+other.vertSz);
  }

static bool isSame(const phoenix::material& a, const phoenix::material& b) {
  if( a.name                         != b.name ||
      a.group                        != b.group ||
      a.color                        != b.color ||
      a.smooth_angle                 != b.smooth_angle ||
      a.texture                      != b.texture ||
      a.texture_scale                != b.texture_scale ||
      a.texture_anim_fps             != b.texture_anim_fps ||
      a.texture_anim_map_mode        != b.texture_anim_map_mode ||
      a.texture_anim_map_dir         != b.texture_anim_map_dir ||
      a.disable_collision            != b.disable_collision ||
      a.disable_lightmap             != b.disable_lightmap ||
      a.dont_collapse                != b.dont_collapse ||
      a.detail_object                != b.detail_object ||
      a.detail_texture_scale         != b.detail_texture_scale ||
      a.force_occluder               != b.force_occluder ||
      a.environment_mapping          != b.environment_mapping ||
      a.environment_mapping_strength != b.environment_mapping_strength ||
      a.wave_mode                    != b.wave_mode ||
      a.wave_speed                   != b.wave_speed ||
      a.wave_max_amplitude           != b.wave_max_amplitude ||
      a.wave_grid_size               != b.wave_grid_size ||
      a.ignore_sun                   != b.ignore_sun ||
      a.alpha_func                   != b.alpha_func)
    return false;
  return true;
}

PackedMesh::PackedMesh(const phoenix::mesh& mesh, PkgType type) {
  if(Gothic::inst().doMeshShading()) {
    auto& prop = Resources::device().properties().meshlets;
    maxIboSliceLength = prop.maxMeshGroups * prop.maxMeshGroupSize * MaxInd;
  } else {
    maxIboSliceLength = 8*1024*3;
  }

  if(type==PK_VisualLnd || type==PK_Visual) {
    packMeshlets(mesh);
    computeBbox();
    return;
  }

  if(type==PK_Physic) {
    packPhysics(mesh,type);
    return;
  }
}

PackedMesh::PackedMesh(const phoenix::proto_mesh& mesh, PkgType type) {
  subMeshes.resize(mesh.sub_meshes.size());
  isUsingAlphaTest = mesh.alpha_test;
  {
    auto bbox = mesh.bbox;
    mBbox[0] = Vec3(bbox.min.x,bbox.min.y,bbox.min.z);
    mBbox[1] = Vec3(bbox.max.x,bbox.max.y,bbox.max.z);
  }

  packMeshlets(mesh,type,nullptr);
}

PackedMesh::PackedMesh(const phoenix::softskin_mesh&  skinned) {
  auto& mesh = skinned.mesh;
  subMeshes.resize(mesh.sub_meshes.size());
  {
    auto bbox = phoenix_compat::get_total_aabb(skinned);
    mBbox[0] = Vec3(bbox.min.x,bbox.min.y,bbox.min.z);
    mBbox[1] = Vec3(bbox.max.x,bbox.max.y,bbox.max.z);
  }

  std::vector<SkeletalData> vertices(mesh.positions.size());
  // Extract weights and local positions
  auto& stream = skinned.weights;
  for(size_t i=0; i<vertices.size(); ++i) {
    auto& vert = vertices[i];

    for(size_t j=0; j<stream[i].size(); j++) {
      auto& weight = stream[i][j];

      vert.boneIndices[j]    = weight.node_index;
      vert.localPositions[j] = {weight.position.x, weight.position.y, weight.position.z};
      vert.weights[j]        = weight.weight;
    }
  }

  packMeshlets(mesh,PK_Visual,&vertices);
  }

void PackedMesh::packPhysics(const phoenix::mesh& mesh, PkgType type) {
  auto& vbo = mesh.vertices;
  auto& ibo = mesh.polygons.vertex_indices;
  vertices.reserve(vbo.size());

  std::unordered_map<uint32_t,size_t> icache;
  auto& mid = mesh.polygons.material_indices;
  auto& mat = mesh.materials;

  phoenix::material_group mats[] = {
      phoenix::material_group::undefined,
      phoenix::material_group::metal,
      phoenix::material_group::stone,
      phoenix::material_group::wood,
      phoenix::material_group::earth,
      phoenix::material_group::water,
      phoenix::material_group::snow,
      phoenix::material_group::none,
  };

  for(auto i : mats) {
    SubMesh sub;
    sub.material.name  = "";
    sub.material.group = i;
    sub.iboOffset         = indices.size();

    for(size_t r=0; r<ibo.size(); ++r) {
      auto& m = mat[size_t(mid[r/3u])];
      if(m.group!=i || m.disable_collision)
        continue;
      if(m.name.find(':')!=std::string::npos)
        continue; // named material - add them later

      uint32_t index = ibo[r];
      auto     rx    = icache.find(index);
      if(rx!=icache.end()) {
        indices.push_back(uint32_t(rx->second));
      } else {
        Vertex vx = {};
        vx.pos[0] = vbo[index].x;
        vx.pos[1] = vbo[index].y;
        vx.pos[2] = vbo[index].z;

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

  for(size_t i=0; i<mat.size(); ++i) {
    auto& m = mat[i];
    if(m.disable_collision)
      continue;
    if(m.name.find(':')==std::string::npos)
      continue; // only named materials

    SubMesh sub;
    sub.material.name  = m.name;
    sub.material.group = m.group;
    sub.iboOffset      = indices.size();

    for(size_t r=0; r<ibo.size(); ++r) {
      if(size_t(mid[r/3u])!=i)
        continue;
      uint32_t index = ibo[r];
      auto     rx    = icache.find(index);
      if(rx!=icache.end()) {
        indices.push_back(uint32_t(rx->second));
      } else {
        Vertex vx = {};
        vx.pos[0] = vbo[index].x;
        vx.pos[1] = vbo[index].y;
        vx.pos[2] = vbo[index].z;

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

void PackedMesh::packMeshlets(const phoenix::mesh& mesh) {
  auto& ibo  = mesh.polygons.vertex_indices;
  auto& feat = mesh.polygons.feature_indices;
  auto& mat  = mesh.polygons.material_indices;

  std::vector<size_t> duplicates(mesh.materials.size());
  for(size_t i=0; i<mesh.materials.size(); ++i)
    duplicates[i] = i;

  if(!Gothic::inst().doMeshShading()) {
    for(size_t i=0; i<mesh.materials.size(); ++i) {
      if(duplicates[i]!=i)
        continue;
      duplicates[i] = i;
      for(size_t r=i+1; r<mesh.materials.size(); ++r) {
        auto& a = mesh.materials[i];
        auto& b = mesh.materials[r];
        if(!isSame(a,b))
          continue;
        duplicates[r] = i;
      }
    }
  }

  for(size_t mId=0; mId<mesh.materials.size(); ++mId) {
    std::vector<Meshlet> meshlets;
    Meshlet activeMeshlets[16];

    if(duplicates[mId]!=mId)
      continue;

    for(size_t i=0; i<ibo.size(); i+=3) {
      if(duplicates[size_t(mat[i/3u])]!=mId)
        continue;
      auto a = std::make_pair(ibo[i+0],feat[i+0]);
      auto b = std::make_pair(ibo[i+1],feat[i+1]);
      auto c = std::make_pair(ibo[i+2],feat[i+2]);
      addIndex(activeMeshlets,16,meshlets, a,b,c);
    }

    for(auto& meshlet:activeMeshlets)
      if(meshlet.indSz>0)
        meshlets.push_back(std::move(meshlet));
    postProcessP1(mesh,mId,meshlets);
  }
}

void PackedMesh::packMeshlets(const phoenix::proto_mesh& mesh, PkgType type,
                             const std::vector<SkeletalData>* skeletal) {
  auto* vId = (type==PK_VisualMorph) ? &verticesId : nullptr;

  for(size_t mId=0; mId<mesh.sub_meshes.size(); ++mId) {
    auto& sm   = mesh.sub_meshes[mId];
    auto& pack = subMeshes[mId];

    pack.material = sm.mat;

    std::vector<Meshlet> meshlets;
    Meshlet activeMeshlets[16];
    for(size_t i=0; i<sm.triangles.size(); ++i) {
      const uint16_t* ibo = sm.triangles[i].wedges;

      Vert vert[3];
      for(int x=0; x<3; ++x) {
        auto& wedge = sm.wedges[ibo[x]];
        vert[x] = std::make_pair(wedge.index,ibo[x]);
      }
      addIndex(activeMeshlets,16,meshlets, vert[0],vert[1],vert[2]);
    }

    for(auto& meshlet:activeMeshlets)
      if(meshlet.indSz>0)
        meshlets.push_back(std::move(meshlet));

    std::vector<Meshlet*> ind(meshlets.size());
    for(size_t i=0; i<meshlets.size(); ++i) {
      meshlets[i].updateBounds(mesh);
      ind[i] = &meshlets[i];
    }
    mergePass(ind,false);

    pack.iboOffset = indices.size();
    for(auto& i:ind) {
      i->updateBounds(mesh);
      i->flush(vertices,verticesA,indices,vId,pack,mesh.positions,sm.wedges,skeletal);
    }
    pack.iboLength = indices.size() - pack.iboOffset;
  }
}

void PackedMesh::sortPass(std::vector<Meshlet*>& meshlets) {
  if(meshlets.size()<2)
    return;

  float    d = -1;
  Meshlet* a = nullptr;
  Meshlet* b = nullptr;

  if(meshlets.size()>1000) {
    Vec3 minP = meshlets[0]->bounds.pos;
    Vec3 maxP = meshlets[0]->bounds.pos;
    for(size_t i=0; i<meshlets.size(); ++i) {
      auto p = meshlets[i]->bounds.pos;
      auto r = meshlets[i]->bounds.r;

      minP.x = std::min(p.x-r, minP.x);
      minP.y = std::min(p.y-r, minP.y);
      minP.z = std::min(p.z-r, minP.z);
      maxP.x = std::max(p.x+r, maxP.x);
      maxP.y = std::max(p.y+r, maxP.y);
      maxP.z = std::max(p.z+r, maxP.z);
      }

    const float clusterSz = clusterRadius*2;
    // const int   lenX      = int(std::ceil((maxP.x-minP.x)/clusterSz));
    const int   lenY      = int(std::ceil((maxP.y-minP.y)/clusterSz));
    const int   lenZ      = int(std::ceil((maxP.z-minP.z)/clusterSz));

    std::sort(meshlets.begin(),meshlets.end(),[&](const Meshlet* l, const Meshlet* r) {
      auto lp  = (l->bounds.pos-minP)/clusterSz;
      auto rp  = (r->bounds.pos-minP)/clusterSz;

      int  lId = (int(lp.x)*lenY + int(lp.y))*lenZ + int(lp.z);
      int  rId = (int(rp.x)*lenY + int(rp.y))*lenZ + int(rp.z);

      return lId<rId;
      });
    return;
    }

  for(size_t i=0; i<meshlets.size(); ++i)
    for(size_t r=i+1; r<meshlets.size(); ++r) {
      auto dx = meshlets[i]->bounds.pos - meshlets[r]->bounds.pos;
      if(dx.quadLength()>d) {
        a = meshlets[i];
        b = meshlets[r];
        }
      }
  Vec3 orig = a->bounds.pos;
  Vec3 axis = Vec3::normalize(b->bounds.pos - a->bounds.pos);
  std::sort(meshlets.begin(),meshlets.end(),[=](const Meshlet* l, const Meshlet* r) {
    float lx = Vec3::dotProduct(axis, l->bounds.pos - orig);
    float rx = Vec3::dotProduct(axis, r->bounds.pos - orig);
    return lx<rx;
    });
  }

void PackedMesh::mergePass(std::vector<Meshlet*>& ind, bool fast) {
  for(size_t i=0; i<ind.size(); ++i) {
    auto mesh = ind[i];
    if(mesh==nullptr)
      continue;
    if(mesh->indSz>=MaxInd-3 || mesh->vertSz>=MaxVert-3)
      continue;
    for(size_t r=i+1; r<ind.size(); ++r) {
      if(ind[r]==nullptr)
        continue;
      if(!mesh->canMerge(*ind[r])) {
        if(fast)
          break;
        continue;
        }
      float qDist = mesh->qDistance(*ind[r]);
      float R     = (mesh->bounds.r+ind[r]->bounds.r)*0.5f;
      if(qDist>(500.f*500.f) && qDist>R*R) {
        // 5 meters or radius
        if(fast)
          break;
        continue;
        }
      mesh->merge(*ind[r]);
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
  }

void PackedMesh::postProcessP1(const phoenix::mesh& mesh, size_t matId, std::vector<Meshlet>& meshlets) {
  if(meshlets.size()<=1) {
    SubMesh sub;
    sub.material  = mesh.materials[matId];
    sub.iboOffset = indices.size();
    for(auto& m:meshlets) {
      m.updateBounds(mesh);
      m.flush(vertices,indices,meshletBounds,sub,mesh);
    }
    sub.iboLength = indices.size()-sub.iboOffset;
    if(sub.iboLength>0)
      subMeshes.push_back(std::move(sub));
    return;
  }

  std::vector<Meshlet*> ind(meshlets.size());
  for(size_t i=0; i<meshlets.size(); ++i) {
    meshlets[i].updateBounds(mesh);
    ind[i] = &meshlets[i];
  }

  // merge
  sortPass(ind);
  mergePass(ind,false);

  for(auto i:ind)
    i->updateBounds(mesh);

  postProcessP2(mesh,matId,ind);
  // dbgUtilization(ind);
  // dbgMeshlets(mesh,ind);
  }

void PackedMesh::postProcessP2(const phoenix::mesh& mesh, size_t matId, std::vector<Meshlet*>& meshlets) {
  if(meshlets.empty())
    return;

  //const bool hasMeshShaders = Gothic::inst().doMeshShading();

  SubMesh sub;
  sub.material  = mesh.materials[matId];
  sub.iboOffset = indices.size();

  auto prev = meshlets[0];
  for(size_t i=0; i<meshlets.size(); ++i) {
    auto meshlet  = meshlets[i];
    bool overflow = (indices.size()-sub.iboOffset+MaxInd > maxIboSliceLength);
    bool disjoint = !meshlet->hasIntersection(*prev) && (indices.size()-sub.iboOffset>4096*3);

    if(disjoint || overflow) {
      sub.iboLength = indices.size()-sub.iboOffset;
      if(sub.iboLength>0)
        subMeshes.push_back(std::move(sub));
      sub = SubMesh();
      sub.material  = mesh.materials[matId];
      sub.iboOffset = indices.size();
    }
    meshlet->updateBounds(mesh);
    meshlet->flush(vertices,indices,meshletBounds,sub,mesh);
    prev = meshlet;
  }
  sub.iboLength = indices.size()-sub.iboOffset;
  if(sub.iboLength>0)
    subMeshes.push_back(std::move(sub));
}

void PackedMesh::debug(std::ostream &out) const {
  for(auto& i:vertices) {
    out << "v  " << i.pos[0]  << " " << i.pos[1]  << " " << i.pos[2]  << std::endl;
    out << "vn " << i.norm[0] << " " << i.norm[1] << " " << i.norm[2] << std::endl;
    out << "vt " << i.uv[0]   << " " << i.uv[1]  << std::endl;
    }

  for(auto& s:subMeshes) {
    out << "o " << s.material.name << std::endl;
    for(size_t i=0; i<s.iboLength; i+=3) {
      const uint32_t* tri = &indices[s.iboOffset+i];
      out << "f " << 1+tri[0] << " " << 1+tri[1] << " " << 1+tri[2] << std::endl;
      }
    }

  }

std::pair<Vec3, Vec3> PackedMesh::bbox() const {
  return std::make_pair(mBbox[0],mBbox[1]);
  }

void PackedMesh::computeBbox() {
  if(vertices.size()==0) {
    mBbox[0] = Vec3();
    mBbox[1] = Vec3();
    return;
    }

  mBbox[0].x = vertices[0].pos[0];
  mBbox[0].y = vertices[0].pos[1];
  mBbox[0].z = vertices[0].pos[2];
  mBbox[1] = mBbox[0];

  for(size_t i=1; i<vertices.size(); ++i) {
    mBbox[0].x = std::min(mBbox[0].x, vertices[i].pos[0]);
    mBbox[0].y = std::min(mBbox[0].y, vertices[i].pos[1]);
    mBbox[0].z = std::min(mBbox[0].z, vertices[i].pos[2]);

    mBbox[1].x = std::max(mBbox[1].x, vertices[i].pos[0]);
    mBbox[1].y = std::max(mBbox[1].y, vertices[i].pos[1]);
    mBbox[1].z = std::max(mBbox[1].z, vertices[i].pos[2]);
    }
  }

void PackedMesh::dbgUtilization(const std::vector<Meshlet*>& meshlets) {
  size_t used = 0, allocated = 0;
  for(auto i:meshlets) {
    used      += i->indSz;
    allocated += MaxInd;
    }

  float procent = (float(used*100)/float(allocated));
  Log::d("Meshlet usage: ", procent," %");
  if(procent<25)
    Log::d("");
  }

void PackedMesh::dbgMeshlets(const phoenix::mesh& mesh, const std::vector<Meshlet*>& meshlets) {
  std::ofstream out("dbg.obj");

  size_t off = 1;
  auto&  vbo = mesh.vertices;
  for(auto i:meshlets) {
    out << "o meshlet" << off <<" " << i->bounds.r << std::endl;
    for(size_t r=0; r<i->vertSz; ++r) {
      auto& v = vbo[i->vert[r].first];
      out << "v " << v.x << " " << v.y << " " << v.z << std::endl;
      }
    for(size_t r=0; r<i->indSz; r+=3) {
      auto tri = &i->indexes[r];
      out << "f " << off+tri[0] << " " << off+tri[1] << " " << off+tri[2] << std::endl;
      }
    off += i->vertSz;
    }
  }

void PackedMesh::addIndex(Meshlet* active, size_t numActive, std::vector<Meshlet>& meshlets,
                          const Vert& a, const Vert& b, const Vert& c) {
  for(size_t r=0; r<MaxMeshlets; ++r) {
    auto& meshlet = active[r];
    if(meshlet.insert(a,b,c,2)) {
      return;
      }
    }
  for(size_t r=0; r<MaxMeshlets; ++r) {
    auto& meshlet = active[r];
    if(meshlet.insert(a,b,c,1)) {
      return;
      }
    }

  size_t smallest = 0;
  size_t biggest  = 0;
  for(size_t r=0; r<MaxMeshlets; ++r) {
    if(active[r].indSz>active[biggest].indSz) {
      biggest = r;
      }
    if(active[r].indSz<active[biggest].indSz) {
      smallest = r;
      }
    }

  {
  auto& meshlet = active[smallest];
  if(meshlet.indSz<MaxInd-3 && meshlet.vertSz<MaxVert-3) {
    meshlet.insert(a,b,c,0);
    return;
    }
  }

  auto& meshlet = active[biggest];
  if(meshlet.indSz<MaxInd-3 && meshlet.vertSz<MaxVert-3) {
    meshlet.insert(a,b,c,0);
    return;
    }
  meshlets.push_back(std::move(meshlet));
  meshlet.clear();
  meshlet.insert(a,b,c,0);
  }

