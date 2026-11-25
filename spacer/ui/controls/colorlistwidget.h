#pragma once

#include <Tempest/Widget>
#include <zenkit/Misc.hh>

#include <string_view>
#include <vector>

class ColorListWidget : public Tempest::Widget {
  public:
    explicit ColorListWidget(std::string_view name = "Color animation list");

    void setValue(std::vector<zenkit::Color> colors);
    const std::vector<zenkit::Color>& value() const;
    void setDefaultColor(const zenkit::Color& color);
    void setAsSmooth(bool smooth);
    void setExpanded(bool expanded);

    Tempest::Signal<void(bool commit)> onValueModifyed;
    Tempest::Signal<void()> onResize;

  private:
    struct Ramp;
    struct Frame;

    void resizeEvent(Tempest::SizeEvent&) override;
    void alignList();
    void toggleExpanded();
    void adjustSize();
    void syncFrames();
    void modifyColor(size_t index, Tempest::Vec4 value, bool commit);
    void addFrame();
    void removeFrame(size_t index);

    std::vector<zenkit::Color> colors;
    zenkit::Color              defaultColor = {255, 255, 255, 255};
    Ramp*                      preview      = nullptr;
    Tempest::Widget*           list         = nullptr;
    bool                       smooth       = true;
  };
