#include "floatwidget.h"

#include <Tempest/Painter>

#include "numberedit.h"
#include "slider.h"
#include "assets.h"

using namespace Tempest;

struct FloatWidget::Num : NumberEdit {
  void paintEvent(PaintEvent& e) override {
    Painter p(e);
    p.setBrush(Assets::inst().colors.workspaceD);
    p.drawRect(0,0,w(),h());

    style().draw(p, text(), Style::TE_TextEditContent, state(), Rect(0,0,w(),h()), Style::Extra(*this));
    }
  };

FloatWidget::FloatWidget() : FloatWidget("") {
  }

FloatWidget::FloatWidget(std::string_view name) {
  lbl = &addWidget(new Slider());
  txt = &addWidget(new Num());
  setLayout(Horizontal);
  setSizeHint(txt->sizeHint());
  setSizePolicy(Preferred,Fixed);

  txt->setMinimumSize(75,txt->minSize().h);
  txt->setSizePolicy(Fixed,Fixed);
  lbl->setText(name);
  val = 0;

  txt->enableFloats(true);
  txt->onValueModifyed.bind(this,&FloatWidget::onModifyed);

  lbl->setClampEnable(false);
  lbl->onValueModifyed.bind(this,&FloatWidget::onSlide);
  }

void FloatWidget::setTitle(std::string_view name) {
  lbl->setText(name);
  }

void FloatWidget::setSliderMinMax(float min, float max) {
  lbl->setRange(min,max);
  }

Vec2 FloatWidget::sliderRange() const {
  return lbl->range();
  }

void FloatWidget::enableFloats(bool e) {
  txt->enableFloats(e);
  lbl->enableFloats(e);
  }

bool FloatWidget::hasFloats() const {
  return lbl->hasFloats();
  }

void FloatWidget::setFloatDigits(uint8_t d) {
  return txt->setFloatDigits(d);
  }

void FloatWidget::setUndoRedoEnabled(bool e) {
  txt->setUndoRedoEnabled(e);
  }

void FloatWidget::setValue(float v) {
  if(v==val)
    return;
  txt->setValue(v);
  lbl->setValue(v);

  val = v;
  onValueChanged(v);
  }

float FloatWidget::value() const {
  return float(txt->value());
  }

void FloatWidget::onModifyed(double dv, bool commit) {
  float v = float(dv);
  if(v==val && !commit)
    return;
  val = v;
  lbl->setValue(val);

  onValueModifyed(float(v),commit);
  onValueChanged (float(v));
  }

void FloatWidget::onSlide(double dv, bool commit) {
  float v = float(dv);
  if(v==val && !commit)
    return;
  val = v;
  txt->setValue(val);

  onValueModifyed(float(v),commit);
  onValueChanged (float(v));
  }
