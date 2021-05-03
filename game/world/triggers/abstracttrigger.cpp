#include "abstracttrigger.h"

#include <Tempest/Log>

#include "world/objects/npc.h"
#include "world/world.h"
#include "game/serialize.h"

using namespace Tempest;

AbstractTrigger::AbstractTrigger(Vob* parent, World &world, ZenLoad::zCVobData &&data, bool startup)
  : Vob(parent,world,data,startup), data(std::move(data)), callback(this) {
  if(!hasFlag(StartEnabled))
    ;//disabled = true;
  bboxSize   = Vec3(data.bbox[1].x-data.bbox[0].x,data.bbox[1].y-data.bbox[0].y,data.bbox[1].z-data.bbox[0].z)*0.5f;
  bboxOrigin = Vec3(data.bbox[1].x+data.bbox[0].x,data.bbox[1].y+data.bbox[0].y,data.bbox[1].z+data.bbox[0].z)*0.5f;
  bboxOrigin = bboxOrigin - position();

  box        = world.physic()->bboxObj(&callback,data.bbox);
  world.addTrigger(this);
  }

AbstractTrigger::~AbstractTrigger() {
  if(box!=nullptr)
    world.physic()->deleteObj(box);
  }

ZenLoad::zCVobData::EVobType AbstractTrigger::vobType() const {
  return data.vobType;
  }

const std::string &AbstractTrigger::name() const {
  return data.vobName;
  }

bool AbstractTrigger::isEnabled() const {
  return !disabled;
  }

void AbstractTrigger::processOnStart(const TriggerEvent& evt) {
  if(vobType()==ZenLoad::zCVobData::VT_oCTriggerWorldStart) {
    processEvent(evt);
    return;
    }
  }

void AbstractTrigger::processEvent(const TriggerEvent& evt) {
  switch(evt.type) {
    case TriggerEvent::T_Startup:
    case TriggerEvent::T_StartupFirstTime:
    case TriggerEvent::T_Trigger:
      if(disabled) {
        return;
        }
      onTrigger(evt);
      break;
    case TriggerEvent::T_Untrigger:
      if(disabled) {
        return;
        }
      onUntrigger(evt);
      break;
    case TriggerEvent::T_Enable:
      disabled = false;
      break;
    case TriggerEvent::T_Disable:
      disabled = true;
      break;
    case TriggerEvent::T_ToogleEnable:
      disabled = !disabled;
      break;
    case TriggerEvent::T_Activate: {
      const bool canActivate = (data.zCTrigger.numCanBeActivated<=0 ||
                                emitCount<uint32_t(data.zCTrigger.numCanBeActivated));
      if(canActivate) {
        ++emitCount;
        onTrigger(evt);
        } else {
        //Log::d("skip trigger: ",evt.target," [emitCount]");
        }
      break;
      }
    case TriggerEvent::T_Move: {
      onGotoMsg(evt);
      };
    }
  }

void AbstractTrigger::onTrigger(const TriggerEvent&) {
  Log::d("TODO: trigger[",name(),"]");
  }

void AbstractTrigger::onUntrigger(const TriggerEvent&) {
  }

void AbstractTrigger::onGotoMsg(const TriggerEvent&) {
  }

void AbstractTrigger::moveEvent() {
  }

bool AbstractTrigger::hasFlag(ReactFlg flg) const {
  ReactFlg filter = ReactFlg(data.zCTrigger.flags & data.zCTrigger.filterFlags);
  return (filter&flg)==flg;
  }

void AbstractTrigger::onIntersect(Npc &n) {
  if(!hasFlag(n.isPlayer() ? RespondToPC : RespondToNPC) &&
     !hasFlag(ReactToOnTouch) &&
     data.vobType!=ZenLoad::zCVobData::VT_oCTouchDamage)
    return;

  if(!isEnabled())
    return;

  for(auto i:intersect)
    if(i==&n)
      return;
  intersect.push_back(&n);
  if(intersect.size()==1) {
    enableTicks();
    TriggerEvent e("","",TriggerEvent::T_Activate);
    processEvent(e);
    }
  }

void AbstractTrigger::tick(uint64_t) {
  for(size_t i=0;i<intersect.size();) {
    Npc& npc = *intersect[i];
    auto pos = npc.position();
    if(!checkPos(pos.x,pos.y+npc.translateY(),pos.z)) {
      intersect[i] = intersect.back();
      intersect.pop_back();
      } else {
      ++i;
      }
    }
  if(intersect.size()==0) {
    disableTicks();
    //TriggerEvent e("","",TriggerEvent::T_Untrigger);
    //processEvent(e);
    }
  }

bool AbstractTrigger::hasVolume() const {
  if( bboxSize.x>0 &&
      bboxSize.y>0 &&
      bboxSize.z>0 )
    return true;
  return false;
  }

bool AbstractTrigger::checkPos(float x,float y,float z) const {
  auto dp = Vec3(x,y,z) - (position() + bboxOrigin);
  if(std::fabs(dp.x)<bboxSize.x &&
     std::fabs(dp.y)<bboxSize.y &&
     std::fabs(dp.z)<bboxSize.z)
    return true;
  return false;
  }

void AbstractTrigger::save(Serialize& fout) const {
  Vob::save(fout);
  fout.write(uint32_t(intersect.size()));
  for(auto i:intersect)
    fout.write(i);
  fout.write(emitCount,disabled);
  }

void AbstractTrigger::load(Serialize& fin) {
  if(fin.version()<10)
    return;

  Vob::load(fin);
  uint32_t size=0;
  fin.read(size);
  intersect.resize(size);
  for(auto& i:intersect)
    fin.read(i);
  for(size_t i=0;i<intersect.size();)
    if(intersect[i]==nullptr) {
      intersect[i] = intersect.back();
      intersect.pop_back();
      } else {
      ++i;
      }
  fin.read(emitCount,disabled);

  if(intersect.size()>0)
    enableTicks();
  }

void AbstractTrigger::enableTicks() {
  world.enableTicks(*this);
  }

void AbstractTrigger::disableTicks() {
  world.disableTicks(*this);
  }

const std::vector<Npc*>& AbstractTrigger::intersections() const {
  return intersect;
  }

void AbstractTrigger::Cb::onCollide(DynamicWorld::BulletBody&) {
  if(!tg->hasFlag(ReactToOnTouch))
    return;
  TriggerEvent ex(tg->data.vobName,tg->data.vobName,tg->world.tickCount(),TriggerEvent::T_Activate);
  tg->processEvent(ex);
  }

void TriggerEvent::save(Serialize& fout) const {
  fout.write(target,emitter,uint8_t(type),timeBarrier);
  if(type==T_Move)
    fout.write(uint8_t(move.msg),move.key);
  }

void TriggerEvent::load(Serialize& fin) {
  fin.read(target,emitter,reinterpret_cast<uint8_t&>(type),timeBarrier);
  if(type==T_Move)
    fin.read(reinterpret_cast<uint8_t&>(move.msg),move.key);
  }
