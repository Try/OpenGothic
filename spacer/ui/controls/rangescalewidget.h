#pragma once

#include <Tempest/Widget>

#include <string_view>
#include <vector>

class RangeScaleWidget : public Tempest::Widget {
  public:
    explicit RangeScaleWidget(std::string_view name);

    void setValue(std::vector<float> samples);
    const std::vector<float>& value() const;

    Tempest::Signal<void(bool commit)> onValueModifyed;
    Tempest::Signal<void()> onResize;

  protected:
    void resizeEvent(Tempest::SizeEvent&) override;

  private:
    struct Toggle;
    struct Frame;

    void toggleExpanded();
    void alignAddButton();
    void adjustSize();
    void syncFrames();
    void addSample();
    void removeSample(size_t index);

    std::vector<float> samples;

    Toggle*          toggle  = nullptr;
    Tempest::Widget* list    = nullptr;
    Tempest::Widget* addRow  = nullptr;
  };
