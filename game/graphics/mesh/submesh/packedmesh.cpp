#include "packedmesh.h"

#include <Tempest/Application>
#include <Tempest/Log>
#include <cassert>
#include <fstream>
#include <algorithm>

#include "game/compatibility/phoenix.h"
#include "gothic.h"

using namespace Tempest;

static uint64_t mkUInt64(uint32_t a, uint32_t b) {
  return (uint64_t(a)<<32) | uint64_t(b);
  };

static bool isVisuallySame(const zenkit::Material& a, const zenkit::Material& b) {
  return
          // a.name                         == b.name && // mat name
    a.group                        == b.group &&
    a.color                        == b.color &&
    a.smooth_angle                 == b.smooth_angle &&
    a.texture                      == b.texture &&
    a.texture_scale                == b.texture_scale &&
    a.texture_anim_fps             == b.texture_anim_fps &&
    a.texture_anim_map_mode        == b.texture_anim_map_mode &&
    a.texture_anim_map_dir         == b.texture_anim_map_dir &&
    // a.disable_collision            == b.disable_collision &&
    // a.disable_lightmap             == b.disable_lightmap &&
    // a.dont_collapse                == b.dont_collapse &&
    a.detail_object                == b.detail_object &&
    a.detail_object_scale          == b.detail_object_scale &&
    a.force_occluder               == b.force_occluder &&
    a.environment_mapping          == b.environment_mapping &&
    a.environment_mapping_strength == b.environment_mapping_strength &&
    a.wave_mode                    == b.wave_mode &&
    a.wave_speed                   == b.wave_speed &&
    a.wave_max_amplitude           == b.wave_max_amplitude &&
    a.wave_grid_size               == b.wave_grid_size &&
    a.ignore_sun                   == b.ignore_sun &&
    // a.alpha_func                   == b.alpha_func &&
    a.default_mapping              == b.default_mapping;
  }

static float areaOf(const Vec3 sahMin, const Vec3 sahMax) {
  auto sz = sahMax - sahMin;
  return 2*(sz.x*sz.y + sz.x*sz.z + sz.y*sz.z);
  }

static uint32_t floatBitsToUint(float f) {
  return reinterpret_cast<uint32_t&>(f);
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
    auto r = l; ++r;
    while(r!=data.end()) {
      if(r->first>v)
        break;
      ++r;
      }
    return std::make_pair(l,r);
    }
  };

void PackedMesh::Meshlet::flush(std::vector<Vertex>& vertices,
                                std::vector<uint32_t>& indices,
                                std::vector<uint8_t>& indices8,
                                std::vector<Cluster>& instances,
                                const zenkit::Mesh& mesh) {
  if(indSz==0)
    return;

  if(!validate())
    return;

  instances.push_back(bounds);

  auto& vbo = mesh.vertices;  // xyz
  auto& uv  = mesh.features;  // uv, normal

  size_t vboSz = vertices.size();
  vertices.resize(vboSz + MaxVert);
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
    vx.color   = 0xFFFFFFFF;
    vertices[vboSz+i] = vx;
    }
  for(size_t i=vertSz; i<MaxVert; ++i) {
    Vertex vx = {};
    vertices[vboSz+i] = vx;
    }

  size_t iboSz  = indices.size();
  indices.resize(iboSz + MaxInd);
  for(size_t i=0; i<indSz; ++i) {
    indices[iboSz+i] = uint32_t(vboSz)+indexes[i];
    }
  for(size_t i=indSz; i<MaxInd; ++i) {
    // padd with degenerated triangles
    indices[iboSz+i] = uint32_t(vboSz+indSz/3);
    }

  if(Gothic::options().doMeshShading || true) {
    size_t iboSz8 = indices8.size();
    indices8.resize(iboSz8 + MaxPrim*4);
    for(size_t i=0; i<indSz; i+=3) {
      size_t at = iboSz8 + (i/3)*4;
      indices8[at+0] = indexes[i+0];
      indices8[at+1] = indexes[i+1];
      indices8[at+2] = indexes[i+2];
      indices8[at+3] = 0;
      }
    if(indSz+1<MaxInd) {
      size_t at = iboSz8 + MaxPrim*4 - 4;
      indices8[at+0] = indexes[0];
      indices8[at+1] = indexes[0];
      indices8[at+2] = indSz/3;
      indices8[at+3] = vertSz;
      }
    }
  }

void PackedMesh::Meshlet::flush(std::vector<Vertex>& vertices, std::vector<VertexA>& verticesA,
                                std::vector<uint32_t>& indices, std::vector<uint8_t>& indices8,
                                std::vector<uint32_t>* verticesId,
                                const std::vector<zenkit::Vec3>& vboList,
                                const std::vector<zenkit::MeshWedge>& wedgeList,
                                const std::vector<SkeletalData>* skeletal) {
  if(indSz==0)
    return;

  auto& vbo = vboList;    // xyz
  auto& uv  = wedgeList;  // uv, normal

  size_t vboSz  = 0;
  size_t iboSz  = indices.size();

  if(skeletal==nullptr) {
    vboSz = vertices.size();
    vertices.resize(vboSz+MaxVert);
    } else {
    vboSz = verticesA.size();
    verticesA.resize(vboSz+MaxVert);
    }

  indices.resize(iboSz + MaxInd);

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

  if(Gothic::options().doMeshShading || true) {
    size_t iboSz8 = indices8.size();
    indices8.resize(iboSz8 + MaxPrim*4);
    for(size_t i=0; i<indSz; i+=3) {
      size_t at = iboSz8 + (i/3)*4;
      indices8[at+0] = indexes[i+0];
      indices8[at+1] = indexes[i+1];
      indices8[at+2] = indexes[i+2];
      indices8[at+3] = 0;
      }
    if(indSz+1<MaxInd) {
      size_t at = iboSz8 + MaxPrim*4 - 4;
      indices8[at+0] = indexes[0];
      indices8[at+1] = indexes[0];
      indices8[at+2] = indSz/3;
      indices8[at+3] = vertSz;
      }
    }
  }

