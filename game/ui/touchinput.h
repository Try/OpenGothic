#pragma once

#include <Tempest/Widget>

class PlayerControl;

class TouchInput : public Tempest::Widget {
  public:
    TouchInput(PlayerControl& ctrl);

    void paintEvent(Tempest::PaintEvent& e);
    void mouseDownEvent(Tempest::MouseEvent& e);
    void mouseUpEvent(Tempest::MouseEvent& e);

  private:
    PlayerControl& ctrl;
  };

