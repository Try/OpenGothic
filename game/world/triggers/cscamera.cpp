#include "cscamera.h"

#include <Tempest/Log>

using namespace Tempest;

CsCamera::CsCamera(Vob* parent, World& world, const phoenix::vobs::cs_camera& cam, Flags flags)
  :AbstractTrigger(parent,world,cam,flags) {
  }

void CsCamera::onTrigger(const TriggerEvent& evt) {
  Log::d("TODO: cs-camera, \"", name() , "\"");
  }