bool PackedMesh::Meshlet::validate() const {
  return true;
  /*
  // debug code
  for(int i=0; i<indSz; ++i) {
    for(int r=i+1; r<indSz; ++r) {
      if(indexes[i]==indexes[r])
        return true;
      }
    }
  return false;
  */
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

  if(vSz==vertSz) {
    for(size_t i=0; i<indSz; i+=3) {
      if(indexes[i+0]==ea && indexes[i+1]==eb && indexes[i+2]==ec) {
        // duplicant?
        return true;
        }
      }
    }

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

void PackedMesh::Meshlet::updateBounds(const zenkit::Mesh& mesh) {
  updateBounds(mesh.vertices);
  }

void PackedMesh::Meshlet::updateBounds(const zenkit::MultiResolutionMesh& mesh) {
  updateBounds(mesh.positions);
  }

void PackedMesh::Meshlet::updateBounds(const std::vector<zenkit::Vec3>& vbo) {
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


PackedMesh::PackedMesh(const zenkit::Mesh& mesh, PkgType type) {
  if(type==PK_VisualLnd && Gothic::inst().options().doSoftwareRT) {
    packBVH(mesh);
    }

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

PackedMesh::PackedMesh(const zenkit::MultiResolutionMesh& mesh, PkgType type) {
  subMeshes.resize(mesh.sub_meshes.size());
  /* NOTE: some mods do have corrupted content description,
   * using alpha_test==0, for some of vegetation
   */
  isUsingAlphaTest = true || mesh.alpha_test;
  {
  auto bbox = mesh.bbox;
  mBbox[0] = Vec3(bbox.min.x,bbox.min.y,bbox.min.z);
  mBbox[1] = Vec3(bbox.max.x,bbox.max.y,bbox.max.z);
  }

  packMeshletsObj(mesh,type,nullptr);
  }

PackedMesh::PackedMesh(const zenkit::SoftSkinMesh& skinned) {
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
    if(stream.size()<=i) {
      continue;
      }
    for(size_t j=0; j<stream[i].size(); j++) {
      auto& weight = stream[i][j];
      vert.boneIndices[j]    = weight.node_index;
      vert.localPositions[j] = {weight.position.x, weight.position.y, weight.position.z};
      vert.weights[j]        = weight.weight;
      }
    // MODS: renormalize weights. Found issue with not 1.0 summ in gold-remaster mod.
    const float sum = (vert.weights[0] + vert.weights[1] + vert.weights[2] + vert.weights[3]);
    if(sum>std::numeric_limits<float>::min()) {
      vert.weights[0] /= sum;
      vert.weights[1] /= sum;
      vert.weights[2] /= sum;
      vert.weights[3] /= sum;
      }
    }

  packMeshletsObj(mesh,PK_Visual,&vertices);
  }

void PackedMesh::packPhysics(const zenkit::Mesh& mesh, PkgType type) {
  auto& vbo = mesh.vertices;
  auto& ibo = mesh.polygons.vertex_indices;
  vertices.reserve(vbo.size());

  auto& mid = mesh.polygons.material_indices;
  auto& mat = mesh.materials;

  std::vector<Prim> prim;
  prim.reserve(mid.size());
  for(size_t i=0; i<mid.size(); ++i) {
    auto& m = mat[mid[i]];
    if(m.disable_collision)
      continue;
    Prim p;
    p.primId = uint32_t(i*3);
    if(m.name.find(':')==std::string::npos) {
      // unnamed materials - can merge them
      p.mat   = uint8_t(m.group);
      } else {
      // offset named materials
      p.mat   = uint32_t(zenkit::MaterialGroup::NONE) + mid[i];
      }
    prim.push_back(p);
    }

  std::sort(prim.begin(), prim.end(), [](const Prim& a, const Prim& b){
    return std::tie(a.mat) < std::tie(b.mat);
    });

  std::unordered_map<uint32_t,size_t> icache;
  icache.rehash(size_t(std::sqrt(ibo.size())));

  indices.reserve(ibo.size());
  for(size_t i=0; i<prim.size();) {
    const auto mId = prim[i].mat;

    SubMesh sub;
    sub.iboOffset = indices.size();

    if(mId < size_t(zenkit::MaterialGroup::NONE)) {
      sub.material.name  = "";
      sub.material.group = zenkit::MaterialGroup(mId);
      } else {
      auto& m = mat[mId-size_t(zenkit::MaterialGroup::NONE)];
      sub.material.name  = m.name;
      sub.material.group = m.group;
      }

    for(; i<prim.size() && prim[i].mat==mId; ++i) {
      auto pr = prim[i].primId;
      for(size_t r=0; r<3; ++r) {
        uint32_t index = ibo[pr+r];
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
      }

    sub.iboLength = indices.size()-sub.iboOffset;
    if(sub.iboLength>0)
      subMeshes.emplace_back(std::move(sub));
    }
  }

void PackedMesh::packBVH(const zenkit::Mesh& mesh) {
  auto& ibo  = mesh.polygons.vertex_indices;
  //auto& feat = mesh.polygons.feature_indices;
  auto& mid  = mesh.polygons.material_indices;

  auto pullVert = [&](uint32_t i) {
    auto v = mesh.vertices[i];
    return Tempest::Vec3(v.x, v.y, v.z);
    };
  auto areaOf = [](const Vec3 sz) {
    return 2*(sz.x*sz.y + sz.x*sz.z + sz.y*sz.z);
    };

  // https://www.highperformancegraphics.org/posters23/Fast_Triangle_Pairing_for_Ray_Tracing.pdf
  std::vector<HalfEdge> edgeFrag;
  for(size_t i=0; i<mid.size(); ++i) {
    for(size_t r=0; r<3; ++r) {
      auto primId = uint32_t(i*3);
      auto i0     = ibo[primId+(r  )  ];
      auto i1     = ibo[primId+(r+1)%3];
      auto bbox   = pullVert(i0) - pullVert(i1);
      bbox.x = std::abs(bbox.x);
      bbox.y = std::abs(bbox.y);
      bbox.z = std::abs(bbox.z);

      HalfEdge f;
      f.sah  = areaOf(bbox);
      f.iMin = std::min(i0, i1);
      f.iMax = std::max(i0, i1);
      f.prim = uint32_t(i);
      edgeFrag.push_back(f);
      }
    }
  std::sort(edgeFrag.begin(), edgeFrag.end(), [](const HalfEdge& l, const HalfEdge& r){
    return std::tie(l.sah, l.iMin, l.iMax, l.prim) > std::tie(r.sah, r.iMin, r.iMax, r.prim);
    });

  std::vector<uint32_t> ibo2  = mesh.polygons.vertex_indices;
  std::vector<bool>     pairings(mid.size());
  std::vector<Fragment> frag;

  auto storePrim = [&](uint32_t prim, uint32_t iMin, uint32_t iMax) -> uint32_t {
    auto i0 = ibo[prim+0];
    auto i1 = ibo[prim+1];
    auto i2 = ibo[prim+2];
    while(i1==iMin || i1==iMax) {
      auto t = i0;
      i0 = i1;
      i1 = i2;
      i2 = t;
      }
    ibo2[prim+0] = i0;
    ibo2[prim+1] = i1;
    ibo2[prim+2] = i2;
    return prim;
    };

  for(size_t i=0; i<edgeFrag.size(); ++i) {
    const auto& a = edgeFrag[i+0];
    const auto& b = (i+1<edgeFrag.size()) ? edgeFrag[i+1] : a;
    if(pairings[a.prim] || pairings[b.prim])
      continue;

    Fragment f;
    if(i+1<edgeFrag.size() && a.iMin==b.iMin && a.iMax==b.iMax) {
      pairings[a.prim] = true;
      pairings[b.prim] = true;
      f.primId  = storePrim(uint32_t(a.prim*3), a.iMin, a.iMax);
      f.primId1 = storePrim(uint32_t(b.prim*3), a.iMin, a.iMax);
      frag.push_back(f);
      ++i;
      continue;
      }
    }

  for(size_t i=0; i<pairings.size(); ++i) {
    if(pairings[i])
      continue;
    Fragment f;
    f.primId  = storePrim(uint32_t(i*3), 0xFFFFFFFF, 0xFFFFFFFF);
    f.primId1 = 0xFFFFFFFF;
    frag.push_back(f);
    }

  for(auto& f:frag) {
    Vec3 bbmin, bbmax;
    for(auto i:{f.primId, f.primId1}) {
      if(i==0xFFFFFFFF)
        continue;
      for(size_t r=0; r<3; ++r) {
        auto index = ibo[i+r];
        Vec3 vec = pullVert(index);
        if(r==0 && i==f.primId) {
          bbmin = vec;
          bbmax = vec;
          continue;
          }
        bbmin.x = std::min(bbmin.x, vec.x);
        bbmin.y = std::min(bbmin.y, vec.y);
        bbmin.z = std::min(bbmin.z, vec.z);

        bbmax.x = std::max(bbmax.x, vec.x);
        bbmax.y = std::max(bbmax.y, vec.y);
        bbmax.z = std::max(bbmax.z, vec.z);
        }
      }
    f.centroid = (bbmin+bbmax)/2.f;
    f.bbmin    = bbmin;
    f.bbmax    = bbmax;
    }

  std::vector<BVHNode> nodes;
  Vec3 bbox[2] = {};
  packBVH(mesh, ibo2, nodes, bbox[0], bbox[1], frag.data(), frag.size());
  //TODO: ensure, that first node is a box node
  bvhNodes = std::move(nodes);
  }

uint32_t PackedMesh::packBVH(const zenkit::Mesh& mesh, std::vector<uint32_t>& ibo, std::vector<BVHNode>& nodes,
                             Tempest::Vec3& bbmin, Tempest::Vec3& bbmax, Fragment* frag, size_t size) {
  auto pullVert = [&](uint32_t i) {
    auto v = mesh.vertices[i];
    return Tempest::Vec3(v.x, v.y, v.z);
    };

  if(size==0) {
    assert(0);
    return BVH_NullNode;
    }

  bbmin = frag[0].bbmin;
  bbmax = frag[0].bbmax;
  for(size_t i=1; i<size; ++i) {
    auto& f = frag[i];
    bbmin.x = std::min(bbmin.x, f.bbmin.x);
    bbmin.y = std::min(bbmin.y, f.bbmin.y);
    bbmin.z = std::min(bbmin.z, f.bbmin.z);

    bbmax.x = std::max(bbmax.x, f.bbmax.x);
    bbmax.y = std::max(bbmax.y, f.bbmax.y);
    bbmax.z = std::max(bbmax.z, f.bbmax.z);
    }

  if(size<=1) {
    const uint32_t primId0 = frag[0].primId;
    BVHNode node = {};
    node.padd0  = 0;
    node.padd1  = primId0;
    node.lmin   = pullVert(ibo[primId0+0]);
    node.lmax   = pullVert(ibo[primId0+1]);
    node.rmin   = pullVert(ibo[primId0+2]);
    node.lmax  -= node.lmin;
    node.rmin  -= node.lmin;

    const size_t nId = nodes.size();
    if(true && frag[0].primId1!=0xFFFFFFFF) {
      const uint32_t primId1 = frag[0].primId1;
      node.rmax  = pullVert(ibo[primId1+1]);
      node.rmax -= node.lmin;
      nodes.emplace_back(node);
      return uint32_t(nId | BVH_Tri2Node);
      }

    nodes.emplace_back(node);
    return uint32_t(nId | BVH_Tri1Node);
    }

  const Vec3 sz = bbmax - bbmin;
  if(sz.x>sz.y && sz.x>sz.z) {
    std::sort(frag, frag+size, [](const Fragment& l, const Fragment& r){ return l.centroid.x < r.centroid.x; });
    }
  else if(sz.y>sz.x && sz.y>sz.z) {
    std::sort(frag, frag+size, [](const Fragment& l, const Fragment& r){ return l.centroid.y < r.centroid.y; });
    }
  else {
    std::sort(frag, frag+size, [](const Fragment& l, const Fragment& r){ return l.centroid.z < r.centroid.z; });
    }

  const auto   splitV = findNodeSplit(bbmin, bbmax, frag, size);
  const size_t split  = splitV.first;

  const size_t nId = nodes.size();
  nodes.emplace_back(); //reserve memory

  BVHNode node = {};
  node.left  = packBVH(mesh, ibo, nodes, node.lmin, node.lmax, frag,       split);
  node.right = packBVH(mesh, ibo, nodes, node.rmin, node.rmax, frag+split, size-split);
  nodes[nId] = node;
  return uint32_t(nId | BVH_BoxNode);
  }

std::pair<uint32_t,float> PackedMesh::findNodeSplit(const Fragment* frag, size_t size, const bool useSah) {
  if(!useSah)
    return std::make_pair(size/2, 0); // median split

  size_t split    = size/2;
  float  bestCost = std::numeric_limits<float>::max();

  std::vector<float> sahB(size);
  Vec3 sahMin = frag[size-1].bbmin;
  Vec3 sahMax = frag[size-1].bbmax;
  for(size_t i=size; i>1; ) {
    --i;

    auto& f = frag[i];
    sahMin.x = std::min(sahMin.x, f.bbmin.x);
    sahMin.y = std::min(sahMin.y, f.bbmin.y);
    sahMin.z = std::min(sahMin.z, f.bbmin.z);

    sahMax.x = std::max(sahMax.x, f.bbmax.x);
    sahMax.y = std::max(sahMax.y, f.bbmax.y);
    sahMax.z = std::max(sahMax.z, f.bbmax.z);

    sahB[i-1] = areaOf(sahMin, sahMax);
    }

  sahMin = frag[0].bbmin;
  sahMax = frag[0].bbmax;
  for(size_t i=0; i+1<size; ++i) {
    auto& f = frag[i];
    sahMin.x = std::min(sahMin.x, f.bbmin.x);
    sahMin.y = std::min(sahMin.y, f.bbmin.y);
    sahMin.z = std::min(sahMin.z, f.bbmin.z);

    sahMax.x = std::max(sahMax.x, f.bbmax.x);
    sahMax.y = std::max(sahMax.y, f.bbmax.y);
    sahMax.z = std::max(sahMax.z, f.bbmax.z);

    const float sahA = areaOf(sahMin, sahMax);
    const float cost = sahA*float(i) + sahB[i]*float(size-i);
    if(cost<bestCost) {
      bestCost = cost;
      split    = i+1;
      }
    }

  return std::make_pair(uint32_t(split), bestCost);
  }

std::pair<uint32_t, bool> PackedMesh::findNodeSplit(Tempest::Vec3 bbmin, Tempest::Vec3 bbmax, const Fragment* frag, size_t size) {
  const bool useSah = true;
  if(!useSah)
    return std::make_pair(size/2, 0); // median split

  const auto     ret      = findNodeSplit(frag, size, useSah);
  const uint32_t split    = ret.first;
  const float    bestCost = ret.second;

  if(bestCost >= areaOf(bbmax, bbmin)*float(size)) {
    //Log::e("size: ", size);
    return std::make_pair(split, false);
    }
  return std::make_pair(split, true);
  }

uint32_t PackedMesh::packPrimNode(const zenkit::Mesh& mesh, std::vector<BVHNode>& nodes, const Fragment* frag, size_t size) {
  auto pullVert = [&](uint32_t i) {
    auto v = mesh.vertices[i];
    return Tempest::Vec3(v.x, v.y, v.z);
    };

  const auto& ibo = mesh.polygons.vertex_indices;

  if(size<=1) {
    const uint32_t prim = frag[0].primId;

    BVHNode node = {};
    node.lmin     = pullVert(ibo[prim+0]);
    node.lmax     = pullVert(ibo[prim+2]);
    node.rmin     = pullVert(ibo[prim+1]);

    node.lmax    -= node.lmin;
    node.rmin    -= node.lmin;

    const size_t nId = nodes.size();
    nodes.emplace_back(node);
    return uint32_t(nId | BVH_Tri1Node);
    }

  if(true && size<=2) {
    uint32_t nIdx    = 0;
    uint32_t libo[6] = {};
    uint32_t cnt [6] = {};

    auto insert = [&](uint32_t id) {
      for(uint32_t i=0; i<nIdx; ++i)
        if(libo[i]==id) {
          cnt [i]++;
          return;
          }
      libo[nIdx] = id;
      cnt [nIdx]++;
      ++nIdx;
      };

    for(size_t i=0; i<size; ++i) {
      const uint32_t prim = frag[i].primId;
      for(size_t r=0; r<3; ++r) {
        insert(ibo[prim+r]);
        }
      }

    if(nIdx==4) {
      while(cnt[2]!=1) {
        auto tmp = libo[2];
        libo[2] = libo[1];
        libo[1] = libo[0];
        libo[0] = tmp;

        tmp = cnt[2];
        cnt[2]= cnt[1];
        cnt[1]= cnt[0];
        cnt[0]= tmp;
        }

      BVHNode node = {};
      node.lmin     = pullVert(libo[0]); node.left  = 0;
      node.lmax     = pullVert(libo[2]); node.right = 0;
      node.rmin     = pullVert(libo[1]); node.padd0 = 0;
      node.rmax     = pullVert(libo[3]); node.padd1 = 0;

      node.lmax    -= node.lmin;
      node.rmin    -= node.lmin;
      node.rmax    -= node.lmin;

      const size_t nId = nodes.size();
      nodes.emplace_back(node);
      return uint32_t(nId | BVH_Tri2Node);
      }
    }

  return BVH_NullNode;
  }

uint32_t PackedMesh::packBVH64(const zenkit::Mesh& mesh, std::vector<BVHNode64>& nodes, Fragment* frag, size_t size) {
  if(size==0) {
    assert(0);
    return BVH_NullNode;
    }

  if(size<=64) {
    BVHNode64 node = {};
    packPrim64(node, mesh, frag, size);
    const size_t nId = nodes.size();
    nodes.push_back(node);
    return uint32_t(nId | BVH_BoxNode);
    }

  size_t nId = nodes.size();
  nodes.emplace_back();

  BVHNode64 node   = {};
  uint32_t  nodeSz = 0;
  packNode64(node, nodeSz, mesh, nodes, frag, size, 0);
  nodes[nId] = node;
  assert(nodeSz<=64);

  return uint32_t(nId | BVH_BoxNode);
  }

void PackedMesh::packNode64(BVHNode64& out, uint32_t& outSz, const zenkit::Mesh& mesh, std::vector<BVHNode64>& nodes, Fragment* frag, size_t size, uint8_t depth) {
  Tempest::Vec3 bbmin = {}, bbmax = {};
  bbmin = frag[0].bbmin;
  bbmax = frag[0].bbmax;
  for(size_t i=1; i<size; ++i) {
    auto& f = frag[i];
    bbmin.x = std::min(bbmin.x, f.bbmin.x);
    bbmin.y = std::min(bbmin.y, f.bbmin.y);
    bbmin.z = std::min(bbmin.z, f.bbmin.z);

    bbmax.x = std::max(bbmax.x, f.bbmax.x);
    bbmax.y = std::max(bbmax.y, f.bbmax.y);
    bbmax.z = std::max(bbmax.z, f.bbmax.z);
    }

  if(depth>=3) {
    out.leaf[outSz].bbmin = bbmin;
    out.leaf[outSz].ptr   = packBVH64(mesh, nodes, frag, size);
    out.leaf[outSz].bbmax = bbmax;
    out.leaf[outSz].padd0 = 0;
    outSz++;
    return;
    }

  const Vec3 sz = bbmax - bbmin;
  if(sz.x>sz.y && sz.x>sz.z) {
    std::sort(frag, frag+size, [](const Fragment& l, const Fragment& r){ return l.centroid.x < r.centroid.x; });
    }
  else if(sz.y>sz.x && sz.y>sz.z) {
    std::sort(frag, frag+size, [](const Fragment& l, const Fragment& r){ return l.centroid.y < r.centroid.y; });
    }
  else {
    std::sort(frag, frag+size, [](const Fragment& l, const Fragment& r){ return l.centroid.z < r.centroid.z; });
    }

  if(depth==0) {
    out.self.bbmin = bbmin;
    out.self.bbmax = bbmax;
    }

  const bool useSah = false;
  const auto splitB = findNodeSplit(frag,        size,        useSah).first;
  const auto splitA = findNodeSplit(frag,        splitB,      useSah).first;
  const auto splitC = findNodeSplit(frag+splitB, size-splitB, useSah).first + splitB;

  packNode64(out, outSz, mesh, nodes, frag,        splitA,        depth+1);
  packNode64(out, outSz, mesh, nodes, frag+splitA, splitB-splitA, depth+1);
  packNode64(out, outSz, mesh, nodes, frag+splitB, splitC-splitB, depth+1);
  packNode64(out, outSz, mesh, nodes, frag+splitC, size  -splitC, depth+1);
  }

void PackedMesh::packPrim64(BVHNode64& out, const zenkit::Mesh& mesh, Fragment* frag, size_t size) {
  assert(size<=64);
  for(size_t i=0; i<size; ++i) {
    auto& l = out.leaf[i];
    l.bbmin = frag[i].bbmin;
    l.bbmax = frag[i].bbmax;
    l.ptr   = uint32_t(frag[i].primId | BVH_Tri1Node);
    }

  Tempest::Vec3 bbmin = {}, bbmax = {};
  bbmin = frag[0].bbmin;
  bbmax = frag[0].bbmax;
  for(size_t i=1; i<size; ++i) {
    auto& f = frag[i];
    bbmin.x = std::min(bbmin.x, f.bbmin.x);
    bbmin.y = std::min(bbmin.y, f.bbmin.y);
    bbmin.z = std::min(bbmin.z, f.bbmin.z);

    bbmax.x = std::max(bbmax.x, f.bbmax.x);
    bbmax.y = std::max(bbmax.y, f.bbmax.y);
    bbmax.z = std::max(bbmax.z, f.bbmax.z);
    }
  out.self.bbmin = bbmin;
  out.self.bbmax = bbmax;
  }

PackedMesh::CWBVH8 PackedMesh::packCWBVH8(const zenkit::Mesh& mesh, std::vector<UVec4>& nodes,
                                          const std::vector<uint32_t>& ibo, Fragment* frag, size_t size) {
  auto pullVert = [&](uint32_t i) {
    auto v = mesh.vertices[i];
    return Tempest::Vec3(v.x, v.y, v.z);
    };

  auto toUvec4 = [](const Vec3 a){
    UVec4 v;
    v.x = floatBitsToUint(a.x);
    v.y = floatBitsToUint(a.y);
    v.z = floatBitsToUint(a.z);
    v.w = 0; //TODO: texcoords
    return v;
    };

  if(size==0) {
    assert(0);
    return CWBVH8();
    }

  CWBblock block[8] = {};
  {
  uint32_t blockSz = 0;
  packCW8Blocks(block, blockSz, mesh, nodes, frag, size, 0);
  assert(blockSz<=8);
  }

  constexpr uint32_t numVec = (sizeof(CWBVH8)/sizeof(UVec4));
  CWBVH8 node = nodeFromBlocks(block);

  const uint32_t numNodes = node.pNodes;
  const uint32_t numPrims = node.pPrimitives;

  node.pNodes      = node.pNodes==0      ? 0 : uint32_t(nodes.size());
  node.pPrimitives = node.pPrimitives==0 ? 0 : uint32_t(nodes.size()) + numNodes*numVec;
  nodes.resize(nodes.size() + numNodes*numVec + numPrims*3);

  uint32_t iNode = 0, iPrim = 0;
  for(size_t i=0; i<8; ++i) {
    auto& b = block[i];
    if(b.size==0)
      continue;

    if((node.imask & (1u << i))!=0) {
      auto  child  = packCWBVH8(mesh, nodes, ibo, b.frag, b.size);
      auto* cwnode = reinterpret_cast<CWBVH8*>(nodes.data() + node.pNodes);
      cwnode[iNode] = child;
      iNode++;
      continue;
      }

    if(b.size<=1) {
      const uint32_t prim = b.frag[0].primId;
      const Vec3     ta   = pullVert(ibo[prim+0]);
      const Vec3     tb   = pullVert(ibo[prim+1]);
      const Vec3     tc   = pullVert(ibo[prim+2]);

      auto* cwnode = (nodes.data() + node.pPrimitives + iPrim*3);
      cwnode[0] = toUvec4(ta);
      cwnode[1] = toUvec4(tb);
      cwnode[2] = toUvec4(tc);

      cwnode[0].w = prim;
      iPrim++;
      continue;
      }

    assert(0);
    }

  return node;
  }

void PackedMesh::packCW8Blocks(CWBblock* out, uint32_t& outSz, const zenkit::Mesh& mesh, std::vector<UVec4>& nodes,
                               Fragment* frag, size_t size, uint8_t depth) {
  Tempest::Vec3 bbmin = {}, bbmax = {};
  bbmin = frag[0].bbmin;
  bbmax = frag[0].bbmax;
  for(size_t i=1; i<size; ++i) {
    auto& f = frag[i];
    bbmin.x = std::min(bbmin.x, f.bbmin.x);
    bbmin.y = std::min(bbmin.y, f.bbmin.y);
    bbmin.z = std::min(bbmin.z, f.bbmin.z);

    bbmax.x = std::max(bbmax.x, f.bbmax.x);
    bbmax.y = std::max(bbmax.y, f.bbmax.y);
    bbmax.z = std::max(bbmax.z, f.bbmax.z);
    }

  if(depth>=3) {
    out[outSz].bbmin = bbmin;
    out[outSz].bbmax = bbmax;
    out[outSz].frag  = frag;
    out[outSz].size  = size;
    outSz++;
    return;
    }

  Vec3 sz = bbmax-bbmin;
  if(sz.x>sz.y && sz.x>sz.z) {
    std::sort(frag, frag+size, [](const Fragment& l, const Fragment& r){ return l.centroid.x < r.centroid.x; });
    }
  else if(sz.y>sz.x && sz.y>sz.z) {
    std::sort(frag, frag+size, [](const Fragment& l, const Fragment& r){ return l.centroid.y < r.centroid.y; });
    }
  else {
    std::sort(frag, frag+size, [](const Fragment& l, const Fragment& r){ return l.centroid.z < r.centroid.z; });
    }
  //
  const bool useSah = false;
  const auto split  = findNodeSplit(frag, size, useSah).first;

  packCW8Blocks(out, outSz, mesh, nodes, frag,       split,      depth+1);
  packCW8Blocks(out, outSz, mesh, nodes, frag+split, size-split, depth+1);

  if(depth==0) {
    orderBlocks(out, outSz, bbmin, bbmax);
    }
  }

void PackedMesh::orderBlocks(CWBblock* block, const uint32_t numBlocks, const Tempest::Vec3 bbmin, const Tempest::Vec3 bbmax) {
  // Greedy child node ordering
  const Vec3 nodeCen = (bbmin + bbmax) * 0.5f;

  int assignment[8] = {};
  float cost[8][8];
  bool  isSlotEmpty[8];

  for(uint32_t s=0; s<8; s++) {
    isSlotEmpty[s] = true;
    for(size_t i=0; i<8; ++i)
      cost[s][i] = std::numeric_limits<float>::max();
    }

  for(size_t i=0; i<8; i++)
    assignment[i] = -1;

  for(uint32_t s = 0; s < 8; s++) {
    Vec3 ds = Vec3(
      (((s >> 2) & 1) == 1) ? -1.0f : 1.0f,
      (((s >> 1) & 1) == 1) ? -1.0f : 1.0f,
      (((s >> 0) & 1) == 1) ? -1.0f : 1.0f);

    for(size_t i=0; i<numBlocks; ++i) {
      Vec3 cen = (block[i].bbmin + block[i].bbmax) * 0.5f;
      cost[s][i] = Vec3::dotProduct(cen - nodeCen, ds);
      }
    }

  while(true) {
    float minCost  = std::numeric_limits<float>::max();
    IVec2 minEntry = IVec2(-1);

    for(int s = 0; s < 8; s++) {
      for(int i = 0; i < 8; i++) {
        if(assignment[i] == -1 && isSlotEmpty[s] && cost[s][i] < minCost) {
          minCost  = cost[s][i];
          minEntry = IVec2(s, i);
          }
        }
      }

    if(minEntry.x != -1 || minEntry.y != -1) {
      assert(minEntry.x != -1 && minEntry.y != -1);
      isSlotEmpty[minEntry.x] = false;
      assignment [minEntry.y] = minEntry.x;
      } else {
      assert(minEntry.x == -1 && minEntry.y == -1);
      break;
      }
    }

  for(size_t i = 0; i < 8; i++) {
    if(assignment[i] == -1) {
      for(int s = 0; s < 8; s++) {
        if(isSlotEmpty[s]) {
          isSlotEmpty[s] = false;
          assignment[i] = s;
          break;
          }
        }
      }
    }

  CWBblock tmp[8];
  for(size_t i=0; i<8; ++i)
    tmp[i] = block[assignment[i]];
  std::memcpy(block, tmp, sizeof(CWBblock)*8);
  }

PackedMesh::CWBVH8 PackedMesh::nodeFromBlocks(CWBblock* block) {
  CWBVH8 node = {};

  Vec3 bbmin = block[0].bbmin;
  Vec3 bbmax = block[0].bbmax;
  for(size_t i=1; i<8; ++i) {
    auto& f = block[i];
    bbmin.x = std::min(bbmin.x, f.bbmin.x);
    bbmin.y = std::min(bbmin.y, f.bbmin.y);
    bbmin.z = std::min(bbmin.z, f.bbmin.z);

    bbmax.x = std::max(bbmax.x, f.bbmax.x);
    bbmax.y = std::max(bbmax.y, f.bbmax.y);
    bbmax.z = std::max(bbmax.z, f.bbmax.z);
    }

  node.p[0] = bbmin.x;
  node.p[1] = bbmin.y;
  node.p[2] = bbmin.z;

  // Calculate quantization parameters for each axis respectively
  constexpr int   Nq    = 8;
  constexpr float denom = 1.0f / float((1 << Nq) - 1);

  const float ex = exp2f(ceilf(log2f((bbmax.x - bbmin.x) * denom)));
  const float ey = exp2f(ceilf(log2f((bbmax.y - bbmin.y) * denom)));
  const float ez = exp2f(ceilf(log2f((bbmax.z - bbmin.z) * denom)));

  const float inv_ex = 1.f/ex;
  const float inv_ey = 1.f/ey;
  const float inv_ez = 1.f/ez;

  node.e[0] = uint8_t(reinterpret_cast<const uint32_t&>(ex) >> 23);
  node.e[1] = uint8_t(reinterpret_cast<const uint32_t&>(ey) >> 23);
  node.e[2] = uint8_t(reinterpret_cast<const uint32_t&>(ez) >> 23);

  node.imask       = 0;
  node.pPrimitives = 0;
  node.pNodes      = 0;

  const size_t maxPrimPerNode = 1;
  for(size_t i=0; i<8; ++i) {
    auto& b = block[i];
    if(b.size==0)
      continue;

    if(b.size<=maxPrimPerNode) {
      static const uint32_t uNum[] = {0b000, 0b001, 0b011, 0b111};
      const uint32_t triIndex = node.pPrimitives * 3; // 3 x uvec4, per trinagle
      node.pPrimitives++;
      assert(triIndex<=24);

      node.imask |= (0u << i); // nop
      node.meta[i] = uint8_t((uNum[b.size] << 5) | (triIndex & 0b11111));
      } else {
      node.pNodes++;

      node.imask |= (1u << i);
      node.meta[i] = (1u << 5) | ((24+i) & 0b11111);
      }

    node.qmin_x[i] = uint8_t(floorf((b.bbmin.x - node.p[0]) * inv_ex));
    node.qmin_y[i] = uint8_t(floorf((b.bbmin.y - node.p[1]) * inv_ey));
    node.qmin_z[i] = uint8_t(floorf((b.bbmin.z - node.p[2]) * inv_ez));

    node.qmax_x[i] = uint8_t(ceilf ((b.bbmax.x - node.p[0]) * inv_ex));
    node.qmax_y[i] = uint8_t(ceilf ((b.bbmax.y - node.p[1]) * inv_ey));
    node.qmax_z[i] = uint8_t(ceilf ((b.bbmax.z - node.p[2]) * inv_ez));
    }

  return node;
  }

void PackedMesh::packMeshletsLnd(const zenkit::Mesh& mesh) {
  auto& ibo  = mesh.polygons.vertex_indices;
  auto& feat = mesh.polygons.feature_indices;
  auto& mid  = mesh.polygons.material_indices;

  std::vector<uint32_t> mat(mesh.materials.size());
  for(size_t i=0; i<mesh.materials.size(); ++i)
    mat[i] = uint32_t(i);

  for(size_t i=0; i<mesh.materials.size(); ++i) {
    for(size_t r=i+1; r<mesh.materials.size(); ++r) {
      if(mat[i]==mat[r])
        continue;
      auto& a = mesh.materials[i];
      auto& b = mesh.materials[r];
      if(isVisuallySame(a,b))
        mat[r] = mat[i];
      }
    }

  std::vector<Prim> prim;
  prim.reserve(mid.size());
  for(size_t i=0; i<mid.size(); ++i) {
    Prim p;
    p.primId = uint32_t(i*3);
    p.mat    = mat[mid[i]];
    prim.push_back(p);
    }
  std::sort(prim.begin(), prim.end(), [](const Prim& a, const Prim& b){
    return std::tie(a.mat) < std::tie(b.mat);
    });

  PrimitiveHeap heap;
  heap.reserve(mid.size());
  std::vector<bool> used(mid.size(),false);

  vertices.reserve(mesh.vertices.size());
  indices .reserve(ibo.size());
  indices8.reserve(ibo.size());
  meshletBounds.reserve(prim.size()/MaxPrim);
  for(size_t i=0; i<prim.size();) {
    const auto mId = prim[i].mat;

    heap.clear();
    for(; i<prim.size() && prim[i].mat==mId; ++i) {
      const uint32_t id = prim[i].primId;

      auto a = mkUInt64(ibo[id+0],feat[id+0]);
      auto b = mkUInt64(ibo[id+1],feat[id+1]);
      auto c = mkUInt64(ibo[id+2],feat[id+2]);

      heap.push_back(std::make_pair(a, id));
      heap.push_back(std::make_pair(b, id));
      heap.push_back(std::make_pair(c, id));
      }

    if(heap.size()==0)
      continue;

    std::vector<Meshlet> meshlets = buildMeshlets(&mesh,nullptr,heap,used);
    for(size_t i=0; i<meshlets.size(); ++i)
      meshlets[i].updateBounds(mesh);

    SubMesh pack;
    pack.material  = mesh.materials[mId];
    pack.iboOffset = indices.size();
    for(auto& i:meshlets)
      i.flush(vertices,indices,indices8,meshletBounds,mesh);
    pack.iboLength = indices.size() - pack.iboOffset;
    if(pack.iboLength>0)
      subMeshes.push_back(std::move(pack));

    //dbgUtilization(meshlets);
    }
  }

void PackedMesh::packMeshletsObj(const zenkit::MultiResolutionMesh& mesh, PkgType type,
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
        heap.push_back(std::make_pair(vert,uint32_t(i*3)));
        }
      }

    std::vector<Meshlet> meshlets = buildMeshlets(nullptr,&sm,heap,used);
    for(size_t i=0; i<meshlets.size(); ++i)
      meshlets[i].updateBounds(mesh);

    pack.iboOffset = indices.size();
    for(auto& i:meshlets)
      i.flush(vertices,verticesA,indices,indices8,vId,mesh.positions,sm.wedges,skeletal);
    pack.iboLength = indices.size() - pack.iboOffset;

    //dbgUtilization(meshlets);
    }
  }

