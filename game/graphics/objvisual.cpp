#include "objvisual.h"

#include <Tempest/Log>

#include "world/world.h"
#include "game/serialize.h"
#include "graphics/mesh/pose.h"
#include "graphics/mesh/skeleton.h"
#include "utils/fileext.h"
#include "gothic.h"

ObjVisual::ObjVisual() {
  }

ObjVisual::ObjVisual(ObjVisual&& other) {
  setType(other.type);
  other.type = M_None;

  switch(type) {
    case M_None:
      break;
    case M_Mdl:
      mdl = std::move(other.mdl);
      break;
    case M_Mesh:
      mesh = std::move(other.mesh);
    case M_Pfx:
      pfx = std::move(other.pfx);
      break;
    case M_Bundle:
      bundle = std::move(other.bundle);
      break;
    }
  }

ObjVisual& ObjVisual::operator =(ObjVisual&& other) {
  if(this==&other)
    return *this;
  setType(other.type);
  other.type = M_None;

  switch(type) {
    case M_None:
      break;
    case M_Mdl:
      mdl = std::move(other.mdl);
      break;
    case M_Mesh:
      mesh = std::move(other.mesh);
    case M_Pfx:
      pfx = std::move(other.pfx);
      break;
    case M_Bundle:
      bundle = std::move(other.bundle);
      break;
    }
  return *this;
  }

ObjVisual::~ObjVisual() {
  cleanup();
  }

void ObjVisual::save(Serialize& fout, const Interactive& mob) const {
  if(type==M_Mdl)
    return mdl.view.save(fout,mob);
  }

void ObjVisual::load(Serialize& fin, Interactive& mob) {
  if(type==M_Mdl)
    return mdl.view.load(fin,mob);
  }

void ObjVisual::cleanup() {
  switch(type) {
    case M_None:
      break;
    case M_Mdl:
      mdl.~Mdl();
      break;
    case M_Mesh:
      mesh.~Mesh();
      break;
    case M_Pfx:
      pfx.~PfxEmitter();
      break;
    case M_Bundle:
      bundle.~VobBundle();
      break;
    }
  type = M_None;
  }

void ObjVisual::setType(Type t) {
  if(t==type)
    return;
  cleanup();
  switch(t) {
    case M_None:
      break;
    case M_Mdl:
      new(&mdl) Mdl();
      break;
    case M_Mesh:
      new (&mesh) Mesh();
      break;
    case M_Pfx:
      new (&pfx) PfxEmitter();
      break;
    case M_Bundle:
      new(&bundle) VobBundle();
      break;
    }
  type = t;
  }

void ObjVisual::setVisual(const zenkit::IItem& hitem, World& world, bool staticDraw) {
  cleanup();

  if(FileExt::hasExt(hitem.visual,"ZEN")) {
    setType(M_Bundle);
    bundle = VobBundle(world,hitem.visual,(staticDraw ? Vob::Static : Vob::None));
    } else {
    setType(M_Mesh);
    mesh.view = world.addView(hitem);
    }
  }

