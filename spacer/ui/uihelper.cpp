#include "uihelper.h"

using namespace Tempest;

Button* UiHelper::btn(const char* name) {
  auto* b = new Button();
  b->setText(name);
  b->setButtonType(Button::T_FlatButton);
  return b;
  }

Button* UiHelper::toolBtn(const Icon& ic) {
  auto* b = new Button();
  b->setIcon(ic);
  b->setMaximumSize(Size(27));
  b->setSizePolicy(Fixed);
  b->setMargins(Margin(0));
  b->setButtonType(Button::T_FlatButton);
  return b;
  }
