#include "abstracttrigger.h"

#include <Tempest/Log>

#include <zenkit/vobs/Trigger.hh>

#include "world/objects/npc.h"
#include "world/world.h"
#include "game/serialize.h"

using namespace Tempest;

AbstractTrigger::AbstractTrigger(Vob* parent, World &world, const zenkit::VirtualObject& data, Flags flags)
  : Vob(parent,world,data,flags & (~Flags::Static)), callback(this), vobName(data.vob_name) {
  bboxSize   = Vec3(data.bbox.max.x-data.bbox.min.x,data.bbox.max.y-data.bbox.min.y,data.bbox.max.z-data.bbox.min.z)*0.5f;
  bboxOrigin = Vec3(data.bbox.max.x+data.bbox.min.x,data.bbox.max.y+data.bbox.min.y,data.bbox.max.z+data.bbox.min.z)*0.5f;
  bboxOrigin = bboxOrigin - position();

  box        = world.physic()->bboxObj(&callback,data.bbox);
  if(bboxSize!=Vec3()) {
    boxNpc = CollisionZone(world,bboxOrigin+position(),bboxSize);
    boxNpc.setCallback([this](Npc& npc){
      this->onIntersect(npc);
      });
    }

  using zenkit::VirtualObjectType;

  if(data.type == VirtualObjectType::zCTrigger            || data.type == VirtualObjectType::zCTriggerList ||
     data.type == VirtualObjectType::oCTriggerScript      || data.type == VirtualObjectType::zCMover ||
     data.type == VirtualObjectType::oCTriggerChangeLevel || data.type == VirtualObjectType::oCCSTrigger) {
    auto& trigger = reinterpret_cast<const zenkit::VTrigger&>(data);
    fireDelay          = uint64_t(trigger.fire_delay_sec*1000.f);
    retriggerDelay     = uint64_t(trigger.retrigger_delay_sec*1000.f);
    maxActivationCount = (data.type==VirtualObjectType::zCMover && trigger.max_activation_count!=0) ? uint32_t(-1) : uint32_t(trigger.max_activation_count);
    target             = trigger.target;
    disabled           = !trigger.start_enabled;
    sendUntrigger      = trigger.send_untrigger;
    reactToOnTrigger   = trigger.react_to_on_trigger;
    reactToOnTouch     = trigger.react_to_on_touch;
    respondToNpc       = trigger.respond_to_npc;
    respondToPlayer    = trigger.respond_to_pc;
    respondToObject    = trigger.respond_to_object;
    }

  world.addTrigger(this);
  }

AbstractTrigger::~AbstractTrigger() {}

std::string_view AbstractTrigger::name() const {
  return vobName;
  }

bool AbstractTrigger::isEnabled() const {
  return !disabled;
  }

void AbstractTrigger::processDelayedEvents() {
  if(!hasDelayedEvents())
    return;
  if(world.tickCount()<delayedEvent.timeBarrier)
    return;
  auto evt = std::move(delayedEvent);
  delayedEvent = TriggerEvent();
  implProcessEvent(evt);
  }

void AbstractTrigger::processEvent(const TriggerEvent& evt) {
  if(hasDelayedEvents()) {
    // discard, if already have pending
    return;
    }
  if(0!=emitTimeLast && world.tickCount()<emitTimeLast+retriggerDelay) {
    // need to discard event
    return;
    }
  if(fireDelay>0) {
    TriggerEvent ex(evt.target, evt.emitter, world.tickCount() + fireDelay, evt.type);
    delayedEvent = std::move(ex);
    return;
    }
  implProcessEvent(evt);
  }

void AbstractTrigger::implProcessEvent(const TriggerEvent& evt) {
  emitTimeLast = world.tickCount();
  switch(evt.type) {
    case TriggerEvent::T_Activate:
    case TriggerEvent::T_Startup:
    case TriggerEvent::T_StartupFirstTime:
    case TriggerEvent::T_Trigger:
      if(disabled || !reactToOnTrigger)
        return;
      if(emitCount>=maxActivationCount)
        return;
      ++emitCount;
      onTrigger(evt);
      break;
    case TriggerEvent::T_Untrigger:
      if(disabled || !sendUntrigger)
        return;
      onUntrigger(evt);
      break;
    case TriggerEvent::T_Enable:
      disabled = false;
      break;
    case TriggerEvent::T_Disable:
      disabled = true;
      break;
    case TriggerEvent::T_ToggleEnable:
      disabled = !disabled;
      break;
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

void AbstractTrigger::onIntersect(Npc& n) {
  /* NOTE:
   *
   * In Adanaos temple trap-movers have:
   *  flags       = 0b00000011
   *  filterFlags = 0b00011001
   *
   * smaller doors in temple:
   *  flags       = 0b00000011
   *  filterFlags = 0b00110011
   */
  if((n.isPlayer() ? !respondToPlayer : !respondToNpc) || !reactToOnTouch)
    return;

  if(!isEnabled())
    return;

  if(boxNpc.intersections().size()==1) {
    // enableTicks();
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
  fout.write(emitTimeLast);

  delayedEvent.save(fout);
  fout.write(ticksEnabled);
  }

void AbstractTrigger::load(Serialize& fin) {
  Vob::load(fin);
  boxNpc.load(fin);
  fin.read(emitCount,disabled);
  fin.read(emitTimeLast);

  if(fin.version()>=47)
    delayedEvent.load(fin);
  if(fin.version()>=48) {
    fin.read(ticksEnabled);
    if(ticksEnabled)
      world.enableTicks(*this);
    }
  }

bool AbstractTrigger::hasDelayedEvents() const {
  return !delayedEvent.target.empty();
  }

void AbstractTrigger::enableTicks() {
  if(ticksEnabled)
    return;
  ticksEnabled = true;
  world.enableTicks(*this);
  }

void AbstractTrigger::disableTicks() {
  if(!ticksEnabled)
    return;
  ticksEnabled = false;
  world.disableTicks(*this);
  }

const std::vector<Npc*>& AbstractTrigger::intersections() const {
  return boxNpc.intersections();
  }

void AbstractTrigger::Cb::onCollide(DynamicWorld::BulletBody& b) {
  if(!tg->respondToObject || !tg->reactToOnTouch)
    return;
  if(b.isSpell())
    return;
  TriggerEvent ex(tg->vobName,tg->vobName,tg->world.tickCount(),TriggerEvent::T_Activate);
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
