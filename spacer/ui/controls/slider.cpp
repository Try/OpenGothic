#include "slider.h"

#include <Tempest/Painter>

#include "assets.h"

using namespace Tempest;

Slider::Slider() {
  setMargins(Margin(4));
  }

void Slider::setClampEnable(bool v) {
  clamp = v;
  setValue(val);
  }

void Slider::enableFloats(bool i) {
  integer = !i;
  setValue(val);
  }

bool Slider::hasFloats() const {
  return !integer;
  }

void Slider::setValue(float v) {
  implSet(v,true,false);
  }

void Slider::setRange(float a, float b) {
  min = std::min(a,b);
  max = std::max(a,b);
  setValue(val);
  update();
  }

Vec2 Slider::range() const {
  return Vec2(min,max);
  }

void Slider::paintEvent(PaintEvent& e) {
  const int  sz = 6;
  const auto sp = Assets::inst().ic.circle.sprite(h(), h(), Tempest::Icon::ST_Normal);

  Painter p(e);
  float v   = std::max(min,std::min(val,max))-min;
  int   x   = 0;
  if(max>min)
    x = int((v/(max-min))*(w() - sp.w())) + sp.w()/2;

  p.setBrush(Assets::inst().colors.workspaceD);
  p.drawRect(0,0,w(),h());

  p.setBrush(Assets::inst().colors.highlight);
  p.drawRect(0, (h()-sz)/2, x, sz);

  p.setBrush(Assets::inst().colors.workspace);
  p.drawRect(x, (h()-sz)/2, w(), sz);

  p.setBrush(Brush(sp, Assets::inst().colors.highlight));
  p.drawRect(x-sp.w()/2, (h()-sp.h())/2, sp.h(), sp.h());

  Label::paintEvent(e);
  }

void Slider::mouseDownEvent(MouseEvent&) {
  update();
  }

void Slider::mouseDragEvent(MouseEvent& e) {
  implMouse(e);
  }

void Slider::mouseUpEvent(MouseEvent& e) {
  implMouse(e);
  }

void Slider::implMouse(MouseEvent& e) {
  auto  sp = Assets::inst().ic.circle.sprite(h(), h(), Tempest::Icon::ST_Normal);
  float v  = float(e.x - sp.w()/2)/float(w()-sp.w());
  v = std::max(0.f,std::min(v,1.f));
  implSet(v*(max-min)+min,e.type()==Event::MouseUp,true);
  }

void Slider::implSet(float v, bool commit, bool modify) {
  if(clamp)
    v = std::max(min,std::min(v,max));
  if(integer) {
    v = std::round(v);
    v = std::max(std::ceil(min),std::min(v,std::floor(max)));
    }

  if(std::fabs(max-min)>1.f)
    v = std::round(v*100.f)/100.f; else
    v = std::round(v*1000.f)/1000.f;

  if(v==val && !commit)
    return;
  val = v;
  onValueChanged(val);
  if(modify)
    onValueModifyed(val,commit);
  update();
  }

