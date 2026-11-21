#pragma once

#include <Tempest/LineEdit>

#include <string>

class SearchLine : public Tempest::LineEdit {
  public:
    explicit SearchLine(std::string_view placeholder = "Search");

  protected:
    void paintEvent    (Tempest::PaintEvent& e) override;
    void mouseDownEvent(Tempest::MouseEvent& e) override;

  private:
    Tempest::Rect actionRect() const;

    std::string placeholder;
  };
