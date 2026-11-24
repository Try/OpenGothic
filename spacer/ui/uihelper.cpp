#include "uihelper.h"

#include <Tempest/LineEdit>

using namespace Tempest;

Size UiHelper::inputSizeHint(const LineEdit& input, std::string_view sample) {
  const auto  textSize = input.font().textSize(sample);
  const auto& margins  = input.margins();
  return Size(textSize.w + margins.xMargin(), textSize.h + margins.yMargin());
  }

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
