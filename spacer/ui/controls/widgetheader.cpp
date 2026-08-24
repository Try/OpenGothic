#include "widgetheader.h"

#include <Tempest/Painter>

#include "assets.h"

using namespace Tempest;

WidgetHeader::WidgetHeader() {
  setButtonType(T_FlatButton);
  setMargins(Margin(0,0,0,0));
  }

void WidgetHeader::paintEvent(Tempest::PaintEvent &e) {
  Painter p(e);
  style().draw(p,static_cast<Button*>(nullptr),Style::E_Background,
               state(),Rect(0,0,w(),h()),Style::Extra(*this));

  auto th = p.font().textSize(title).h;
  p.drawText(margins().left,h()-(h()-th)/2,title);

  auto& ic = closed ? Assets::inst().ic.down : Assets::inst().ic.up;
  auto sp = ic.sprite(w(),h(),Icon::ST_Normal);
  int dx = w()-margins().right-sp.w();
  p.setBrush(sp);
  p.drawRect(dx,(h()-sp.h())/2,sp.w(),sp.h());
  dx -=sp.w()+8;

  if(showCl) {
    /*
    auto& ic = Assets::inst().socket;
    p.setBrush(Brush(ic,Color(cl.x,cl.y,cl.z,1)));
    p.drawRect(dx,(h()-16)/2,16,16,
               0,0,p.brush().w(),p.brush().h());
    */
    }
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
