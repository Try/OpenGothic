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
#include "assets.h"
#include "resources.h"

using namespace Tempest;

auto WorldEdit::Vob::release(size_t i) -> std::unique_ptr<WorldEdit::Vob> {
  auto v = std::move(child[i]);
  child.erase(child.begin()+i);
  return v;
  }

void WorldEdit::Vob::insert(size_t i, std::unique_ptr<Vob> v) {
  child.insert(child.begin()+i, std::move(v));
  }

void WorldEdit::Vob::clearView() {
  for(auto& i:child)
    i->clearView();
  phys  = PhysicMesh();
  mesh  = MeshObjects::Mesh();
  light = LightGroup::Light();
  }

void WorldEdit::Vob::initView(WorldEdit& owner) {
  for(auto& i:child)
    i->initView(owner);

  assert(orig!=nullptr);
  auto& vob = *orig;
  //TODO: hierarchical transform?
  auto pos = Tempest::Matrix4x4(vob.rotation.columns[0].x, vob.rotation.columns[1].x, vob.rotation.columns[2].x, vob.position.x,
                                vob.rotation.columns[0].y, vob.rotation.columns[1].y, vob.rotation.columns[2].y, vob.position.y,
                                vob.rotation.columns[0].z, vob.rotation.columns[1].z, vob.rotation.columns[2].z, vob.position.z,
                                0, 0, 0, 1);

  //FIXME: copypaste from ObjVisual
  if(vob.type==zenkit::VirtualObjectType::zCVob) {
    const auto& visName = vob.visual_name;
    if(visName.empty())
      return;
    switch (vob.visual->type) {
     case zenkit::VisualType::MESH:
     case zenkit::VisualType::MULTI_RESOLUTION_MESH: {
       auto view = Resources::loadMesh(visName);
       if(!view)
         return;
       // setType(M_Mesh);
       if(vob.show_visual) {
         mesh = owner.wview->addStaticView(view, true);
         mesh.setWind(vob.anim_mode,vob.anim_strength);
         mesh.setObjMatrix(pos);

         phys = PhysicMesh(*view, *owner.physics, false);
         phys.setObjMatrix(pos);
         phys.setPayloadPtr(orig.get());
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

  if(vob.type==zenkit::VirtualObjectType::zCVobLight) {
    /*
    static bool once = false;
    if(once) {
      out.orig = nullptr;
      return;
      }
    once = true;
    */
    light = owner.wview->addLight(reinterpret_cast<const zenkit::VLight&>(vob), 0);
    }
  }

void WorldEdit::Vob::setPosition(const Tempest::Vec3& v) {
  auto& vob = *orig;
  vob.position = zenkit::Vec3(v.x, v.y, v.z);

  auto pos = Tempest::Matrix4x4(vob.rotation.columns[0].x, vob.rotation.columns[1].x, vob.rotation.columns[2].x, vob.position.x,
                                vob.rotation.columns[0].y, vob.rotation.columns[1].y, vob.rotation.columns[2].y, vob.position.y,
                                vob.rotation.columns[0].z, vob.rotation.columns[1].z, vob.rotation.columns[2].z, vob.position.z,
                                0, 0, 0, 1);
  phys.setObjMatrix(pos);
  mesh.setObjMatrix(pos);
  light.setPosition(v);
  }


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
    i->initView(*this);
  }

WorldEdit::~WorldEdit() {
  }

void WorldEdit::load(Vob& out, std::vector<std::shared_ptr<zenkit::VirtualObject>>& child) {
  out.child.reserve(child.size());
  for(size_t i=0; i<child.size(); ++i) {
    out.child.emplace_back(std::make_unique<Vob>(vobNextId)); ++vobNextId;
    load(*out.child[i], child[i]->children);
    out.child[i]->orig = child[i];
    out.child[i]->orig->children.clear();
    }
  }

WorldEdit::Vob* WorldEdit::rayQuery(Tempest::Matrix4x4 v, Tempest::Matrix4x4 vp,
                                    Tempest::Point mpos, Tempest::Size wsize) {
  auto vInv = v;
  auto vpInv = vp;
  vInv.inverse();
  vpInv.inverse();

  Tempest::Vec2 pos = {mpos.x/float(wsize.w), mpos.y/float(wsize.h)};
  pos = 2.f*pos - 1.f;

  Vec3 dst = {pos.x, pos.y, 1};
  vpInv.project(dst);

  Vec3 src = {pos.x, pos.y, 0};
  vInv.project(src);

  auto ret  = physics->ray(src, dst);

  float           rayT   = ret.hitFraction;
  auto            uptr   = reinterpret_cast<zenkit::VirtualObject*>(ret.uptr);
  WorldEdit::Vob* retVob = validatePointer(uptr, rootVob);

  rayQueryLight(mpos, wsize, vp, src, dst, rayT, retVob, rootVob);

  return retVob;
  }

WorldEdit::Vob* WorldEdit::rayQuery(const Tempest::Vec3 s, const Tempest::Vec3 e) {
  auto ret  = physics->ray(s, e);
  auto uptr = reinterpret_cast<zenkit::VirtualObject*>(ret.uptr);
  return validatePointer(uptr, rootVob);
  }

void WorldEdit::rayQueryLight(Tempest::Point mpos, Tempest::Size wsize, const Tempest::Matrix4x4& vp,
                              const Tempest::Vec3& src, const Tempest::Vec3& dst,
                              float& rayT, WorldEdit::Vob*& ret, WorldEdit::Vob& v) {
  if(v.get()!=nullptr && v.get()->type==zenkit::VirtualObjectType::zCVobLight) {
    auto& vob  = *v.get();
    auto  pos  = Vec3(vob.position.x,vob.position.y,vob.position.z);
    auto  ndc  = pos;
    vp.project(ndc);

    ndc = (ndc*0.5 + 0.5);
    ndc *= Vec3(wsize.w, wsize.h, 1);

    const int spriteSize = Assets::inst().im.pointLight.w();
    if(ndc.z>0 && Vec2(ndc.x - mpos.x, ndc.y - mpos.y).quadLength() < spriteSize*spriteSize) {
      auto dir     = (dst - src);
      auto forward = Vec3(vp[0][2], vp[1][2], vp[2][2]);
      forward = Vec3::normalize(forward);

      float bT = Vec3::dotProduct(pos - src, forward) / Vec3::dotProduct(dir, forward);
      if(0<bT && bT < rayT) {
        rayT = bT;
        ret  = &v;
        }
      }
    }

  for(auto& i:v.child) {
    rayQueryLight(mpos, wsize, vp, src, dst, rayT, ret, *i);
    }
  }

WorldEdit::Vob* WorldEdit::validatePointer(const zenkit::VirtualObject* ptr, Vob& v) {
  if(ptr==v.orig.get())
    return &v;

  for(auto& i:v.child) {
    if(auto n = validatePointer(ptr, *i))
      return n;
    }
  return nullptr;
  }


CmdMoveVob::CmdMoveVob(WorldEdit::Vob* vob, Vec3 pos) : vob(vob), pos(pos) {
  auto p = vob->get()->position;
  orig = {p.x, p.y, p.z};
  }

void CmdMoveVob::redo(WorldEdit& subj) {
  vob->setPosition(pos);
  }

void CmdMoveVob::undo(WorldEdit& subj) {
  vob->setPosition(orig);
  }

bool CmdMoveVob::merge(const Action& prev) {
  if(auto p = dynamic_cast<const CmdMoveVob*>(&prev)) {
    if(p->vob==vob) {
      pos = p->pos;
      return true;
      }
    }
  return false;
  }

CmdDeleteVob::CmdDeleteVob(WorldEdit::Vob* vob):vob(vob) {
  }

void CmdDeleteVob::redo(WorldEdit& subj) {
  parent = findParent(subj.root(), vob);
  assert(parent!=nullptr);
  for(size_t i=0; i<parent->size(); ++i) {
    if(&(*parent)[i]==vob) {
      index = i;
      stash = parent->release(i);
      stash->clearView();
      return;
      }
    }
  }

void CmdDeleteVob::undo(WorldEdit& subj) {
  stash->initView(subj);
  parent->insert(index, std::move(stash));
  }

WorldEdit::Vob* CmdDeleteVob::findParent(WorldEdit::Vob& v, const WorldEdit::Vob* dst) {
  for(size_t i=0; i<v.size(); ++i) {
    if(&v[i]==dst)
      return &v;
    if(auto n = findParent(v[i], dst))
      return n;
    }
  return nullptr;
  }
