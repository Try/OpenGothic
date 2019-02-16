#include "trigger.h"

Trigger::Trigger(const ZenLoad::zCVobData &data)
  :data(data) {
  }

const std::string &Trigger::name() const {
  return data.vobName;
  }
