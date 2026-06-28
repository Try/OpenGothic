#include "triggerlist.h"

#include <Tempest/Log>

#include "world/world.h"
#include "game/serialize.h"

using namespace Tempest;

TriggerList::TriggerList(Vob* parent, World &world, const zenkit::VTriggerList& list, Flags flags)
  :AbstractTrigger(parent,world,list,flags) {
  targets = list.targets;
  listProcess = list.mode;
  }

void TriggerList::onTrigger(const TriggerEvent&) {
  emitList(TriggerEvent::T_Trigger);
  }

void TriggerList::onUntrigger(const TriggerEvent&) {
  // NOTE: in original-game zCTriggerList::UntriggerTarget @0x00615470 an untrigger runs the same
  // zCTriggerList::DoTriggering @0x00615190 worker with the +0x200 direction flag cleared, so
  // TriggerActTarget @0x00614f30 relays OnUntrigger to the selected target(s) using the same
  // ALL/NEXT/RANDOM selection, per-target fire-delay, and shared list index as the trigger path.
  // OpenGothic only overrode onTrigger, so untriggers hit the empty AbstractTrigger::onUntrigger
  // and were dropped -- list-driven targets got the "on" half but never the "off" half.
  emitList(TriggerEvent::T_Untrigger);
  }

void TriggerList::emitList(TriggerEvent::Type type) {
  if(targets.empty())
    return;

  switch(listProcess) {
    case zenkit::TriggerBatchMode::ALL: {
      uint64_t offset = 0;
      for(auto& i:targets) {
        offset += uint64_t(i.delay*1000);
        uint64_t time = world.tickCount()+offset;
        TriggerEvent ex(i.name,vobName,time,type);
        world.execTriggerEvent(ex);
        }
      break;
      }
    case zenkit::TriggerBatchMode::NEXT: {
      // NOTE: in original-game zCTriggerList::DoTriggering @0x00615190 the NEXT cursor (field_0x1fc)
      // advances only on the zero-delay fire path; when the selected target carries a positive
      // per-target fire-delay it is armed via zCVob::SetOnTimer and zCTriggerList::OnTimer @0x00615100
      // fires it WITHOUT advancing the cursor (the advance there is gated on ALL mode). So a NEXT list
      // with per-target delays re-fires the same target every trigger; OpenGothic advanced
      // unconditionally and walked the list. Zero-delay NEXT lists are unaffected.
      auto& i = targets[next];
      uint64_t time = world.tickCount()+uint64_t(i.delay*1000);
      if(i.delay<=0)
        next = (next+1)%uint32_t(targets.size());

      TriggerEvent ex(i.name,vobName,time,type);
      world.execTriggerEvent(ex);
      break;
      }
    case zenkit::TriggerBatchMode::RANDOM: {
      // NOTE: in original-game zCTriggerList::DoTriggering (Gothic2.exe 0x00615190) RANDOM mode
      // re-rolls while the new index equals the previously fired one, so the same target is
      // never fired twice in a row. OpenGothic rolled once with no exclusion.
      uint32_t idx = world.script().rand(uint32_t(targets.size()));
      if(targets.size()>1)
        while(idx==next)
          idx = world.script().rand(uint32_t(targets.size()));
      next = idx;
      auto& i = targets[idx];

      uint64_t time = world.tickCount()+uint64_t(i.delay*1000);
      TriggerEvent ex(i.name,vobName,time,type);
      world.execTriggerEvent(ex);
      break;
      }
    }
  }

void TriggerList::save(Serialize& fout) const {
  AbstractTrigger::save(fout);
  fout.write(next);
  }

void TriggerList::load(Serialize& fin) {
  AbstractTrigger::load(fin);
  fin.read(next);
  }
