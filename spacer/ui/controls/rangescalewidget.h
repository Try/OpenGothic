#pragma once

#include <Tempest/Widget>

#include <string_view>
#include <vector>

namespace Tempest { class Button; }

class NumberEdit;

class RangeScaleWidget : public Tempest::Widget {
  public:
    explicit RangeScaleWidget(std::string_view name);

    void setValue(std::vector<float> samples);
    const std::vector<float>& value() const;
    void setTiming(float fps, bool smooth);

    Tempest::Signal<void(bool commit)> onValueModifyed;
    Tempest::Signal<void()> onResize;

  private:
    struct Curve;

    void toggleExpanded();
    void refresh();
    void select(size_t index);
    void addSample();
    void removeSample();
    void editScale(double value, bool commit);
    void changeScale(float value, bool commit);

    std::vector<float> samples;
    size_t selected = 0;
    float fps = 0.f;
    bool smooth = true;
    bool expanded = false;
    bool editing = false;

    Curve* preview = nullptr;
    Curve* curve = nullptr;
    Tempest::Widget* actions = nullptr;
    Tempest::Widget* timeRow = nullptr;
    Tempest::Widget* scaleRow = nullptr;
    Tempest::Button* remove = nullptr;
    NumberEdit* time = nullptr;
    NumberEdit* scale = nullptr;
  };
