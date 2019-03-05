#include "trigger.h"

#include <Tempest/Log>

using namespace Tempest;

Trigger::Trigger(ZenLoad::zCVobData &&data)
  :data(std::move(data)) {
  }

const std::string &Trigger::name() const {
  return data.vobName;
  }

void Trigger::onTrigger() {
  Log::d("TODO: trigger[",name(),"]");
  }
