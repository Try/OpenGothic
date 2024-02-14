#pragma once

#include <phoenix/vobs/trigger.hh>

#include "abstracttrigger.h"

class World;

class TriggerList : public AbstractTrigger {
  public:
    TriggerList(Vob* parent, World &world, const zenkit::VTriggerList& data, Flags flags);

    void onTrigger(const TriggerEvent& evt) override;

  private:
    void save(Serialize &fout) const override;
    void load(Serialize &fin) override;

    enum ProcessMode : uint8_t {
      LP_ALL  = 0,
      LP_NEXT = 1,
      LP_RAND = 2
      };
    uint32_t next=0;
    std::vector<zenkit::VTriggerList::Target> targets;
    zenkit::TriggerBatchMode                  listProcess;
  };
