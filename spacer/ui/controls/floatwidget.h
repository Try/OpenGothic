#pragma once

#include <Tempest/Widget>
#include <Tempest/Label>
#include <Tempest/LineEdit>

class NumberEdit;
class Slider;

class FloatWidget : public Tempest::Widget {
  public:
    FloatWidget();
    FloatWidget(std::string_view name);

    void          setTitle(std::string_view name);

    void          setSliderMinMax(float min, float max);
    Tempest::Vec2 sliderRange() const;

    void          enableFloats(bool integer);
    bool          hasFloats() const;
    void          setFloatDigits(uint8_t d);

    void          setValue(float v);
    float         value() const;

    void          setUndoRedoEnabled(bool e);

    Tempest::Signal<void(float)> onValueChanged;
    Tempest::Signal<void(float v,bool commit)> onValueModifyed;

  private:
    struct Num;
    Slider*         lbl = nullptr;
    NumberEdit*     txt = nullptr;
    float           val = 0;

    void            onModifyed(double fv, bool commit);
    void            onSlide(double fv, bool commit);
  };

