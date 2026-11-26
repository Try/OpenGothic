#pragma once

#include <Tempest/ComboBox>
#include <Tempest/Menu>

#include <initializer_list>

#include "game/constants.h"

class Renderer;

class ViewportSettingsMenu final : public Tempest::Menu {
  public:
    struct Settings {
      bool  animationEnabled = false;
      float timeOfDay        = 8.f;
      };

    ViewportSettingsMenu(Tempest::Widget& anchor, Renderer& renderer, Settings& settings);

    void show();

  private:
    static constexpr int width      = 400;
    static constexpr int labelWidth = 136;

    template<class Method>
    struct Option {
      const char* label;
      Method      method;
      bool        supported;
      };

    template<class Method>
    static void setMethods(Tempest::ComboBox& combo, std::vector<Method>& methods, Method current,
                           std::initializer_list<Option<Method>> options);

    Tempest::Widget* createItems(const std::vector<Item>& items) override;

    void setAnimation(bool enabled);
    void setTime(float hours);
    void setShadow(size_t index);
    void setGi(size_t index);

    Tempest::Widget&          anchor;
    Renderer&                 renderer;
    Settings&                 settings;
    std::vector<ShadowMethod> shadowMethods;
    std::vector<GiMethod>     giMethods;
  };
