#pragma once

#include <Tempest/Label>
#include <Tempest/Widget>

class Slider : public Tempest::Label {
  public:
    Slider();

    Tempest::Signal<void(double)>      onValueChanged;
    Tempest::Signal<void(double,bool)> onValueModifyed;

    void  setClampEnable(bool v);
    void  enableFloats(bool i);
    bool  hasFloats() const;
    void  setValue(float v);
    void  setRange(float min, float max);

    Tempest::Vec2 range() const;

  private:
    void  paintEvent(Tempest::PaintEvent& event) override;
    void  mouseDownEvent(Tempest::MouseEvent& e) override;
    void  mouseDragEvent(Tempest::MouseEvent& e) override;
    void  mouseUpEvent  (Tempest::MouseEvent& e) override;

    void  implMouse(Tempest::MouseEvent& e);
    void  implSet(float v, bool commit, bool modify);

    bool  clamp   = false;
    bool  integer = false;
    float min     = 0;
    float max     = 1;
    float val     = 0;
  };

