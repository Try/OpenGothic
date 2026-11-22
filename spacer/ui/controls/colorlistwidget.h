#pragma once

#include <Tempest/Widget>
#include <zenkit/Misc.hh>

#include <string_view>
#include <vector>

class ColorWidget;
class NumberEdit;

class ColorListWidget : public Tempest::Widget {
  public:
    explicit ColorListWidget(std::string_view name = "Color animation list");

    void setValue(std::vector<zenkit::Color> colors);
    const std::vector<zenkit::Color>& value() const;
    void setTiming(float fps, bool smooth);
    void setExpanded(bool expanded);

    Tempest::Signal<void(bool commit)> onValueModifyed;
    Tempest::Signal<void()> onResize;

  private:
    struct Ramp;
    static constexpr size_t NoSelection = size_t(-1);

    void toggleExpanded();
    void adjustSize();
    void select(size_t index);
    void updateTime();
    void modifyColor(Tempest::Vec4 value, bool commit);
    void modifyTime(float t, bool commit);
    void updateRamps();

    std::vector<zenkit::Color> colors;
    size_t           selected = NoSelection;
    float            fps      = 0;
    bool             smooth   = true;

    Ramp*            preview  = nullptr;
    Ramp*            ramp     = nullptr;
    Tempest::Widget* timeRow  = nullptr;
    NumberEdit*      timeEdit = nullptr;
    ColorWidget*     color    = nullptr;
  };
