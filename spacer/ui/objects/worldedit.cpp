#include "worldedit.h"

#include <Tempest/Log>
#include <future>
#include <cassert>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <zenkit/World.hh>

#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/mesh/submesh/staticmesh.h"
#include "graphics/worldview.h"
#include "physics/dynamicworld.h"
#include "utils/workers.h"
#include "resources.h"

using namespace Tempest;

namespace {

Tempest::Matrix4x4 vobTransform(const zenkit::VirtualObject& vob) {
  return Tempest::Matrix4x4(vob.rotation.columns[0].x, vob.rotation.columns[1].x, vob.rotation.columns[2].x, vob.position.x,
                            vob.rotation.columns[0].y, vob.rotation.columns[1].y, vob.rotation.columns[2].y, vob.position.y,
                            vob.rotation.columns[0].z, vob.rotation.columns[1].z, vob.rotation.columns[2].z, vob.position.z,
                            0, 0, 0, 1);
  }

bool rayAabb(const Tempest::Vec3& origin, const Tempest::Vec3& direction,
             const Tempest::Vec3& bmin, const Tempest::Vec3& bmax, float& distance) {
  float tMin = 0.f;
  float tMax = std::numeric_limits<float>::max();
  const float org[3] = {origin.x, origin.y, origin.z};
  const float dir[3] = {direction.x, direction.y, direction.z};
  const float mn [3] = {bmin.x, bmin.y, bmin.z};
  const float mx [3] = {bmax.x, bmax.y, bmax.z};

  for(size_t i=0; i<3; ++i) {
    if(std::abs(dir[i])<1e-7f) {
      if(org[i]<mn[i] || org[i]>mx[i])
        return false;
      continue;
      }

    float a = (mn[i]-org[i])/dir[i];
    float b = (mx[i]-org[i])/dir[i];
    if(a>b)
      std::swap(a,b);
    tMin = std::max(tMin,a);
    tMax = std::min(tMax,b);
    if(tMin>tMax)
      return false;
    }

  distance = tMin;
  return true;
  }

bool rayObb(const Tempest::Vec3& origin, const Tempest::Vec3& direction,
            const Tempest::Matrix4x4& transform, const Tempest::Vec3 bbox[2], float& distance) {
  auto inv = transform;
  inv.inverse();

  auto localOrigin = origin;
  inv.project(localOrigin);

  float x = direction.x;
  float y = direction.y;
  float z = direction.z;
  float w = 0.f;
  inv.project(x,y,z,w);
  return rayAabb(localOrigin, Tempest::Vec3(x,y,z), bbox[0], bbox[1], distance);
  }

bool validBounds(const Tempest::Vec3 bbox[2]) {
  return bbox[0].x<bbox[1].x || bbox[0].y<bbox[1].y || bbox[0].z<bbox[1].z;
  }

bool isLevelVob(std::string_view name) {
  std::string normalized;
  normalized.reserve(name.size());
  for(const unsigned char ch:name)
    if(std::isalnum(ch)!=0)
      normalized.push_back(char(std::toupper(ch)));
  return normalized=="LEVELVOB";
  }

bool rayTriangle(const Tempest::Vec3& origin, const Tempest::Vec3& direction,
                 const Tempest::Vec3& a, const Tempest::Vec3& b, const Tempest::Vec3& c, float& distance) {
  const auto edge1 = b-a;
  const auto edge2 = c-a;
  const auto p = Tempest::Vec3::crossProduct(direction,edge2);
  const float determinant = Tempest::Vec3::dotProduct(edge1,p);
  if(std::abs(determinant)<1e-7f)
    return false;

  const float invDeterminant = 1.f/determinant;
  const auto t = origin-a;
  const float u = Tempest::Vec3::dotProduct(t,p)*invDeterminant;
  if(u<0.f || u>1.f)
    return false;

  const auto q = Tempest::Vec3::crossProduct(t,edge1);
  const float v = Tempest::Vec3::dotProduct(direction,q)*invDeterminant;
  if(v<0.f || u+v>1.f)
    return false;

  distance = Tempest::Vec3::dotProduct(edge2,q)*invDeterminant;
  return distance>=0.f;
  }

}

struct WorldEdit::PickMesh {
  std::vector<Resources::Vertex> vertices;
  std::vector<uint32_t>          indices;
  };