void ObjVisual::setVisual(const zenkit::VirtualObject& vob, World& world, bool staticDraw) {
  cleanup();

  const bool       enableCollision = vob.cd_dynamic;    // collide with player
  std::string_view visName         = vob.visual->name;  // *.PFX; *.TGA; *.3DS; *.MDS; *.ASC; *.MMS

  if(visName.empty())
    return;

  // NOTE: .ZEN / M_Bundle is unused only by items - just assert it for now
  assert(!FileExt::hasExt(visName,"ZEN"));

  switch (vob.visual->type) {
    case zenkit::VisualType::MESH:
    case zenkit::VisualType::MULTI_RESOLUTION_MESH: {
      auto view = Resources::loadMesh(visName);
      if(!view)
        return;
      setType(M_Mesh);
      mesh.proto = view;
      if(vob.show_visual) {
        mesh.view = world.addStaticView(view,staticDraw);
        mesh.view.setWind(vob.anim_mode,vob.anim_strength);
        }

      const bool windy = (vob.anim_mode!=zenkit::AnimationType::NONE && vob.anim_strength>0);
      if(vob.show_visual && enableCollision && !windy) {
        mesh.physic = PhysicMesh(*view,*world.physic(),false);
        }
      break;
      }
    case zenkit::VisualType::MODEL:
    case zenkit::VisualType::MORPH_MESH:{
      // *.MDS; *.ASC; *.MMS
      auto visual = std::string(visName);
      FileExt::exchangeExt(visual,"ASC","MDL");

      auto view = Resources::loadMesh(visual);
      if(!view)
        return;
      setType(M_Mdl);
      mdl.proto = view;
      mdl.view.setYTranslationEnable(false);
      mdl.view.setVisual(view->skeleton.get());

      if(vob.show_visual) {
        if((view->skeleton==nullptr || view->skeleton->animation()==nullptr) && enableCollision)
          mdl.view.setVisualBody(world,world.addStaticView(view,true)); else
          mdl.view.setVisualBody(world,world.addView(view));
        }

      if(vob.show_visual && enableCollision) {
        mdl.physic = PhysicMesh(*view,*world.physic(),true);
        mdl.physic.setSkeleton(view->skeleton.get());
        }
      break;
      }
    case zenkit::VisualType::DECAL: {
      // *.TGA
      if(vob.sprite_camera_facing_mode!=zenkit::SpriteAlignment::NONE) {
        setType(M_Pfx);
        pfx = PfxEmitter(world,vob);
        pfx.setActive(true);
        pfx.setLooped(true);
        }
      else if(auto decal = dynamic_cast<const zenkit::VisualDecal*>(vob.visual.get())) {
        setType(M_Mesh);
        mesh.view = world.addDecalView(*decal);
        }
      break;
      }
    case zenkit::VisualType::PARTICLE_EFFECT: {
      // *.PFX
      setType(M_Pfx);
      pfx = PfxEmitter(world,vob);
      pfx.setActive(true);
      pfx.setLooped(true);
      break;
      }
    case zenkit::VisualType::AI_CAMERA:
    case zenkit::VisualType::UNKNOWN:
      break;
    }
  }

void ObjVisual::setObjMatrix(const Tempest::Matrix4x4& obj) {
  switch(type) {
    case M_None:
      break;
    case M_Mdl:
      mdl.view.setObjMatrix(obj,true);
      mdl.physic.setObjMatrix(obj);
      break;
    case M_Mesh:
      mesh.view.setObjMatrix(obj);
      mesh.physic.setObjMatrix(obj);
      break;
    case M_Pfx:
      pfx.setObjMatrix(obj);
      break;
    case M_Bundle:
      bundle.setObjMatrix(obj);
      break;
    }
  }

void ObjVisual::setInteractive(Interactive* it) {
  switch(type) {
    case M_None:
    case M_Pfx:
      break;
    case M_Mdl:
      mdl.physic.setInteractive(it);
      break;
    case M_Mesh:
      mesh.physic.setInteractive(it);
      break;
    case M_Bundle:
      //bundle.setObjMatrix(obj);
      break;
    }
  }

const Animation::Sequence* ObjVisual::startAnimAndGet(std::string_view name, uint64_t tickCount, bool force) {
  if(type==M_Mdl) {
    return mdl.view.startAnimAndGet(name,tickCount,force);
    }
  return nullptr;
  }

bool ObjVisual::isAnimExist(std::string_view name) const {
  if(type==M_Mdl)
    return mdl.view.isAnimExist(name);
  return false;
  }

bool ObjVisual::updateAnimation(Npc* npc, World& world, uint64_t dt) {
  if(type==M_Mdl) {
    bool ret = mdl.view.updateAnimation(npc,world,dt);
    if(ret)
      mdl.view.syncAttaches();
    return ret;
    }
  return false;
  }

void ObjVisual::processLayers(World& world) {
  if(type==M_Mdl) {
    mdl.view.processLayers(world);
    }
  }

void ObjVisual::syncPhysics() {
  if(type==M_Mdl)
    mdl.physic.setPose(mdl.view.pose());
  if(type==M_Mesh)
    ;//mesh.physic.setObjMatrix(mesh.view.);
  }

const ProtoMesh* ObjVisual::protoMesh() const {
  if(type==M_Mdl)
    return mdl.proto;
  if(type==M_Mesh)
    return mesh.proto;
  return nullptr;
  }

const Tempest::Matrix4x4& ObjVisual::bone(size_t i) const {
  if(type==M_Mdl)
    return mdl.view.pose().bone(i);
  static const Tempest::Matrix4x4 m;
  return m;
  }
