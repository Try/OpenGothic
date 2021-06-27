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
  if(bboxSize!=Vec3()) {
    boxNpc = CollisionZone(world,bboxOrigin+position(),bboxSize);
    boxNpc.setCallback([this](Npc& npc){
      this->onIntersect(npc);
      });
    }
  world.addTrigger(this);
  }

AbstractTrigger::~AbstractTrigger() {
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
  boxNpc.setPosition(position()+bboxOrigin);
  }

bool AbstractTrigger::hasFlag(ReactFlg flg) const {
  ReactFlg filter = ReactFlg(data.zCTrigger.flags & data.zCTrigger.filterFlags);
  return (filter&flg)==flg;
  }

void AbstractTrigger::onIntersect(Npc &n) {
  if(!hasFlag(n.isPlayer() ? RespondToPC : RespondToNPC) &&
     !hasFlag(ReactToOnTouch)/* &&
     data.vobType!=ZenLoad::zCVobData::VT_oCTouchDamage*/)
    return;

  if(!isEnabled())
    return;

  if(boxNpc.intersections().size()==1) {
    enableTicks();
    TriggerEvent e("","",TriggerEvent::T_Activate);
    processEvent(e);
    }
  }

void AbstractTrigger::tick(uint64_t) {
  }

bool AbstractTrigger::hasVolume() const {
  if( bboxSize.x>0 &&
      bboxSize.y>0 &&
      bboxSize.z>0 )
    return true;
  return false;
  }

bool AbstractTrigger::checkPos(const Tempest::Vec3& pos) const {
  return boxNpc.checkPos(pos);
  }

void AbstractTrigger::save(Serialize& fout) const {
  Vob::save(fout);
  boxNpc.save(fout);
  fout.write(emitCount,disabled);
  }

void AbstractTrigger::load(Serialize& fin) {
  if(fin.version()<10)
    return;

  Vob::load(fin);
  boxNpc.load(fin);
  fin.read(emitCount,disabled);
  }

void AbstractTrigger::enableTicks() {
  world.enableTicks(*this);
  }

void AbstractTrigger::disableTicks() {
  world.disableTicks(*this);
  }

const std::vector<Npc*>& AbstractTrigger::intersections() const {
  return boxNpc.intersections();
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
