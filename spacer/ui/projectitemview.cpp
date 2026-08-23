#include "projectitemview.h"

#include <Tempest/Painter>

#include "project/projectmgr.h"
#include "project/projectitem.h"

#include "assets/assets.h"

using namespace Tempest;

ProjectItemView::ProjectItemView(const ProjectItem& it) : it(it) {
  setMargins(4);
  auto& m = style().metrics();
  setSizeHint(Size(m.buttonSize,m.buttonSize));
  setSizePolicy(Preferred,Fixed);
  setFocusPolicy(ClickFocus);

  auto st=state();
  st.button = WidgetState::ButtonType::T_FlatButton;
  setWidgetState(st);
  }

ProjectItemView::~ProjectItemView() {
  ProjectMgr::inst().onGpuAssetChanged();
  }

void ProjectItemView::setText(std::string_view t) {
  txt = t;
  update();
  }

void ProjectItemView::setDepth(size_t d) {
  depth = d;
  update();
  }

void ProjectItemView::setAsOpen(bool open) {
  closed = !open;
  update();
  }

void ProjectItemView::mouseDownEvent(Tempest::MouseEvent& e) {
  auto st=state();
  st.pressed=true;
  setWidgetState(st);
  update();

  dd.begin(e,*this);
  }

void ProjectItemView::mouseDragEvent(MouseEvent& event) {
  dd.drag(event);
  }

void ProjectItemView::mouseUpEvent(Tempest::MouseEvent& event) {
  const bool ddEnd = dd.isDrag();
  dd.end(event);

  auto st=state();
  st.pressed=false;
  setWidgetState(st);
  update();

  if(0<=event.x && event.x<w() &&
     0<=event.y && event.y<h()){
    if(!ddEnd)
      onClick(this,it);
    }
  }

void ProjectItemView::mouseEnterEvent(MouseEvent&) {
  auto st=state();
  st.moveOver=true;
  setWidgetState(st);
  update();
  }

void ProjectItemView::mouseLeaveEvent(MouseEvent&) {
  auto st=state();
  st.moveOver=false;
  setWidgetState(st);
  update();
  }

void ProjectItemView::mouseMoveEvent(MouseEvent& event) {
  event.accept();
  }

void ProjectItemView::paintEvent(Tempest::PaintEvent& e) {
  Painter p(e);
  style().draw(p,static_cast<Button*>(nullptr),Style::E_Background,
               state(),Rect(0,0,w(),h()),Style::Extra(*this));
  auto th = p.font().textSize(txt).h;
  int  dx = 4+int(depth)*8;

  if(it.type()==ProjectItem::T_Dir || it.type()==ProjectItem::T_Project) {
    auto& ic = closed ? Assets::inst().ic.tri_close : Assets::inst().ic.tri_open;
    auto  sp = ic.sprite(w(),h(),Icon::ST_Normal);
    p.setBrush(sp);
    p.drawRect(dx,(h()-sp.h())/2,sp.w(),sp.h());
    dx+=sp.w()+4;
    }
  p.drawText(dx,h()-(h()-th)/2,txt);
  }

std::string_view ProjectItemView::text() const {
  return txt;
  }

bool ProjectItemView::isDrag() const {
  return dd.isDrag();
  }
