#pragma once

#include <Tempest/UiOverlay>

class NumberEdit;

class ColorPicker final : public Tempest::UiOverlay {
  public:
    ColorPicker(Tempest::Widget& anchor, Tempest::Vec3 color, bool preferLeft);

    bool isOpen()    const { return open;     }
    bool dismissed() const { return detached; }
    void dismiss();

    Tempest::Signal<void(Tempest::Vec3)> onChange;

  private:
    struct Wheel;
    struct ValueBar;
    struct ChannelEdit;
    struct PickerPanel;

    void resizeEvent(Tempest::SizeEvent&) override;
    void mouseDownEvent(Tempest::MouseEvent& e) override;
    void keyDownEvent(Tempest::KeyEvent& e) override;
    void updateRgb();
    void editRgb(double, bool);
    void syncHsv();

    Tempest::Widget& anchor;
    Tempest::Widget* panel = nullptr;
    Wheel*           wheel = nullptr;
    ValueBar*        valueBar = nullptr;
    NumberEdit*      channels[3] = {};
    Tempest::Vec3    rgb;
    bool             preferLeft;
    bool             open = true;
    bool             detached = false;
  };
