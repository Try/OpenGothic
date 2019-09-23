#pragma once

#include <daedalus/DaedalusStdlib.h>
#include <Tempest/RenderState>

#include <Tempest/Texture2d>

class ParticleFx final {
  public:
    ParticleFx(const Daedalus::GEngineClasses::C_ParticleFX & src);

    const Tempest::Texture2d* view=nullptr;
    Tempest::Vec3             colorS, colorE;

    int32_t                   frameCount=1;
    int32_t                   emitPerSec=1;

  private:
    Tempest::RenderState rs;
    const Daedalus::GEngineClasses::C_ParticleFX* src=nullptr;

    static Tempest::Vec3 loadVec3(const std::string& src);
  };

