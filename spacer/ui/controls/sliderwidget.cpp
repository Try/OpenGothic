#include "sliderwidget.h"

#include <Tempest/Label>
#include <Tempest/Painter>

#include "ui/controls/numberedit.h"
#include "assets.h"
#include "ui/controls/slider.h"
#include "ui/uihelper.h"
#include "ui/uistyle.h"

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
  auto& lbl = addWidget(new Label());
  auto& rgh = addWidget(new Widget());

  txt = &rgh.addWidget(new NumberEdit());
  sld = &rgh.addWidget(new Slider());

  lbl.setText(name);
  txt->setMinimumSize(UiHelper::inputSizeHint(*txt, ".8888 ").w, 0);
  txt->setSizePolicy(Fixed,Fixed);
  sld->setSizePolicy(Expanding,Preferred);

  rgh.setMinimumSize(txt->minSize().w + sld->minSize().w, 0);
  rgh.setSpacing(4);
  rgh.setLayout(Horizontal);

  setSizeHint(0,UiStyle::propertyRowHeight);
  setSizePolicy(Preferred,Fixed);
  setLayout(Horizontal);

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
