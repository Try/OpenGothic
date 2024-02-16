#include "earthquake.h"

#include <Tempest/Log>

using namespace Tempest;

Earthquake::Earthquake(Vob* parent, World& world, const zenkit::VEarthquake& data, Flags flags)
  : AbstractTrigger(parent,world,data,flags) {
  }

void Earthquake::onTrigger(const TriggerEvent& evt) {
  Log::d("TODO: earthquake, \"", name() , "\"");
  }
