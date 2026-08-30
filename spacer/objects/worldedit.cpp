#include "worldedit.h"

#include <Tempest/Log>
#include <future>
#include <cassert>
#include <zenkit/World.hh>

#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/worldview.h"
#include "physics/dynamicworld.h"
#include "physics/physicmesh.h"
#include "utils/workers.h"
#include "resources.h"

using namespace Tempest;

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

  load(rootVob, world.world_vobs);
  physics = wdynamicFut.get();
  wview   = wviewFut.get();

  for(auto& i:rootVob.child)
    initView(i);
  }

WorldEdit::~WorldEdit() {
  }

void WorldEdit::load(Vob& out, std::vector<std::shared_ptr<zenkit::VirtualObject> >& child) {
  out.child.reserve(child.size());
  for(size_t i=0; i<child.size(); ++i) {
    out.child.emplace_back(vobNextId); ++vobNextId;
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
  if(visName.empty())
    return;

  //TODO: hierarchical transform?
  auto pos = Tempest::Matrix4x4(vob.rotation.columns[0].x, vob.rotation.columns[1].x, vob.rotation.columns[2].x, vob.position.x,
                                vob.rotation.columns[0].y, vob.rotation.columns[1].y, vob.rotation.columns[2].y, vob.position.y,
                                vob.rotation.columns[0].z, vob.rotation.columns[1].z, vob.rotation.columns[2].z, vob.position.z,
                                0, 0, 0, 1);

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
         out.mesh.setObjMatrix(pos);

         out.phys = PhysicMesh(*view, *physics, false);
         out.phys.setObjMatrix(pos);
         out.phys.setPayloadPtr(out.orig.get());
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

WorldEdit::Vob* WorldEdit::rayQuery(const Tempest::Vec3 s, const Tempest::Vec3 e) {
  auto ret  = physics->ray(s, e);
  auto uptr = reinterpret_cast<zenkit::VirtualObject*>(ret.uptr);
  return validatePointer(uptr, rootVob);
  }

WorldEdit::Vob* WorldEdit::validatePointer(const zenkit::VirtualObject* ptr, Vob& v) {
  if(ptr==v.orig.get())
    return &v;

  for(auto& i:v.child) {
    if(auto n = validatePointer(ptr, i))
      return n;
    }
  return nullptr;
  }
