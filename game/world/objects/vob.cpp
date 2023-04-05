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

Vob::Vob(Vob* parent, World& owner, const phoenix::vob& vob, Flags flags)
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
      case phoenix::vob_type::oCMOB:
      case phoenix::vob_type::oCMobBed:
      case phoenix::vob_type::oCMobDoor:
      case phoenix::vob_type::oCMobInter:
      case phoenix::vob_type::oCMobContainer:
      case phoenix::vob_type::oCMobSwitch:
      case phoenix::vob_type::oCMobLadder:
      case phoenix::vob_type::oCMobWheel:
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


std::unique_ptr<Vob> Vob::load(Vob* parent, World& world, const phoenix::vob& vob, Flags flags) {
  namespace vobs = phoenix::vobs;
  switch(vob.type) {
    case phoenix::vob_type::unknown:
      return nullptr;
    case phoenix::vob_type::zCVob:
    case phoenix::vob_type::zCVobStair:
      return std::unique_ptr<Vob>(new StaticObj(parent,world,vob,flags));
    case phoenix::vob_type::zCVobAnimate:
      // NOTE: engine animates all objects anyway
      return std::unique_ptr<Vob>(new StaticObj(parent,world,vob,flags));
    case phoenix::vob_type::zCVobLevelCompo:
      return std::unique_ptr<Vob>(new Vob(parent,world,vob,flags));
    case phoenix::vob_type::oCMobFire:
      return std::unique_ptr<Vob>(new FirePlace(parent,world,reinterpret_cast<const vobs::mob_fire&>(vob),flags));
    case phoenix::vob_type::oCMOB:
      // Irdotar bow-triggers
      // focusOverride=true
      return std::unique_ptr<Vob>(new Interactive(parent,world,reinterpret_cast<const vobs::mob&>(vob),flags));
    case phoenix::vob_type::oCMobInter:
    case phoenix::vob_type::oCMobBed:
    case phoenix::vob_type::oCMobDoor:
    case phoenix::vob_type::oCMobContainer:
    case phoenix::vob_type::oCMobSwitch:
    case phoenix::vob_type::oCMobLadder:
    case phoenix::vob_type::oCMobWheel:
      return std::unique_ptr<Vob>(new Interactive(parent,world,reinterpret_cast<const vobs::mob&>(vob),flags));

    case phoenix::vob_type::zCMover:
      return std::unique_ptr<Vob>(new MoveTrigger(parent,world,reinterpret_cast<const vobs::trigger_mover&>(vob),flags));
    case phoenix::vob_type::zCCodeMaster:
      return std::unique_ptr<Vob>(new CodeMaster(parent,world,reinterpret_cast<const vobs::code_master&>(vob),flags));
    case phoenix::vob_type::zCTriggerList:
      return std::unique_ptr<Vob>(new TriggerList(parent,world,reinterpret_cast<const vobs::trigger_list&>(vob),flags));
    case phoenix::vob_type::oCTriggerScript:
      return std::unique_ptr<Vob>(new TriggerScript(parent,world,reinterpret_cast<const vobs::trigger_script&>(vob),flags));
    case phoenix::vob_type::zCTriggerWorldStart:
      return std::unique_ptr<Vob>(new TriggerWorldStart(parent,world,reinterpret_cast<const vobs::trigger_world_start&>(vob),flags));
    case phoenix::vob_type::oCTriggerChangeLevel:
      return std::unique_ptr<Vob>(new ZoneTrigger(parent,world,reinterpret_cast<const vobs::trigger_change_level&>(vob),flags));
    case phoenix::vob_type::zCTrigger:
      return std::unique_ptr<Vob>(new Trigger(parent,world,vob,flags));
    case phoenix::vob_type::zCMessageFilter:
      return std::unique_ptr<Vob>(new MessageFilter(parent,world,reinterpret_cast<const vobs::message_filter&>(vob),flags));
    case phoenix::vob_type::zCPFXController:
      return std::unique_ptr<Vob>(new PfxController(parent,world,reinterpret_cast<const vobs::pfx_controller&>(vob),flags));
    case phoenix::vob_type::oCTouchDamage:
      return std::unique_ptr<Vob>(new TouchDamage(parent,world,reinterpret_cast<const vobs::touch_damage&>(vob),flags));
    case phoenix::vob_type::zCTriggerUntouch:
      return std::unique_ptr<Vob>(new Vob(parent,world,vob,flags));
    case phoenix::vob_type::zCMoverController:
      return std::unique_ptr<Vob>(new MoverControler(parent,world,reinterpret_cast<const vobs::mover_controller&>(vob),flags));
    case phoenix::vob_type::zCEarthquake:
      return std::unique_ptr<Vob>(new Earthquake(parent,world,reinterpret_cast<const vobs::earthquake&>(vob),flags));
    case phoenix::vob_type::zCVobStartpoint: {
      float dx = vob.rotation[2].x;
      float dy = vob.rotation[2].y;
      float dz = vob.rotation[2].z;
      world.addStartPoint(Vec3(vob.position.x,vob.position.y,vob.position.z),Vec3(dx,dy,dz),vob.vob_name);
      // FIXME
      return std::unique_ptr<Vob>(new Vob(parent,world,vob,flags));
      }
    case phoenix::vob_type::zCVobSpot: {
      float dx = vob.rotation[2].x;
      float dy = vob.rotation[2].y;
      float dz = vob.rotation[2].z;
      world.addFreePoint(Vec3(vob.position.x,vob.position.y,vob.position.z),Vec3(dx,dy,dz),vob.vob_name);
      // FIXME
      return std::unique_ptr<Vob>(new Vob(parent,world,vob,flags));
      }
    case phoenix::vob_type::oCItem: {
      if(flags)
        world.addItem(reinterpret_cast<const vobs::item&>(vob));
      // FIXME
      return std::unique_ptr<Vob>(new Vob(parent,world,vob,flags));
      }

    case phoenix::vob_type::zCVobSound:
    case phoenix::vob_type::zCVobSoundDaytime:
    case phoenix::vob_type::oCZoneMusic:
    case phoenix::vob_type::oCZoneMusicDefault: {
      world.addSound(vob);
      // FIXME
      return std::unique_ptr<Vob>(new Vob(parent,world,vob,flags));
      }
    case phoenix::vob_type::zCVobLight: {
      return std::unique_ptr<Vob>(new WorldLight(parent,world,reinterpret_cast<const vobs::light&>(vob),flags));
      }
    case phoenix::vob_type::zCCSCamera:
      return std::unique_ptr<Vob>(new CsCamera(parent,world,reinterpret_cast<const vobs::cs_camera&>(vob),flags));
    case phoenix::vob_type::zCCamTrj_KeyFrame:
      break;
    case phoenix::vob_type::zCZoneZFog:
    case phoenix::vob_type::zCZoneZFogDefault:
      break;
    case phoenix::vob_type::zCZoneVobFarPlane:
    case phoenix::vob_type::zCZoneVobFarPlaneDefault:
      // wont do: no distance culling in any plans
      break;
    case phoenix::vob_type::zCVobLensFlare:
    case phoenix::vob_type::zCVobScreenFX:
      break;
    case phoenix::vob_type::oCNpc:
    case phoenix::vob_type::oCCSTrigger:
    case phoenix::vob_type::ignored:
      break;
    }

  return std::unique_ptr<Vob>(new Vob(parent,world,vob,flags));
  }

void Vob::saveVobTree(Serialize& fin) const {
  for(auto& i:child)
    i->saveVobTree(fin);
  if(vobType==phoenix::vob_type::zCVob)
    return;
  if(vobObjectID!=uint32_t(-1))
    save(fin);
  }

void Vob::loadVobTree(Serialize& fin) {
  if(fin.version()<43) {
    if(vobType==phoenix::vob_type::zCEarthquake ||
       vobType==phoenix::vob_type::zCCSCamera)
      return;
    }

  for(auto& i:child)
    i->loadVobTree(fin);
  if(vobObjectID!=uint32_t(-1) && vobType!=phoenix::vob_type::zCVob)
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
