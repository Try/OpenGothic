#include "vob.h"

#include <Tempest/Log>
#include <Tempest/Vec>

#include <glm/gtc/type_ptr.hpp>

#include "world/objects/fireplace.h"
#include "world/objects/interactive.h"
#include "world/objects/staticobj.h"
#include "world/triggers/earthquake.h"
#include "world/triggers/movetrigger.h"
#include "world/triggers/movercontroler.h"
#include "world/triggers/codemaster.h"
#include "world/triggers/triggerlist.h"
#include "world/triggers/triggerscript.h"
#include "world/triggers/triggerworldstart.h"
#include "world/triggers/zonetrigger.h"
#include "world/triggers/messagefilter.h"
#include "world/triggers/pfxcontroller.h"
#include "world/triggers/trigger.h"
#include "world/triggers/touchdamage.h"
#include "world/triggers/cscamera.h"
#include "world/worldlight.h"
#include "world/world.h"
#include "game/serialize.h"

using namespace Tempest;

Vob::Vob(World& owner)
  : world(owner) {
  }

Vob::Vob(Vob* parent, World& owner, const zenkit::VirtualObject& vob, Flags flags)
  : world(owner), vobType(vob.type), vobObjectID(vob.id), parent(parent) {

  glm::mat4x4 worldMatrix = vob.rotation;
  worldMatrix[3] = glm::vec4(vob.position, 1);

  pos   = Tempest::Matrix4x4(glm::value_ptr(worldMatrix));
  local = pos;

  if(parent!=nullptr) {
    auto m = parent->transform();
    m.inverse();
    m.mul(local);
    local = m;
    }

  for(auto& i:vob.children) {
    auto p = Vob::load(this,owner,*i,flags);
    if(p!=nullptr)
      child.emplace_back(std::move(p));
    }
  }

Vob::~Vob() {
  }

Vec3 Vob::position() const {
  return Vec3(pos.at(3,0),pos.at(3,1),pos.at(3,2));
  }

void Vob::setGlobalTransform(const Matrix4x4& p) {
  pos   = p;
  local = pos;

  if(parent!=nullptr) {
    auto m = parent->transform();
    m.inverse();
    m.mul(local);
    local = m;
    }
  }

void Vob::setLocalTransform(const Matrix4x4& p) {
  local = p;
  recalculateTransform();
  }

bool Vob::setMobState(std::string_view scheme, int32_t st) {
  bool ret = true;
  for(auto& i:child)
    ret &= i->setMobState(scheme,st);
  return ret;
  }

void Vob::moveEvent() {
  }

bool Vob::isDynamic() const {
  return false;
  }

float Vob::extendedSearchRadius() const {
  return 0;
  }

void Vob::recalculateTransform() {
  auto old = position();
  if(parent!=nullptr) {
    pos = parent->transform();
    pos.mul(local);
    } else {
    pos = local;
    }
  if(old!=position() && !isDynamic()) {
    switch(vobType) {
      case zenkit::VirtualObjectType::oCMOB:
      case zenkit::VirtualObjectType::oCMobBed:
      case zenkit::VirtualObjectType::oCMobDoor:
      case zenkit::VirtualObjectType::oCMobInter:
      case zenkit::VirtualObjectType::oCMobContainer:
      case zenkit::VirtualObjectType::oCMobSwitch:
      case zenkit::VirtualObjectType::oCMobLadder:
      case zenkit::VirtualObjectType::oCMobWheel:
        world.invalidateVobIndex();
        break;
      default:
        break;
      }
    }
  moveEvent();
  for(auto& i:child) {
    i->recalculateTransform();
    }
  }


