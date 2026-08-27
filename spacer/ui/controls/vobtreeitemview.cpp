#include "vobtreeitemview.h"

#include <Tempest/Painter>

#include "assets.h"

using namespace Tempest;

VobTreeItemView::VobTreeItemView(VobTreeDelegate& owner, size_t id)
  : owner(owner), id(id) {
  setMargins(4);
  auto& m = style().metrics();
  setSizeHint(Size(m.buttonSize,m.buttonSize));
  setSizePolicy(Preferred,Fixed);
  setFocusPolicy(ClickFocus);

  auto st = state();
  st.button = WidgetState::ButtonType::T_FlatButton;
  setWidgetState(st);
  }

VobTreeItemView::~VobTreeItemView() {
  }

void VobTreeItemView::setText(std::string_view t) {
  txt = t;
  update();
  }

void VobTreeItemView::setDepth(size_t d) {
  depth = d;
  update();
  }

void VobTreeItemView::setAsOpen(bool open) {
  closed = !open;
  update();
  }

void VobTreeItemView::mouseDownEvent(Tempest::MouseEvent& e) {
  auto st = state();
  st.pressed=true;
  setWidgetState(st);
  update();
  }

void VobTreeItemView::mouseUpEvent(Tempest::MouseEvent& event) {
  auto st=state();
  st.pressed=false;
  setWidgetState(st);
  update();

  onClick(this,id);
  }

void VobTreeItemView::mouseEnterEvent(MouseEvent&) {
  auto st=state();
  st.moveOver=true;
  setWidgetState(st);
  update();
  }

void VobTreeItemView::mouseLeaveEvent(MouseEvent&) {
  auto st=state();
  st.moveOver=false;
  setWidgetState(st);
  update();
  }

void VobTreeItemView::mouseMoveEvent(MouseEvent& event) {
  event.accept();
  }

void VobTreeItemView::paintEvent(Tempest::PaintEvent& e) {
  //auto& it = world.vobById(id);

  Painter p(e);
  style().draw(p,static_cast<Button*>(nullptr),Style::E_Background,
               state(),Rect(0,0,w(),h()),Style::Extra(*this));
  auto th = p.font().textSize(txt).h;
  int  dx = 4+int(depth)*8;

  if(true) {
    // icon placeholder
    auto& ic = Assets::inst().ic.check_on;
    auto  sp = ic.sprite(w(),h(),Icon::ST_Normal);
    p.setBrush(sp);
    p.drawRect(dx,(h()-sp.h())/2,sp.w(),sp.h());
    dx+=sp.w()+4;
    }

  p.drawText(dx,h()-(h()-th)/2,txt);
  }

std::string_view VobTreeItemView::text() const {
  return txt;
  }
