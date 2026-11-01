#pragma once

#include <Tempest/Widget>

class NumberEdit;
class Slider;

class SliderWidget : public Tempest::Widget {
  public:
    SliderWidget();
    SliderWidget(std::string_view name);

    void  setUndoRedoEnabled(bool e);
    void          setSliderMinMax(float min, float max);
    Tempest::Vec2 sliderRange() const;

    void          enableFloats(bool integer);

    void          setValue(float v);
    float         value() const;

    Tempest::Signal<void(float)> onValueChanged;
    Tempest::Signal<void(float v,bool commit)> onValueModifyed;

  private:
    struct Wrap;
    void onModifyed(double fv, bool commit);
    void onSlide(double fv, bool commit);

    Slider*     sld = nullptr;
    NumberEdit* txt = nullptr;
    float       val = 0;
  };

