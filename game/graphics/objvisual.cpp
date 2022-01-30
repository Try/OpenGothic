#include "objvisual.h"

#include <Tempest/Log>

#include "world/world.h"
#include "game/serialize.h"
#include "graphics/mesh/pose.h"
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

void ObjVisual::setVisual(const Daedalus::GEngineClasses::C_Item& hitem, World& world) {
  cleanup();

  if(FileExt::hasExt(hitem.visual.c_str(),"ZEN")) {
    setType(M_Bundle);
    bundle = VobBundle(world,hitem.visual.c_str());
    } else {
    setType(M_Mesh);
    mesh.view = world.addView(hitem);
    }
  }

void ObjVisual::setVisual(const ZenLoad::zCVobData& vob, World& world) {
  cleanup();

  // *.ZEN; *.PFX; *.TGA; *.3DS; *.MDS; *.ASC; *.MMS
  if(FileExt::hasExt(vob.visual,"ZEN")) {
    setType(M_Bundle);
    bundle = VobBundle(world,vob.visual);
    }
  else if(FileExt::hasExt(vob.visual,"PFX") || FileExt::hasExt(vob.visual,"TGA")) {
    if(vob.visualCamAlign==0 && FileExt::hasExt(vob.visual,"TGA")) {
      setType(M_Mesh);
      mesh.view = world.addDecalView(vob);
      } else {
      setType(M_Pfx);
      pfx = PfxEmitter(world,vob);
      pfx.setActive(true);
      pfx.setLooped(true);
      }
    }
  else if(FileExt::hasExt(vob.visual,"3DS")) {
    auto view = Resources::loadMesh(vob.visual);
    if(!view)
      return;
    setType(M_Mesh);
    mesh.proto = view;
    if(vob.showVisual) {
      mesh.view = world.addStaticView(view);
      if(Gothic::inst().version().game!=1)
        mesh.view.setWind(vob.visualAniMode);
      }
    if(vob.showVisual && (vob.cdDyn || vob.cdStatic) && vob.visualAniMode!=ZenLoad::AnimMode::WIND2) {
      mesh.physic = PhysicMesh(*view,*world.physic(),false);
      }
    }
  else if(FileExt::hasExt(vob.visual,"MDS") ||
          FileExt::hasExt(vob.visual,"MMS") ||
          FileExt::hasExt(vob.visual,"ASC"))  {
    auto visual = vob.visual;
    FileExt::exchangeExt(visual,"ASC","MDL");

    auto view = Resources::loadMesh(visual);
    if(!view)
      return;
    setType(M_Mdl);
    mdl.proto = view;
    mdl.view.setYTranslationEnable(false);
    mdl.view.setVisual(view->skeleton.get());
    if(vob.showVisual)
      mdl.view.setVisualBody(world,world.addView(view));

    if(vob.showVisual && (vob.cdDyn || vob.cdStatic)) {
      mdl.physic = PhysicMesh(*view,*world.physic(),true);
      mdl.physic.setSkeleton(view->skeleton.get());
      }
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
    mdl.physic.setPose(mdl.view.pose(),mdl.view.transform());
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
