#pragma once

#include <Tempest/Widget>

class ColorPicker;

namespace zenkit {
  struct Color;
  }

class ColorWidget : public Tempest::Widget {
  public:
    explicit ColorWidget(std::string_view name, std::string_view swatchText = {});
    ~ColorWidget() override;

    Tempest::Vec3 value() const;
    void          setValue(const Tempest::Vec3& v);
    void          setValue(const zenkit::Color& v);

    Tempest::Signal<void(Tempest::Vec4 v,bool commit)> onValueModifyed;

  protected:
    void setContextMenuEnabled(bool enabled);

  private:
    struct Swatch;

    void openPicker();
    void previewColor(Tempest::Vec3 v);

    Swatch*       swatch = nullptr;
    ColorPicker*  picker = nullptr;
    Tempest::Vec3 rgb = {};
    Tempest::Vec3 original = {};
    bool          modified = false;
  };
