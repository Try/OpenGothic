#pragma once

#include <Tempest/Button>

class WidgetHeader : public Tempest::Button {
  public:
    WidgetHeader();

    void setText(std::string_view s);

    bool isClose() const { return closed; }
    void setClosed(bool c);

    void setPriview(const Tempest::Vec4& c);

    void showPriview(bool s);

  protected:
    void emitClick() override;

    void paintEvent(Tempest::PaintEvent &e) override;
    void mouseWheelEvent(Tempest::MouseEvent& e) override;

  private:
    bool          closed=true;
    std::string   title;

    Tempest::Vec4 cl;
    bool          showCl   =true;
  };

