#pragma once

#include <daedalus/DaedalusStdlib.h>
#include <Tempest/RenderState>

class VisualFx final {
  public:
    VisualFx(const Daedalus::GEngineClasses::CFx_Base & src);

    const Daedalus::GEngineClasses::CFx_Base& handle() const { return *fx; }

  private:
    Tempest::RenderState rs;
    const Daedalus::GEngineClasses::CFx_Base* fx=nullptr;
  };