std::vector<PackedMesh::Meshlet> PackedMesh::buildMeshlets(const zenkit::Mesh* mesh,
                                                           const zenkit::SubMesh* proto_mesh,
                                                           PrimitiveHeap& heap, std::vector<bool>& used) {
  heap.sort();
  std::fill(used.begin(), used.end(), false);

  const bool tightPacking = true;

  size_t  firstUnused = 0;
  size_t  firstVert   = 0;
  Meshlet active;

  std::vector<Meshlet> meshlets;
  while(true) {
    size_t triId = size_t(-1);

    for(size_t r=firstVert; r<active.vertSz; ++r) {
      auto i = active.vert[r];
      auto e = heap.equal_range(mkUInt64(i.first,i.second));
      for(auto it = e.first; it!=e.second; ++it) {
        auto id = it->second/3;
        if(used[id])
          continue;
        if(addTriangle(active,mesh,proto_mesh,id)) {
          used[id] = true;
          continue;
          }
        triId = id;
        break;
        }
      firstVert = r+1;
      }

    if(triId==size_t(-1) && active.indSz!=0 && !tightPacking) {
      // active.validate();
      meshlets.push_back(std::move(active));
      active.clear();
      firstVert = 0;
      continue;
      }

    if(triId==size_t(-1)) {
      for(auto i=heap.begin()+ptrdiff_t(firstUnused); i!=heap.end();) {
        if(used[i->second/3]) {
          ++i;
          continue;
          }
        firstUnused = size_t(std::distance(heap.begin(),i+1));
        triId       = i->second/3;
        break;
        }
      }

    if(triId!=size_t(-1) && addTriangle(active,mesh,proto_mesh,triId)) {
      used[triId] = true;
      continue;
      }

    if(active.indSz!=0) {
      // active.validate();
      meshlets.push_back(std::move(active));
      active.clear();
      firstVert = 0;
      }

    if(triId==size_t(-1))
      break;
    }

  return meshlets;
  }

bool PackedMesh::addTriangle(Meshlet& dest, const zenkit::Mesh* mesh, const zenkit::SubMesh* sm, size_t id) {
  if(mesh!=nullptr) {
    size_t id3  = id*3;
    auto&  ibo  = mesh->polygons.vertex_indices;
    auto&  feat = mesh->polygons.feature_indices;

    auto a = std::make_pair(ibo[id3+0],feat[id3+0]);
    auto b = std::make_pair(ibo[id3+1],feat[id3+1]);
    auto c = std::make_pair(ibo[id3+2],feat[id3+2]);
    return dest.insert(a,b,c);
    }

  const uint16_t* feat = sm->triangles[id].wedges;
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
  procentV = float(procentV*100)/100.f;

  char buf[256] = {};
  std::snprintf(buf,sizeof(buf),"Meshlet usage: prim = %02.02f%%, vert = %02.02f%%, count = %d", procentP, procentV, int(meshlets.size()));
  Log::d(buf);
  if(procentV<25)
    Log::d("");
  if(usedV<usedP)
    Log::d("");
  }

void PackedMesh::dbgMeshlets(const zenkit::Mesh& mesh, const std::vector<Meshlet*>& meshlets) {
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
