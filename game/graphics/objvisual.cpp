#include "objvisual.h"

#include "world/world.h"
#include "graphics/mesh/pose.h"
#include "utils/fileext.h"

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
      new (&mesh) MeshObjects::Mesh();
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
    mesh   = world.addItmView(hitem.visual,hitem.material);
    }
  }

void ObjVisual::setVisual(const ZenLoad::zCVobData& vob, World& world) {
  cleanup();

  if(FileExt::hasExt(vob.visual,"ZEN")) {
    setType(M_Bundle);
    bundle = VobBundle(world,vob.visual);
    }
  else if(FileExt::hasExt(vob.visual,"PFX") || FileExt::hasExt(vob.visual,"TGA")) {
    if(vob.visualCamAlign==0 && FileExt::hasExt(vob.visual,"TGA")) {
      setType(M_Mesh);
      mesh = world.addDecalView(vob);
      } else {
      setType(M_Pfx);
      pfx = PfxEmitter(world,vob);
      pfx.setActive(true);
      pfx.setLooped(true);
      }
    }
  else {
    auto view = Resources::loadMesh(vob.visual);
    if(!view)
      return;
    setType(M_Mdl);
    auto sk   = Resources::loadSkeleton(vob.visual.c_str());
    auto mesh = world.addStaticView(vob.visual.c_str());
    mdl.view.setVisual(sk);
    mdl.view.setVisualBody(std::move(mesh),world);
    mdl.view.setYTranslationEnable(false);

    if(vob.cdDyn || vob.cdStatic) {
      mdl.physic = PhysicMesh(*view,*world.physic(),false);
      }
    }
  }

void ObjVisual::setObjMatrix(const Tempest::Matrix4x4& obj) {
  switch(type) {
    case M_None:
      break;
    case M_Mdl:
      mdl.view.setObjMatrix(obj);
      mdl.physic.setObjMatrix(obj);
      break;
    case M_Mesh:
      mesh.setObjMatrix(obj);
      break;
    case M_Pfx:
      pfx.setObjMatrix(obj);
      break;
    case M_Bundle:
      bundle.setObjMatrix(obj);
      break;
    }
  }

const Animation::Sequence* ObjVisual::startAnimAndGet(const char* name, uint64_t tickCount) {
  if(type==M_Mdl) {
    return mdl.view.startAnimAndGet(name,tickCount);
    }
  return nullptr;
  }