std::unique_ptr<Vob> Vob::load(Vob* parent, World& world, const zenkit::VirtualObject& vob, Flags flags) {
  switch(vob.type) {
    case zenkit::VirtualObjectType::unknown:
      return nullptr;
    case zenkit::VirtualObjectType::zCVob:
    case zenkit::VirtualObjectType::zCVobStair:
      return std::unique_ptr<Vob>(new StaticObj(parent,world,vob,flags));
    case zenkit::VirtualObjectType::zCVobAnimate:
      // NOTE: engine animates all objects anyway
      return std::unique_ptr<Vob>(new StaticObj(parent,world,vob,flags));
    case zenkit::VirtualObjectType::zCVobLevelCompo:
      return std::unique_ptr<Vob>(new Vob(parent,world,vob,flags));
    case zenkit::VirtualObjectType::oCMobFire:
      return std::unique_ptr<Vob>(new FirePlace(parent,world,reinterpret_cast<const zenkit::VFire&>(vob),flags));
    case zenkit::VirtualObjectType::oCMOB:
      // Irdotar bow-triggers
      // focusOverride=true
      return std::unique_ptr<Vob>(new Interactive(parent,world,reinterpret_cast<const zenkit::VMovableObject&>(vob),flags));
    case zenkit::VirtualObjectType::oCMobInter:
    case zenkit::VirtualObjectType::oCMobBed:
    case zenkit::VirtualObjectType::oCMobDoor:
    case zenkit::VirtualObjectType::oCMobContainer:
    case zenkit::VirtualObjectType::oCMobSwitch:
    case zenkit::VirtualObjectType::oCMobLadder:
    case zenkit::VirtualObjectType::oCMobWheel:
      return std::unique_ptr<Vob>(new Interactive(parent,world,reinterpret_cast<const zenkit::VMovableObject&>(vob),flags));

    case zenkit::VirtualObjectType::zCMover:
      return std::unique_ptr<Vob>(new MoveTrigger(parent,world,reinterpret_cast<const zenkit::VMover&>(vob),flags));
    case zenkit::VirtualObjectType::zCCodeMaster:
      return std::unique_ptr<Vob>(new CodeMaster(parent,world,reinterpret_cast<const zenkit::VCodeMaster&>(vob),flags));
    case zenkit::VirtualObjectType::zCTriggerList:
      return std::unique_ptr<Vob>(new TriggerList(parent,world,reinterpret_cast<const zenkit::VTriggerList&>(vob),flags));
    case zenkit::VirtualObjectType::oCTriggerScript:
      return std::unique_ptr<Vob>(new TriggerScript(parent,world,reinterpret_cast<const zenkit::VTriggerScript&>(vob),flags));
    case zenkit::VirtualObjectType::zCTriggerWorldStart:
      return std::unique_ptr<Vob>(new TriggerWorldStart(parent,world,reinterpret_cast<const zenkit::VTriggerWorldStart&>(vob),flags));
    case zenkit::VirtualObjectType::oCTriggerChangeLevel:
      return std::unique_ptr<Vob>(new ZoneTrigger(parent,world,reinterpret_cast<const zenkit::VTriggerChangeLevel&>(vob),flags));
    case zenkit::VirtualObjectType::zCTrigger:
      return std::unique_ptr<Vob>(new Trigger(parent,world,vob,flags));
    case zenkit::VirtualObjectType::zCMessageFilter:
      return std::unique_ptr<Vob>(new MessageFilter(parent,world,reinterpret_cast<const zenkit::VMessageFilter&>(vob),flags));
    case zenkit::VirtualObjectType::zCPFXController:
      return std::unique_ptr<Vob>(new PfxController(parent,world,reinterpret_cast<const zenkit::VParticleEffectController&>(vob),flags));
    case zenkit::VirtualObjectType::oCTouchDamage:
      return std::unique_ptr<Vob>(new TouchDamage(parent,world,reinterpret_cast<const zenkit::VTouchDamage&>(vob),flags));
    case zenkit::VirtualObjectType::zCTriggerUntouch:
      return std::unique_ptr<Vob>(new Vob(parent,world,vob,flags));
    case zenkit::VirtualObjectType::zCMoverController:
      return std::unique_ptr<Vob>(new MoverControler(parent,world,reinterpret_cast<const zenkit::VMoverController&>(vob),flags));
    case zenkit::VirtualObjectType::zCEarthquake:
      return std::unique_ptr<Vob>(new Earthquake(parent,world,reinterpret_cast<const zenkit::VEarthquake&>(vob),flags));
    case zenkit::VirtualObjectType::zCVobStartpoint: {
      float dx = vob.rotation[2].x;
      float dy = vob.rotation[2].y;
      float dz = vob.rotation[2].z;
      world.addStartPoint(Vec3(vob.position.x,vob.position.y,vob.position.z),Vec3(dx,dy,dz),vob.vob_name);
      // FIXME
      return std::unique_ptr<Vob>(new Vob(parent,world,vob,flags));
      }
    case zenkit::VirtualObjectType::zCVobSpot: {
      float dx = vob.rotation[2].x;
      float dy = vob.rotation[2].y;
      float dz = vob.rotation[2].z;
      world.addFreePoint(Vec3(vob.position.x,vob.position.y,vob.position.z),Vec3(dx,dy,dz),vob.vob_name);
      // FIXME
      return std::unique_ptr<Vob>(new Vob(parent,world,vob,flags));
      }
    case zenkit::VirtualObjectType::oCItem: {
      if(flags)
        world.addItem(reinterpret_cast<const zenkit::VItem&>(vob));
      // FIXME
      return std::unique_ptr<Vob>(new Vob(parent,world,vob,flags));
      }

    case zenkit::VirtualObjectType::zCVobSound:
    case zenkit::VirtualObjectType::zCVobSoundDaytime:
    case zenkit::VirtualObjectType::oCZoneMusic:
    case zenkit::VirtualObjectType::oCZoneMusicDefault: {
      world.addSound(vob);
      // FIXME
      return std::unique_ptr<Vob>(new Vob(parent,world,vob,flags));
      }
    case zenkit::VirtualObjectType::zCVobLight: {
      return std::unique_ptr<Vob>(new WorldLight(parent,world,reinterpret_cast<const zenkit::VLight&>(vob),flags));
      }
    case zenkit::VirtualObjectType::zCCSCamera:
      return std::unique_ptr<Vob>(new CsCamera(parent,world,reinterpret_cast<const zenkit::VCutsceneCamera&>(vob),flags));
    case zenkit::VirtualObjectType::zCCamTrj_KeyFrame:
      break;
    case zenkit::VirtualObjectType::zCZoneZFog:
    case zenkit::VirtualObjectType::zCZoneZFogDefault:
      break;
    case zenkit::VirtualObjectType::zCZoneVobFarPlane:
    case zenkit::VirtualObjectType::zCZoneVobFarPlaneDefault:
      // wont do: no distance culling in any plans
      break;
    case zenkit::VirtualObjectType::zCVobLensFlare:
    case zenkit::VirtualObjectType::zCVobScreenFX:
      break;
    case zenkit::VirtualObjectType::oCNpc:
    case zenkit::VirtualObjectType::oCCSTrigger:
      break;
    }

  return std::unique_ptr<Vob>(new Vob(parent,world,vob,flags));
  }

