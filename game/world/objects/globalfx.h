#pragma once

#include <memory>
#include "game/globaleffects.h"

class GlobalFx {
  public:
    GlobalFx();
    GlobalFx(GlobalFx&& other);
    GlobalFx& operator = (GlobalFx&& other);
    ~GlobalFx();

  private:
    GlobalFx(const std::shared_ptr<GlobalEffects::Effect>& h);
    std::shared_ptr<GlobalEffects::Effect> h;
  friend class GlobalEffects;
  };

