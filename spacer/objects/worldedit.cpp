#include "worldedit.h"

#include <Tempest/Log>
#include <future>
#include <cassert>
#include <zenkit/World.hh>
#include <zenkit/vobs/Misc.hh>

#include "graphics/mesh/skeleton.h"
#include "graphics/mesh/submesh/packedmesh.h"
#include "graphics/worldview.h"
#include "physics/dynamicworld.h"
#include "physics/physicmesh.h"
#include "project/projectmgr.h"
#include "utils/fileext.h"
#include "utils/workers.h"
#include "resources.h"

using namespace Tempest;

auto WorldEdit::Vob::release(size_t i) -> std::unique_ptr<WorldEdit::Vob> {
  auto v = std::move(child[i]);
  child.erase(child.begin()+i);
  return v;
  }

WorldEdit::Vob::Vob(std::shared_ptr<zenkit::VirtualObject> vob)  {
  orig = vob;
  }

void WorldEdit::Vob::insert(size_t i, std::unique_ptr<Vob> v) {
  child.insert(child.begin()+i, std::move(v));
  }

Vec3 WorldEdit::Vob::position() const {
  return Vec3(orig->position.x, orig->position.y, orig->position.z);
  }

zenkit::Mat3 WorldEdit::Vob::rotation() const {
  return orig->rotation;
  }

void WorldEdit::Vob::clearView() {
  for(auto& i:child)
    i->clearView();
  phys  = PhysicMesh();
  mesh  = MeshObjects::Mesh();
  mdl   = MdlVisual();
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

  if(!vob.show_visual) {
    mesh = MeshObjects::Mesh();
    mdl   = MdlVisual();
    }
  if(!vob.cd_dynamic) {
    phys  = PhysicMesh();
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
    //NOTE: light is shown anyway, in vanilla
    if(true || vob.show_visual) {
      light = owner.wview->addLight(reinterpret_cast<const zenkit::VLight&>(vob), 0);
      }
    return;
    }

  auto visualMesh = [](const zenkit::VirtualObject& vob) -> std::tuple<std::string_view,bool,zenkit::VisualType> {
    if(vob.type==zenkit::VirtualObjectType::zCVob && vob.visual!=nullptr) {
      return {vob.visual->name, vob.show_visual, vob.visual->type};
      }
    if((zenkit::VirtualObjectType::oCMOB<=vob.type && vob.type<=zenkit::VirtualObjectType::oCMobDoor) && vob.visual!=nullptr) {
      return {vob.visual->name, vob.show_visual, vob.visual->type};
      }
    if((zenkit::VirtualObjectType::zCTrigger<=vob.type && vob.type<=zenkit::VirtualObjectType::zCMover) && vob.visual!=nullptr) {
      return {vob.visual->name, vob.show_visual, vob.visual->type};
      }
    if(vob.type==zenkit::VirtualObjectType::oCItem) {
      auto& itm  = reinterpret_cast<const zenkit::VItem&>(vob);
      auto  pitm = ProjectMgr::inst().findCItem(itm.instance);
      auto  iitm = pitm.get<zenkit::IItem>();
      if(iitm!=nullptr)
        return {iitm->visual, true, zenkit::VisualType::MESH};
      return {"", true, zenkit::VisualType::UNKNOWN};
      }
    return {"", true, zenkit::VisualType::UNKNOWN};
    };

  const auto [visName, visShow, visType] = visualMesh(vob);
  if(visName.empty() || visType==zenkit::VisualType::UNKNOWN)
    return;

  //FIXME: copypaste from ObjVisual
  switch (visType) {
   case zenkit::VisualType::MESH:
   case zenkit::VisualType::MULTI_RESOLUTION_MESH: {
     auto view = Resources::loadMesh(visName);
     if(!view)
       return;
     // setType(M_Mesh);
     if(visShow) {
       mesh = owner.wview->addStaticView(view, true);
       mesh.setWind(vob.anim_mode,vob.anim_strength);
       mesh.setObjMatrix(pos);
       }
     if(vob.cd_dynamic) {
       phys = PhysicMesh(*view, *owner.physics, false);
       phys.setObjMatrix(pos);
       phys.setPayloadPtr(orig.get());
       }
     break;
     }
    case zenkit::VisualType::MODEL:
    case zenkit::VisualType::MORPH_MESH:{
      // *.MDS; *.ASC; *.MMS
      auto visualMdl = std::string(visName);
      FileExt::exchangeExt(visualMdl,"ASC","MDL");

      auto view = Resources::loadMesh(visualMdl);
      if(!view)
        return;
      // setType(M_Mdl);
      // mdl.proto = view;
      mdl.setYTranslationEnable(false);
      mdl.setVisual(view->skeleton.get());

      if(visShow) {
        if(view->skeleton==nullptr || view->skeleton->animation()==nullptr)
          mdl.setVisualBody(*owner.wview, owner.wview->addStaticView(view,true)); else
          mdl.setVisualBody(*owner.wview, owner.wview->addView(view));
        mdl.setObjMatrix(pos);
        }

      if(vob.cd_dynamic) {
        phys = PhysicMesh(*view, *owner.physics, false);
        phys.setSkeleton(view->skeleton.get());
        phys.setObjMatrix(pos);
        phys.setPayloadPtr(orig.get());
        }
      break;
      }
    case zenkit::VisualType::DECAL:
    case zenkit::VisualType::PARTICLE_EFFECT:
    case zenkit::VisualType::AI_CAMERA:
    case zenkit::VisualType::UNKNOWN:
      break;
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
  mdl .setObjMatrix(pos);
  mesh.setObjMatrix(pos);
  light.setPosition(v);
  }

void WorldEdit::Vob::setRotation(const zenkit::Mat3& rot) {
  auto& vob = *orig;
  vob.rotation = rot;

  auto pos = Tempest::Matrix4x4(vob.rotation.columns[0].x, vob.rotation.columns[1].x, vob.rotation.columns[2].x, vob.position.x,
                                vob.rotation.columns[0].y, vob.rotation.columns[1].y, vob.rotation.columns[2].y, vob.position.y,
                                vob.rotation.columns[0].z, vob.rotation.columns[1].z, vob.rotation.columns[2].z, vob.position.z,
                                0, 0, 0, 1);
  phys.setObjMatrix(pos);
  mdl .setObjMatrix(pos);
  mesh.setObjMatrix(pos);
  }

void WorldEdit::Vob::setVisual(WorldEdit& owner, std::string_view vis) {
  orig->visual_name  = vis; //legacy
  orig->show_visual  = true;
  orig->visual       = std::make_shared<zenkit::VisualMesh>();
  orig->visual->name = vis;
  orig->visual->type = zenkit::VisualType::MESH;
  clearView();
  initView(owner);
  }

void WorldEdit::Vob::setCollision(WorldEdit& owner, bool cd) {
  orig->cd_static  = cd;
  orig->cd_dynamic = cd;
  clearView();
  initView(owner);
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
    out.child.emplace_back(std::make_unique<Vob>(nullptr));
    load(*out.child[i], child[i]->children);
    out.child[i]->orig = child[i];
    out.child[i]->orig->children.clear();
    }
  }

