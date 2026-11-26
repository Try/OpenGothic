#include "viewportsettingsmenu.h"

#include <Tempest/CheckBox>

#include "graphics/renderer.h"
#include "ui/controls/floatwidget.h"
#include "ui/uihelper.h"

using namespace Tempest;

template<class Method>
void ViewportSettingsMenu::setMethods(ComboBox& combo, std::vector<Method>& methods, Method current,
                                      std::initializer_list<Option<Method>> options) {
  std::vector<std::string> labels;
  size_t                   selected = 0;
  methods.clear();
  for(const auto& option : options) {
    if(!option.supported)
      continue;
    if(option.method==current)
      selected = methods.size();
    methods.push_back(option.method);
    labels.emplace_back(option.label);
    }
  combo.setItems(labels);
  combo.setCurrentIndex(selected);
  }

ViewportSettingsMenu::ViewportSettingsMenu(Widget& anchor, Renderer& renderer, Settings& settings)
  : Menu(Declarator()), anchor(anchor), renderer(renderer), settings(settings) {
  }

void ViewportSettingsMenu::show() {
  exec(anchor, Point(anchor.w()-width, anchor.h()+4), false);
  }

Widget* ViewportSettingsMenu::createItems(const std::vector<Item>&) {
  auto panel = std::make_unique<Widget>();
  panel->setLayout(Vertical);
  panel->setMargins(Margin(12));
  panel->setSpacing(8);

  auto& animation = panel->addWidget(new CheckBox());
  animation.setText("Viewport animation");
  animation.setChecked(settings.animationEnabled);
  animation.onChecked.bind(this, &ViewportSettingsMenu::setAnimation);

  auto& time = UiHelper::row<FloatWidget>(panel.get(), "Time of day (hours)", labelWidth);
  time.setSliderMinMax(0.f, 24.f);
  time.setFloatDigits(2);
  time.setValue(settings.timeOfDay);
  time.onValueChanged.bind(this, &ViewportSettingsMenu::setTime);

  auto& shadow = UiHelper::row<ComboBox>(panel.get(), "Shadow method", labelWidth);
  setMethods(shadow, shadowMethods, renderer.shadowMethod(), {
    {"Off",                  ShadowMethod::None, true},
    {"Shadow maps",          ShadowMethod::ShadowMap, true},
    {"Virtual shadow maps",  ShadowMethod::VirtualShadowMap, Shaders::isVsmSupported()},
    {"Software ray tracing", ShadowMethod::SoftwareRayTracing, Shaders::isRtsmSupported()},
    });
  shadow.onItemSelected.bind(this, &ViewportSettingsMenu::setShadow);

  const bool rayTracing = Shaders::options().doRtScene;
  auto&      gi         = UiHelper::row<ComboBox>(panel.get(), "GI method", labelWidth);
  setMethods(gi, giMethods, renderer.giMethod(), {
    {"Off",              GiMethod::None,   true},
    {"Probes",           GiMethod::Probes, rayTracing && Shaders::isGi1Supported()},
    {"Irradiance cache", GiMethod::IrrC,   rayTracing && Shaders::isGi2Supported()},
    });
  gi.onItemSelected.bind(this, &ViewportSettingsMenu::setGi);

  panel->resize(width, UiHelper::wrapContent(*panel).h);
  return panel.release();
  }

void ViewportSettingsMenu::setAnimation(bool enabled) {
  settings.animationEnabled = enabled;
  anchor.update();
  }

void ViewportSettingsMenu::setTime(float hours) {
  settings.timeOfDay = hours;
  anchor.update();
  }

void ViewportSettingsMenu::setShadow(size_t index) {
  renderer.setShadowMethod(shadowMethods[index]);
  anchor.update();
  }

void ViewportSettingsMenu::setGi(size_t index) {
  renderer.setGiMethod(giMethods[index]);
  anchor.update();
  }
