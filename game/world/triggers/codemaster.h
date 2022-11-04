#pragma once

#include "abstracttrigger.h"

class World;

class CodeMaster : public AbstractTrigger {
  public:
    CodeMaster(Vob* parent, World &world, const phoenix::vobs::code_master& data, Flags flags);

    void onTrigger(const TriggerEvent& evt) override;

  private:
    void save(Serialize& fout) const override;
    void load(Serialize &fin) override;

    void onFailure();
    void zeroState();

    std::vector<bool>        keys;
    std::vector<std::string> slaves;
    bool                     ordered;
    bool                     firstFalseIsFailure;
    std::string              failureTarget;
  };

