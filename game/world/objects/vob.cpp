#include "vob.h"

#include <Tempest/Log>
#include <Tempest/Vec>

#include "world/objects/fireplace.h"
#include "world/objects/interactive.h"
#include "world/objects/staticobj.h"
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
#include "world/worldlight.h"
#include "world/world.h"
#include "game/serialize.h"

using namespace Tempest;

Vob::Vob(World& owner)
  : world(owner) {
  }

Vob::Vob(Vob* parent, World& owner, ZenLoad::zCVobData& vob, bool startup)
  : world(owner), vobType(uint8_t(vob.vobType)), vobObjectID(vob.vobObjectID), parent(parent) {
  float v[16]={};
  std::memcpy(v,vob.worldMatrix.m,sizeof(v));
  pos   = Tempest::Matrix4x4(v);
  local = pos;

  if(parent!=nullptr) {
    auto m = parent->transform();
    m.inverse();
    m.mul(local);
    local = m;
    }

  for(auto& i:vob.childVobs) {
    auto p = Vob::load(this,owner,std::move(i),startup);
    if(p!=nullptr) {
      childContent = ContentBit(p->childContent|childContent);
      child.emplace_back(std::move(p));
      }
    }
  vob.childVobs.clear();
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

void Vob::recalculateTransform() {
  auto old = position();
  if(parent!=nullptr) {
    pos = parent->transform();
    pos.mul(local);
    } else {
    pos = local;
    }
  if(old != position() && !isDynamic()) {
    if(childContent!=cbNone)
      world.invalidateVobIndex();
    }
  moveEvent();
  for(auto& i:child) {
    i->recalculateTransform();
    }
  }


std::unique_ptr<Vob> Vob::load(Vob* parent, World& world, ZenLoad::zCVobData&& vob, bool startup) {
  switch(vob.vobType) {
    case ZenLoad::zCVobData::VT_Unknown:
      return nullptr;
    case ZenLoad::zCVobData::VT_zCVob:
      return std::unique_ptr<Vob>(new StaticObj(parent,world,std::move(vob),startup));
    case ZenLoad::zCVobData::VT_zCVobLevelCompo:
      return std::unique_ptr<Vob>(new Vob(parent,world,vob,startup));
    case ZenLoad::zCVobData::VT_oCMobFire:
      if(parent!=nullptr)
        parent->childContent = ContentBit(parent->childContent|cbMobsi);
      return std::unique_ptr<Vob>(new FirePlace(parent,world,vob,startup));
    case ZenLoad::zCVobData::VT_oCMOB:
      // Irdotar bow-triggers
      // focusOverride=true
      if(parent!=nullptr)
        parent->childContent = ContentBit(parent->childContent|cbMobsi);
      return std::unique_ptr<Vob>(new Interactive(parent,world,vob,startup));
    case ZenLoad::zCVobData::VT_oCMobBed:
    case ZenLoad::zCVobData::VT_oCMobDoor:
    case ZenLoad::zCVobData::VT_oCMobInter:
    case ZenLoad::zCVobData::VT_oCMobContainer:
    case ZenLoad::zCVobData::VT_oCMobSwitch:
    case ZenLoad::zCVobData::VT_oCMobLadder:
    case ZenLoad::zCVobData::VT_oCMobWheel:
      if(parent!=nullptr)
        parent->childContent = ContentBit(parent->childContent|cbMobsi);
      return std::unique_ptr<Vob>(new Interactive(parent,world,vob,startup));

    case ZenLoad::zCVobData::VT_zCMover:
      if(parent!=nullptr)
        parent->childContent = ContentBit(parent->childContent|cbTrigger);
      return std::unique_ptr<Vob>(new MoveTrigger(parent,world,std::move(vob),startup));
    case ZenLoad::zCVobData::VT_zCCodeMaster:
      if(parent!=nullptr)
        parent->childContent = ContentBit(parent->childContent|cbTrigger);
      return std::unique_ptr<Vob>(new CodeMaster(parent,world,std::move(vob),startup));
    case ZenLoad::zCVobData::VT_zCTriggerList:
      if(parent!=nullptr)
        parent->childContent = ContentBit(parent->childContent|cbTrigger);
      return std::unique_ptr<Vob>(new TriggerList(parent,world,std::move(vob),startup));
    case ZenLoad::zCVobData::VT_zCTriggerScript:
      if(parent!=nullptr)
        parent->childContent = ContentBit(parent->childContent|cbTrigger);
      return std::unique_ptr<Vob>(new TriggerScript(parent,world,std::move(vob),startup));
    case ZenLoad::zCVobData::VT_oCTriggerWorldStart:
      if(parent!=nullptr)
        parent->childContent = ContentBit(parent->childContent|cbTrigger);
      return std::unique_ptr<Vob>(new TriggerWorldStart(parent,world,std::move(vob),startup));
    case ZenLoad::zCVobData::VT_oCTriggerChangeLevel:
      if(parent!=nullptr)
        parent->childContent = ContentBit(parent->childContent|cbTrigger);
      return std::unique_ptr<Vob>(new ZoneTrigger(parent,world,std::move(vob),startup));
    case ZenLoad::zCVobData::VT_zCTrigger:
      if(parent!=nullptr)
        parent->childContent = ContentBit(parent->childContent|cbTrigger);
      return std::unique_ptr<Vob>(new Trigger(parent,world,std::move(vob),startup));
    case ZenLoad::zCVobData::VT_zCMessageFilter:
      if(parent!=nullptr)
        parent->childContent = ContentBit(parent->childContent|cbTrigger);
      return std::unique_ptr<Vob>(new MessageFilter(parent,world,std::move(vob),startup));
    case ZenLoad::zCVobData::VT_zCPFXControler:
      if(parent!=nullptr)
        parent->childContent = ContentBit(parent->childContent|cbTrigger);
      return std::unique_ptr<Vob>(new PfxController(parent,world,std::move(vob),startup));
    case ZenLoad::zCVobData::VT_oCTouchDamage:
      if(parent!=nullptr)
        parent->childContent = ContentBit(parent->childContent|cbTrigger);
      return std::unique_ptr<Vob>(new TouchDamage(parent,world,std::move(vob),startup));
    case ZenLoad::zCVobData::VT_zCTriggerUntouch:
      if(parent!=nullptr)
        parent->childContent = ContentBit(parent->childContent|cbTrigger);
      return std::unique_ptr<Vob>(new Vob(parent,world,vob,startup));
    case ZenLoad::zCVobData::VT_zCMoverControler:
      if(parent!=nullptr)
        parent->childContent = ContentBit(parent->childContent|cbTrigger);
      return std::unique_ptr<Vob>(new MoverControler(parent,world,std::move(vob),startup));

    case ZenLoad::zCVobData::VT_zCVobStartpoint: {
      float dx = vob.rotationMatrix.v[2].x;
      float dy = vob.rotationMatrix.v[2].y;
      float dz = vob.rotationMatrix.v[2].z;
      world.addStartPoint(Vec3(vob.position.x,vob.position.y,vob.position.z),Vec3(dx,dy,dz),vob.vobName.c_str());
      // FIXME
      return std::unique_ptr<Vob>(new Vob(parent,world,vob,startup));
      }
    case ZenLoad::zCVobData::VT_zCVobSpot: {
      float dx = vob.rotationMatrix.v[2].x;
      float dy = vob.rotationMatrix.v[2].y;
      float dz = vob.rotationMatrix.v[2].z;
      world.addFreePoint(Vec3(vob.position.x,vob.position.y,vob.position.z),Vec3(dx,dy,dz),vob.vobName.c_str());
      // FIXME
      return std::unique_ptr<Vob>(new Vob(parent,world,vob,startup));
      }
    case ZenLoad::zCVobData::VT_oCItem: {
      if(startup)
        world.addItem(vob);
      // FIXME
      return std::unique_ptr<Vob>(new Vob(parent,world,vob,startup));
      }
    case ZenLoad::zCVobData::VT_zCVobSound:
    case ZenLoad::zCVobData::VT_zCVobSoundDaytime:
    case ZenLoad::zCVobData::VT_oCZoneMusic:
    case ZenLoad::zCVobData::VT_oCZoneMusicDefault: {
      world.addSound(vob);
      // FIXME
      return std::unique_ptr<Vob>(new Vob(parent,world,vob,startup));
      }
    case ZenLoad::zCVobData::VT_zCVobLight: {
      return std::unique_ptr<Vob>(new WorldLight(parent,world,std::move(vob),startup));
      }
    case ZenLoad::zCVobData::VT_zCVobLightPreset:
      return std::unique_ptr<Vob>(new Vob(parent,world,vob,startup));
    }

  return std::unique_ptr<Vob>(new Vob(parent,world,vob,startup));
  }

void Vob::saveVobTree(Serialize& fin) const {
  for(auto& i:child)
    i->saveVobTree(fin);
  if(vobObjectID!=uint32_t(-1))
    save(fin);
  }

void Vob::loadVobTree(Serialize& fin) {
  for(auto& i:child)
    i->loadVobTree(fin);
  if(vobObjectID!=uint32_t(-1))
    load(fin);
  }

void Vob::save(Serialize& fout) const {
  fout.setEntry("worlds/",world.name(),"/mobsi/",vobObjectID);
  fout.write(vobType,pos,local);
  }

void Vob::load(Serialize& fin) {
  fin.setEntry("worlds/",world.name(),"/mobsi/",vobObjectID);
  uint8_t type = vobType;
  fin.read(vobType,pos,local);
  if(vobType!=type)
    throw std::logic_error("inconsistent *.sav vs world");
  moveEvent();
  }
