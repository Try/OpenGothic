#include "packedmesh.h"

#include <Tempest/Log>
#include <fstream>
#include <algorithm>
#include <unordered_set>

#include "game/compatibility/phoenix.h"
#include "gothic.h"

using namespace Tempest;

static uint64_t mkUInt64(uint32_t a, uint32_t b) {
  return (uint64_t(a)<<32) | uint64_t(b);
  };

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

struct PackedMesh::PrimitiveHeap {
  using value_type = std::pair<uint64_t,uint32_t>;
  using iterator   = std::vector<value_type>::iterator;

  std::vector<std::pair<uint64_t,uint32_t>>  data;

  void   reserve(size_t n) { data.reserve(n*3); }

  void   clear() { data.clear();        }
  size_t size()  { return data.size();  }
  bool   empty() { return data.empty(); }

  void   push_back(const value_type& v) { data.push_back(v); }

  void   sort() {
    std::sort(data.begin(), data.end(), [](const value_type& l, const value_type& r){
      return l.first<r.first;
      });
    }

  iterator begin() { return data.begin(); }
  iterator end()   { return data.end();   }

  iterator erase(iterator& i) {
    return data.erase(i);
    }

  std::pair<iterator,iterator> equal_range(uint64_t v) {
    auto l = std::lower_bound(data.begin(), data.end(), v, [](const value_type& x, uint64_t v){
      return x.first<v;
      });
    auto r = std::upper_bound(data.begin(), data.end(), v, [](uint64_t v, const value_type& x){
      return v<x.first;
      });
    return std::make_pair(l,r);
    }
  };

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

  indices.resize(iboSz+MaxInd);
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

