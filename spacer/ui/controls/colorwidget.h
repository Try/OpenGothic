#pragma once

#include <Tempest/Widget>

class VecWidget;

namespace zenkit {
  class Color;
  }

class ColorWidget : public Tempest::Widget {
  public:
    ColorWidget(std::string_view name);

    Tempest::Vec3 value() const;
    void          setValue(const Tempest::Vec3& v);
    void          setValue(const zenkit::Color& v);

    Tempest::Signal<void(Tempest::Vec4 v,bool commit)> onValueModifyed;
    Tempest::Signal<void()> onResize;

  private:
    struct ClrCycle;
    void adjustSize();
    void modifyProxy(Tempest::Vec4 v,bool commit);

    ClrCycle*  clr  = nullptr;
    VecWidget* edit = nullptr;
  };