void Vob::saveVobTree(Serialize& fin) const {
  for(auto& i:child)
    i->saveVobTree(fin);
  if(vobType==zenkit::VirtualObjectType::zCVob)
    return;
  if(vobObjectID!=uint32_t(-1))
    save(fin);
  }

void Vob::loadVobTree(Serialize& fin) {
  if(fin.version()<43) {
    if(vobType==zenkit::VirtualObjectType::zCEarthquake ||
       vobType==zenkit::VirtualObjectType::zCCSCamera)
      return;
    }

  for(auto& i:child)
    i->loadVobTree(fin);
  if(vobObjectID!=uint32_t(-1) && vobType!=zenkit::VirtualObjectType::zCVob)
    load(fin);
  }

void Vob::save(Serialize& fout) const {
  fout.setEntry("worlds/",fout.worldName(),"/mobsi/",vobObjectID,"/data");
  fout.write(uint8_t(vobType),pos,local);
  }

void Vob::load(Serialize& fin) {
  if(!fin.setEntry("worlds/",fin.worldName(),"/mobsi/",vobObjectID,"/data"))
    return;
  auto type = uint8_t(vobType);
  uint8_t savValue;
  fin.read(savValue,pos,local);

  if(fin.version() > 41) {
    if(savValue!=type)
      throw std::logic_error("inconsistent *.sav vs world");
    }
  moveEvent();
  }
