#include "vob.h"

#include <Tempest/Log>
#include <Tempest/Vec>

#include "interactive.h"
#include "staticobj.h"

#include "world/triggers/movetrigger.h"
#include "world/triggers/codemaster.h"
#include "world/triggers/triggerlist.h"
#include "world/triggers/triggerscript.h"
#include "world/triggers/triggerworldstart.h"
#include "world/triggers/zonetrigger.h"
#include "world/triggers/messagefilter.h"
#include "world/triggers/trigger.h"

#include "world.h"

using namespace Tempest;

Vob::Vob(World& owner, ZenLoad::zCVobData& vob, bool startup) {
  for(auto& i:vob.childVobs) {
    auto p = Vob::load(owner,std::move(i),startup);
    if(p!=nullptr)
      child.emplace_back(std::move(p));
    }
  vob.childVobs.clear();

  float v[16]={};
  std::memcpy(v,vob.worldMatrix.m,sizeof(v));
  pos = Tempest::Matrix4x4(v);
  }

Vob::~Vob() {
  }

Vec3 Vob::position() const {
  return Vec3(pos.at(3,0),pos.at(3,1),pos.at(3,2));
  }

void Vob::setTransform(const Matrix4x4& p) {
  pos = p;
  }


std::unique_ptr<Vob> Vob::load(World& owner, ZenLoad::zCVobData&& vob, bool startup) {
  if(vob.vobType==ZenLoad::zCVobData::VT_zCVobLevelCompo) {
    return std::unique_ptr<Vob>(new Vob(owner,vob,startup));
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_zCVob) {
    return std::unique_ptr<Vob>(new StaticObj(owner,std::move(vob),startup));
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_oCMobFire){
    return std::unique_ptr<Vob>(new StaticObj(owner,std::move(vob),startup));
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_oCMOB) {
    // Irdotar bow-triggers
    // focusOverride=true

    // Graves/Pointers
    // see focusNam
    return std::unique_ptr<Vob>(new Interactive(owner,std::move(vob),startup));
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_oCMobBed ||
          vob.vobType==ZenLoad::zCVobData::VT_oCMobDoor ||
          vob.vobType==ZenLoad::zCVobData::VT_oCMobInter ||
          vob.vobType==ZenLoad::zCVobData::VT_oCMobContainer ||
          vob.vobType==ZenLoad::zCVobData::VT_oCMobSwitch){
    return std::unique_ptr<Vob>(new Interactive(owner,std::move(vob),startup));
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_oCMobLadder) {
    //TODO: mob ladder
    return std::unique_ptr<Vob>(new StaticObj(owner,std::move(vob),startup));
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_zCMover){
    return std::unique_ptr<Vob>(new MoveTrigger(owner,std::move(vob),startup));
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_zCCodeMaster){
    return std::unique_ptr<Vob>(new CodeMaster(owner,std::move(vob),startup));
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_zCTriggerList){
    return std::unique_ptr<Vob>(new TriggerList(owner,std::move(vob),startup));
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_zCTriggerScript){
    return std::unique_ptr<Vob>(new TriggerScript(owner,std::move(vob),startup));
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_oCTriggerWorldStart){
    return std::unique_ptr<Vob>(new TriggerWorldStart(owner,std::move(vob),startup));
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_oCTriggerChangeLevel){
    return std::unique_ptr<Vob>(new ZoneTrigger(owner,std::move(vob),startup));
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_zCTrigger){
    return std::unique_ptr<Vob>(new Trigger(owner,std::move(vob),startup));
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_zCMessageFilter) {
    return std::unique_ptr<Vob>(new MessageFilter(owner,std::move(vob),startup));
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_zCVobStartpoint) {
    float dx = vob.rotationMatrix.v[2].x;
    float dy = vob.rotationMatrix.v[2].y;
    float dz = vob.rotationMatrix.v[2].z;
    owner.addStartPoint(Vec3(vob.position.x,vob.position.y,vob.position.z),Vec3(dx,dy,dz),vob.vobName.c_str());
    return nullptr;
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_zCVobSpot) {
    float dx = vob.rotationMatrix.v[2].x;
    float dy = vob.rotationMatrix.v[2].y;
    float dz = vob.rotationMatrix.v[2].z;
    owner.addFreePoint(Vec3(vob.position.x,vob.position.y,vob.position.z),Vec3(dx,dy,dz),vob.vobName.c_str());
    return nullptr;
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_oCItem) {
    if(startup)
      owner.addItem(vob);
    return nullptr;
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_zCVobSound ||
          vob.vobType==ZenLoad::zCVobData::VT_zCVobSoundDaytime ||
          vob.vobType==ZenLoad::zCVobData::VT_oCZoneMusic ||
          vob.vobType==ZenLoad::zCVobData::VT_oCZoneMusicDefault) {
    owner.addSound(vob);
    return nullptr;
    }
  else if(vob.vobType==ZenLoad::zCVobData::VT_zCVobLight) {
    owner.addLight(vob);
    return nullptr;
    }
  else if(vob.objectClass=="zCVobAnimate:zCVob" || // ork flags
          vob.objectClass=="zCPFXControler:zCVob"){
    //TODO: morph animation
    return std::unique_ptr<Vob>(new StaticObj(owner,std::move(vob),startup));
    }
  else if(vob.objectClass=="oCTouchDamage:zCTouchDamage:zCVob"){
    // NOT IMPLEMENTED
    }
  else if(vob.objectClass=="zCVobLensFlare:zCVob" ||
          vob.objectClass=="zCZoneVobFarPlane:zCVob" ||
          vob.objectClass=="zCZoneVobFarPlaneDefault:zCZoneVobFarPlane:zCVob" ||
          vob.objectClass=="zCZoneZFog:zCVob" ||
          vob.objectClass=="zCZoneZFogDefault:zCZoneZFog:zCVob") {
    // WONT-IMPLEMENT
    }
  else {
    static std::unordered_set<std::string> cls;
    if(cls.find(vob.objectClass)==cls.end()){
      cls.insert(vob.objectClass);
      Tempest::Log::d("unknown vob class ",vob.objectClass);
      }
    }
  return nullptr;
  }
