#pragma once

#include "abstracttrigger.h"

class World;

class CodeMaster : public AbstractTrigger {
  public:
    CodeMaster(Vob* parent, World &world, const zenkit::VCodeMaster& data, Flags flags);

    void onTrigger(const TriggerEvent& evt) override;

  private:
    void save(Serialize& fout) const override;
    void load(Serialize &fin) override;

    void onFailure();
    void onSuccess();
    void zeroState();

    std::vector<bool>        keys;
    std::vector<std::string> slaves;
    uint32_t                 count               = 0;
    bool                     ordered             = false;
    bool                     firstFalseIsFailure = false;
    std::string              failureTarget;
    bool                     untriggeredCancels  = false;
  };

