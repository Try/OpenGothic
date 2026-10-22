#include "widgetheader.h"

#include <Tempest/Application>
#include <Tempest/Painter>

#include "assets.h"

using namespace Tempest;

WidgetHeader::WidgetHeader() {
  setButtonType(T_FlatButton);
  setMargins(Margin(0,0,0,0));

  auto f = Application::font();
  f.setBold(true);
  setFont(f);
  }

void WidgetHeader::paintEvent(Tempest::PaintEvent &e) {
  Painter p(e);
  style().draw(p,static_cast<Button*>(nullptr),Style::E_Background,
               state(),Rect(0,0,w(),h()),Style::Extra(*this));

  auto& ic = closed ? Assets::inst().ic.down : Assets::inst().ic.up;
  auto  sp = ic.sprite(w(),h(),Icon::ST_Normal);
  p.setBrush(sp);
  p.drawRect(margins().left+4, (h()-sp.h())/2, sp.w(), sp.h());

  p.setFont(font());
  auto th = p.font().textSize(title).h;
  p.drawText(margins().left + sp.w() + 8,h()-(h()-th)/2,title);
  }

void WidgetHeader::mouseWheelEvent(Tempest::MouseEvent& e) {
  e.ignore();
  }

void WidgetHeader::setText(std::string_view s) {
  title = s;
  update();
  }

void WidgetHeader::setClosed(bool c) {
  closed = c;
  update();
  }

void WidgetHeader::emitClick() {
  closed = !closed;
  update();
  Button::emitClick();
  }

void WidgetHeader::setPriview(const Tempest::Vec4& c) {
  cl = c;
  update();
  }

void WidgetHeader::showPriview(bool s) {
  showCl = s;
  update();
  }
