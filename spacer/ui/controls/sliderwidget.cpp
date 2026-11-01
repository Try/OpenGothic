#include "sliderwidget.h"

#include <Tempest/Label>
#include <Tempest/Painter>

#include "ui/controls/numberedit.h"
#include "assets.h"
#include "ui/controls/slider.h"
#include "ui/uihelper.h"

using namespace Tempest;

struct SliderWidget::Wrap : Widget {
  void paintEvent(PaintEvent& e) override {
    Painter p(e);
    p.setBrush(Assets::inst().colors.workspaceD);
    p.drawRect(0,0,w(),h());
    }
  };

SliderWidget::SliderWidget() : SliderWidget("") {
  }

SliderWidget::SliderWidget(std::string_view name) {
  auto& lft = addWidget(new Widget());
  auto& rgh = addWidget(new Wrap());

  auto lbl = &lft.addWidget(new Label());
  lft.addWidget(new Widget());
  txt = &rgh.addWidget(new NumberEdit());
  sld = &rgh.addWidget(new Slider());

  lft.setLayout(Vertical);
  rgh.setLayout(Vertical);
  rgh.setMinimumSize(UiHelper::wrapContent(rgh));

  auto sz = UiHelper::wrapContent(*this, Horizontal);
  setSizeHint(sz, margins());
  setSizePolicy(Preferred,Fixed);
  setLayout(Horizontal);

  lbl->setText(name);

  txt->enableFloats(true);
  txt->onValueModifyed.bind(this,&SliderWidget::onModifyed);

  sld->setClampEnable(false);
  sld->onValueModifyed.bind(this,&SliderWidget::onSlide);
  }

void SliderWidget::setUndoRedoEnabled(bool e) {
  txt->setUndoRedoEnabled(e);
  }

void SliderWidget::setSliderMinMax(float min, float max) {
  sld->setRange(min,max);
  }

Vec2 SliderWidget::sliderRange() const {
  return sld->range();
  }

void SliderWidget::enableFloats(bool e) {
  txt->enableFloats(e);
  sld->enableFloats(e);
  }

void SliderWidget::setValue(float v) {
  if(v==val)
    return;
  txt->setValue(v);
  sld->setValue(v);

  val = v;
  onValueChanged(v);
  }

float SliderWidget::value() const {
  return float(txt->value());
  }

void SliderWidget::onModifyed(double dv, bool commit) {
  float v = float(dv);
  if(v==val && !commit)
    return;
  val = v;
  sld->setValue(val);

  onValueModifyed(float(v),commit);
  onValueChanged (float(v));
  }

void SliderWidget::onSlide(double dv, bool commit) {
  float v = float(dv);
  if(v==val && !commit)
    return;
  val = v;
  txt->setValue(val);

  onValueModifyed(float(v),commit);
  onValueChanged (float(v));
  }