WorldEdit::WorldEdit(std::string_view wname) {
  const auto* entry = Resources::vdfsIndex().find(wname);

  if(entry == nullptr) {
    Log::e("unable to open Zen-file: \"",wname,"\"");
    throw std::runtime_error("bad world file");
    }

  auto          buf = entry->open_read();
  zenkit::World world;
  world.load(buf.get(), zenkit::GameVersion::GOTHIC_2);

  auto& worldMesh = world.world_mesh;

  auto wdynamicFut = std::async(std::launch::async, [&]() {
    Workers::setThreadName("Loading: BVH thread");
    return std::unique_ptr<DynamicWorld>(new DynamicWorld(nullptr, worldMesh));
    });
  auto wviewFut = std::async(std::launch::async, [&]() {
    Workers::setThreadName("Loading: PackedMesh thread");
    PackedMesh vmesh(worldMesh,PackedMesh::PK_VisualLnd);
    return std::unique_ptr<WorldView>(new WorldView(vmesh, wname));
    });

  load(root, world.world_vobs);
  physics = wdynamicFut.get();
  wview   = wviewFut.get();

  for(auto& i:root.child)
    initView(i);
  }

WorldEdit::~WorldEdit() {
  }

void WorldEdit::load(Vob& out, std::vector<std::shared_ptr<zenkit::VirtualObject> >& child) {
  out.child.resize(child.size());
  for(size_t i=0; i<child.size(); ++i) {
    load(out.child[i], child[i]->children);
    out.child[i].orig = child[i];
    out.child[i].orig->children.clear();
    }
  }

void WorldEdit::initView(Vob& out) {
  for(auto& i:out.child)
    initView(i);

  assert(out.orig!=nullptr);
  const auto& vob     = *out.orig;
  const auto& visName = vob.visual_name;
  out.transform = vobTransform(vob);
  if(!isLevelVob(vob.vob_name))
    selectable.push_back(&out);

  if(visName.empty())
    return;

  //FIXME: copypaste from ObjVisual
  if(out.orig->type==zenkit::VirtualObjectType::zCVob) {
    switch (vob.visual->type) {
     case zenkit::VisualType::MESH:
     case zenkit::VisualType::MULTI_RESOLUTION_MESH: {
       auto view = Resources::loadMesh(visName);
       if(!view)
         return;
       // setType(M_Mesh);
       if(vob.show_visual) {
         out.mesh = wview->addStaticView(view, true);
         out.mesh.setWind(vob.anim_mode,vob.anim_strength);
         out.mesh.setObjMatrix(out.transform);
         }
       }
      case zenkit::VisualType::DECAL:
      case zenkit::VisualType::PARTICLE_EFFECT:
      case zenkit::VisualType::AI_CAMERA:
      case zenkit::VisualType::MODEL:
      case zenkit::VisualType::MORPH_MESH:
      case zenkit::VisualType::UNKNOWN:
        break;
      }
    }
  }

bool WorldEdit::select(const Tempest::Vec3& rayOrigin, const Tempest::Vec3& rayDirection, float maxDistance) {
  Vob*  nearest = nullptr;
  float nearestDistance = maxDistance;

  if(physics!=nullptr) {
    const auto landscape = physics->ray(rayOrigin,rayOrigin+rayDirection*maxDistance);
    if(landscape.hasCol)
      nearestDistance = (landscape.v-rayOrigin).length();
    }

  for(auto* vob:selectable) {
    float distance = 0.f;
    bool  hit = false;
    if(!vob->mesh.isEmpty()) {
      const auto bounds = vob->mesh.bounds();
      float broadDistance = 0.f;
      hit = validBounds(bounds.bbox) &&
            rayObb(rayOrigin, rayDirection, vob->transform, bounds.bbox, broadDistance) &&
            broadDistance<=nearestDistance &&
            rayMesh(*vob,rayOrigin,rayDirection,distance);
      }

    if(hit && distance<nearestDistance) {
      nearest = vob;
      nearestDistance = distance;
      }
    }

  const bool changed = selected!=nearest;
  selected = nearest;
  return changed;
  }

const WorldEdit::PickMesh& WorldEdit::pickMesh(const StaticMesh& mesh) {
  auto it = pickMeshes.find(&mesh);
  if(it!=pickMeshes.end())
    return *it->second;

  auto data = std::make_shared<PickMesh>();
  data->vertices.resize(mesh.vbo.size());
  data->indices.resize(mesh.ibo.size());
  if(!data->vertices.empty())
    Resources::device().readBytes(mesh.vbo,data->vertices.data(),mesh.vbo.byteSize());
  if(!data->indices.empty())
    Resources::device().readBytes(mesh.ibo,data->indices.data(),mesh.ibo.byteSize());
  auto result = pickMeshes.emplace(&mesh,std::move(data));
  return *result.first->second;
  }