bool PackedMesh::Meshlet::insert(const Vert& a, const Vert& b, const Vert& c) {
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

PackedMesh::PackedMesh(const phoenix::mesh& mesh, PkgType type) {
  if(type==PK_VisualLnd || type==PK_Visual) {
    packMeshletsLnd(mesh);
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

  packMeshletsObj(mesh,type,nullptr);
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

  packMeshletsObj(mesh,PK_Visual,&vertices);
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
    sub.iboOffset      = indices.size();

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

void PackedMesh::packMeshletsLnd(const phoenix::mesh& mesh) {
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

  PrimitiveHeap heap;
  heap.reserve(ibo.size()/3);
  std::vector<bool> used(ibo.size()/3,false);

  for(size_t mId=0; mId<mesh.materials.size(); ++mId) {
    if(duplicates[mId]!=mId)
      continue;

    heap.clear();
    for(size_t id=0; id<ibo.size(); id+=3) {
      if(duplicates[size_t(mat[id/3u])]!=mId)
        continue;
      auto a = mkUInt64(ibo[id+0],feat[id+0]);
      auto b = mkUInt64(ibo[id+1],feat[id+1]);
      auto c = mkUInt64(ibo[id+2],feat[id+2]);

      heap.push_back(std::make_pair(a, id));
      heap.push_back(std::make_pair(b, id));
      heap.push_back(std::make_pair(c, id));
      }

    if(heap.size()==0)
      continue;

    std::vector<Meshlet>  meshlets = buildMeshlets(&mesh,nullptr,heap,used);
    for(size_t i=0; i<meshlets.size(); ++i)
      meshlets[i].updateBounds(mesh);

    SubMesh pack;
    pack.material  = mesh.materials[mId];
    pack.iboOffset = indices.size();
    for(auto& i:meshlets)
      i.flush(vertices,indices,meshletBounds,pack,mesh);
    pack.iboLength = indices.size() - pack.iboOffset;
    if(pack.iboLength>0)
      subMeshes.push_back(std::move(pack));

    //dbgUtilization(meshlets);
    }
  }

void PackedMesh::packMeshletsObj(const phoenix::proto_mesh& mesh, PkgType type,
                                 const std::vector<SkeletalData>* skeletal) {
  auto* vId = (type==PK_VisualMorph) ? &verticesId : nullptr;

  size_t maxTri = 0;
  for(auto& sm:mesh.sub_meshes)
    maxTri = std::max(maxTri, sm.triangles.size());
  PrimitiveHeap heap;
  heap.reserve(maxTri);
  std::vector<bool> used(maxTri);

  for(size_t mId=0; mId<mesh.sub_meshes.size(); ++mId) {
    auto& sm      = mesh.sub_meshes[mId];
    auto& pack    = subMeshes[mId];
    pack.material = sm.mat;

    heap.clear();
    for(size_t i=0; i<sm.triangles.size(); ++i) {
      const uint16_t* ibo = sm.triangles[i].wedges;
      for(int x=0; x<3; ++x) {
        auto& wedge = sm.wedges[ibo[x]];
        auto vert = mkUInt64(wedge.index,ibo[x]);
        heap.push_back(std::make_pair(vert,i*3));
        }
      }

    std::vector<Meshlet> meshlets = buildMeshlets(nullptr,&sm,heap,used);
    for(size_t i=0; i<meshlets.size(); ++i)
      meshlets[i].updateBounds(mesh);

    pack.iboOffset = indices.size();
    for(auto& i:meshlets)
      i.flush(vertices,verticesA,indices,vId,pack,mesh.positions,sm.wedges,skeletal);
    pack.iboLength = indices.size() - pack.iboOffset;

    //dbgUtilization(meshlets);
    }
  }

std::vector<PackedMesh::Meshlet> PackedMesh::buildMeshlets(const phoenix::mesh* mesh,
                                                           const phoenix::sub_mesh* proto_mesh,
                                                           PrimitiveHeap& heap, std::vector<bool>& used) {
  heap.sort();
  std::fill(used.begin(), used.end(), false);

  size_t  firstUnused = 0;
  Meshlet active;

  std::vector<Meshlet> meshlets;
  while(!heap.empty()) {
    size_t id = -1;

    for(size_t r=0; r<active.vertSz; ++r) {
      auto i = active.vert[r];
      auto e = heap.equal_range(mkUInt64(i.first,i.second));
      for(auto it = e.first; it!=e.second; ++it) {
        if(used[it->second/3])
          continue;
        id = it->second;
        break;
        }
      if(id!=size_t(-1))
        break;
      }

    if(id==size_t(-1)) {
      if(active.indSz!=0 && active.vertSz>=MaxVert) {
        meshlets.push_back(std::move(active));
        active.clear();
        }
      for(auto i=heap.begin()+firstUnused; i!=heap.end();) {
        if(used[i->second/3]) {
          firstUnused++;
          ++i;
          continue;
          }
        id = i->second;
        break;
        }
      }

    if(id==size_t(-1)) {
      if(active.indSz>0)
        meshlets.push_back(std::move(active));
      break;
      }

    if(addTriangle(active,mesh,proto_mesh,id)) {
      used[id/3] = true;
      } else {
      meshlets.push_back(std::move(active));
      active.clear();
      }
    }
  return meshlets;
  }

bool PackedMesh::addTriangle(Meshlet& dest, const phoenix::mesh* mesh, const phoenix::sub_mesh* sm, size_t id) {
  if(mesh!=nullptr) {
    auto& ibo  = mesh->polygons.vertex_indices;
    auto& feat = mesh->polygons.feature_indices;

    auto a = std::make_pair(ibo[id+0],feat[id+0]);
    auto b = std::make_pair(ibo[id+1],feat[id+1]);
    auto c = std::make_pair(ibo[id+2],feat[id+2]);
    return dest.insert(a,b,c);
    }

  const uint16_t* feat = sm->triangles[id/3].wedges;
  auto a = std::make_pair(sm->wedges[feat[0]].index, feat[0]);
  auto b = std::make_pair(sm->wedges[feat[1]].index, feat[1]);
  auto c = std::make_pair(sm->wedges[feat[2]].index, feat[2]);
  return dest.insert(a,b,c);
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

void PackedMesh::dbgUtilization(const std::vector<Meshlet>& meshlets) {
  size_t usedV = 0, allocatedV = 0;
  size_t usedP = 0, allocatedP = 0;
  for(auto i:meshlets) {
    usedV      += i.vertSz;
    allocatedV += MaxVert;

    usedP      += i.indSz;
    allocatedP += MaxInd;
    }

  float procentV = (float(usedV*100)/float(allocatedV));
  float procentP = (float(usedP*100)/float(allocatedP));
  procentV = int(procentV*100)/100.f;

  char buf[256] = {};
  std::snprintf(buf,sizeof(buf),"Meshlet usage: prim = %02.02f%%, vert = %02.02f%%, count = %d", procentP, procentV, int(meshlets.size()));
  Log::d(buf);
  if(procentV<25)
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
