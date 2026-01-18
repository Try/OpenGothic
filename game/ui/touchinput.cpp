#include "touchinput.h"

#include <Tempest/Painter>

#include "game/playercontrol.h"

using namespace Tempest;

TouchInput::TouchInput(PlayerControl& ctrl) : ctrl(ctrl) {
  }

void TouchInput::paintEvent(Tempest::PaintEvent& e) {
  Painter p(e);

  p.setBrush(Color(1,1,1, 0.2f));
  p.drawRect(10, h()-10 - 100, 100, 100);
  }

void TouchInput::mouseDownEvent(Tempest::MouseEvent& e) {
  auto r = Rect(10, h()-10-100, 100, 100);
  if(!r.contains(e.pos())) {
    e.ignore();
    return;
    }

  mpos = e.pos();
  ctrl.onKeyPressed(KeyCodec::Forward, KeyEvent::K_NoKey);
  }

void TouchInput::mouseDragEvent(Tempest::MouseEvent& e) {
  auto dp = e.pos() - mpos;
  mpos = e.pos();
  if(std::abs(dp.x)>0) {
    const float scale = 4.f;
    ctrl.onRotateMouse(float(-dp.x) * scale, 0.f);
    }
  }

void TouchInput::mouseUpEvent(Tempest::MouseEvent& e) {
  ctrl.onKeyReleased(KeyCodec::Forward);
  }