bool WorldEdit::rayMesh(Vob& vob, const Tempest::Vec3& rayOrigin,
                        const Tempest::Vec3& rayDirection, float& distance) {
  bool hit = false;
  float nearest = std::numeric_limits<float>::max();
  for(size_t nodeId=0; nodeId<vob.mesh.nodesCount(); ++nodeId) {
    const auto node = vob.mesh.node(nodeId);
    const auto* mesh = node.mesh();
    if(mesh==nullptr)
      continue;

    auto inv = node.position();
    inv.inverse();
    auto localOrigin = rayOrigin;
    inv.project(localOrigin);
    float dx=rayDirection.x, dy=rayDirection.y, dz=rayDirection.z, dw=0.f;
    inv.project(dx,dy,dz,dw);
    const Tempest::Vec3 localDirection = {dx,dy,dz};

    const auto& geometry = pickMesh(*mesh);
    const auto slice = node.meshSlice();
    const size_t end = std::min<size_t>(slice.first+slice.second,geometry.indices.size());
    for(size_t index=slice.first; index+2<end; index+=3) {
      const uint32_t ia = geometry.indices[index+0];
      const uint32_t ib = geometry.indices[index+1];
      const uint32_t ic = geometry.indices[index+2];
      if(ia>=geometry.vertices.size() || ib>=geometry.vertices.size() || ic>=geometry.vertices.size())
        continue;
      const auto& va = geometry.vertices[ia];
      const auto& vb = geometry.vertices[ib];
      const auto& vc = geometry.vertices[ic];
      float triangleDistance = 0.f;
      if(rayTriangle(localOrigin,localDirection,
                     {va.pos[0],va.pos[1],va.pos[2]},
                     {vb.pos[0],vb.pos[1],vb.pos[2]},
                     {vc.pos[0],vc.pos[1],vc.pos[2]},triangleDistance) && triangleDistance<nearest) {
        nearest = triangleDistance;
        hit = true;
        }
      }
    }
  distance = nearest;
  return hit;
  }

bool WorldEdit::hasSelection() const {
  return selected!=nullptr;
  }

Tempest::Matrix4x4 WorldEdit::selectedTransform() const {
  if(selected==nullptr)
    return Tempest::Matrix4x4::mkIdentity();
  return selected->transform;
  }

void WorldEdit::setSelectedTransform(const Tempest::Matrix4x4& transform) {
  if(selected==nullptr)
    return;

  const auto oldPosition = selected->orig->position;
  selected->transform = transform;
  selected->mesh.setObjMatrix(transform);

  auto& vob = *selected->orig;
  vob.position = {transform.at(3,0), transform.at(3,1), transform.at(3,2)};
  for(size_t column=0; column<3; ++column) {
    const Tempest::Vec3 axis = {transform.at(int(column),0), transform.at(int(column),1), transform.at(int(column),2)};
    const float scale = axis.length();
    if(scale<=1e-7f)
      continue;
    vob.rotation.columns[column] = {axis.x/scale, axis.y/scale, axis.z/scale};
    }

  if(selected->mesh.isEmpty()) {
    const float dx = vob.position.x-oldPosition.x;
    const float dy = vob.position.y-oldPosition.y;
    const float dz = vob.position.z-oldPosition.z;
    vob.bbox.min.x += dx; vob.bbox.max.x += dx;
    vob.bbox.min.y += dy; vob.bbox.max.y += dy;
    vob.bbox.min.z += dz; vob.bbox.max.z += dz;
    }
  }

std::string_view WorldEdit::selectedName() const {
  if(selected==nullptr)
    return {};
  if(!selected->orig->vob_name.empty())
    return selected->orig->vob_name;
  return selected->orig->visual_name;
  }

void WorldEdit::selectedMeshes(std::vector<SelectedMesh>& out) const {
  out.clear();
  if(selected==nullptr)
    return;

  out.reserve(selected->mesh.nodesCount());
  for(size_t nodeId=0; nodeId<selected->mesh.nodesCount(); ++nodeId) {
    const auto node = selected->mesh.node(nodeId);
    const auto* mesh = node.mesh();
    if(mesh==nullptr)
      continue;
    const auto slice = node.meshSlice();
    SelectedMesh draw;
    draw.mesh        = mesh;
    draw.transform   = node.position();
    draw.indexOffset = slice.first;
    draw.indexCount  = slice.second;
    out.push_back(draw);
    }
  }