bool WorldEdit::isValidVob(const Vob* v) const {
  return isValidVob(rootVob, v);
  }

bool WorldEdit::isValidVob(const Vob& node, const Vob* v) const {
  if(&node==v)
    return true;
  for(size_t i=0; i<node.child.size(); ++i) {
    if(isValidVob(*node.child[i], v))
      return true;
    }
  return false;
  }


CmdNewVob::CmdNewVob(WorldEdit::Vob* vob):vob(vob), stash(vob) {
  }

void CmdNewVob::redo(WorldEdit& subj) {
  stash->initView(subj);
  parent = &subj.root();
  parent->insert(parent->size(), std::move(stash));
  }

void CmdNewVob::undo(WorldEdit& subj) {
  stash = parent->release(parent->size()-1);
  stash->clearView();
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

CmdRotateVob::CmdRotateVob(WorldEdit::Vob* vob, zenkit::Mat3 ang) : vob(vob), ang(ang) {
  orig = vob->get()->rotation;
  }

void CmdRotateVob::redo(WorldEdit& subj) {
  vob->setRotation(ang);
  }

void CmdRotateVob::undo(WorldEdit& subj) {
  vob->setRotation(orig);
  }

bool CmdRotateVob::merge(const Action& prev) {
  if(auto p = dynamic_cast<const CmdRotateVob*>(&prev)) {
    if(p->vob==vob) {
      ang = p->ang;
      return true;
      }
    }
  return false;
  }