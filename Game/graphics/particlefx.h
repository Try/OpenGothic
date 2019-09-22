#pragma once

#include <daedalus/DaedalusStdlib.h>
#include <Tempest/RenderState>

class ParticleFx final {
  public:
    ParticleFx(const Daedalus::GEngineClasses::C_ParticleFX & src);

  private:
    Tempest::RenderState rs;
  };

