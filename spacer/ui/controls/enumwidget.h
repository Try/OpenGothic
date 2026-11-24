#pragma once

#include <Tempest/ComboBox>
#include <Tempest/Widget>
#include <Tempest/TextModel>

class EnumWidget : public Tempest::Widget {
  public:
    enum class DisplayPolicy : uint8_t {
      Combo,
      Segment
      };

    EnumWidget(std::string_view name);

    Tempest::Signal<void(size_t)> onItemSelected;
    Tempest::Signal<void(size_t)> onSelectionChanged;

    void   setDisplayPolicy(DisplayPolicy policy);
    void   setItems        (const std::vector<std::string>& values);
    void   setCurrentIndex (size_t index);
    size_t currentIndex() const;

  private:
    void resizeEvent    (Tempest::SizeEvent& e) override;
    void paintEvent     (Tempest::PaintEvent& e) override;
    void mouseDownEvent (Tempest::MouseEvent& e) override;
    void mouseUpEvent   (Tempest::MouseEvent& e) override;
    void mouseMoveEvent (Tempest::MouseEvent& e) override;
    void mouseLeaveEvent(Tempest::MouseEvent& e) override;
    void keyDownEvent   (Tempest::KeyEvent& e) override;

    void          updateLayout();
    Tempest::Rect segmentRect (int index) const;
    int           segmentAt(int x, int y) const;
    void          select(int index);

    Tempest::ComboBox              combo;
    Tempest::TextModel             title;
    std::vector<Tempest::TextModel> items;
    Tempest::Rect                  area;
    DisplayPolicy                  displayPolicy = DisplayPolicy::Combo;
    int                            requiredWidth = 0;
    int                            hovered       = -1;
    int                            pressed       = -1;
  };
