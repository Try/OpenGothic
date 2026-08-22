#include "intwidget.h"

#include "numberedit.h"

using namespace Tempest;

IntWidget::IntWidget() : IntWidget("") {
  }

IntWidget::IntWidget(std::string_view name) {
  lbl = &addWidget(new Label());
  txt = &addWidget(new NumberEdit());
  setLayout(Horizontal);
  setSizeHint(txt->sizeHint());
  setSizePolicy(Preferred,Fixed);

  lbl->setText(name);
  txt->enableFloats(false);
  txt->setUndoRedoEnabled(false);
  val = 0;

  txt->onValueModifyed.bind(this,&IntWidget::onModifyed);
  }

void IntWidget::setValue(int v) {
  if(v==val)
    return;
  txt->setValue(v);

  val = v;
  onValueChanged(v);
  }

int IntWidget::value() const {
  return int(txt->value());
  }

void IntWidget::onModifyed(double dv, bool commit) {
  int v = int(dv);
  if(v==val)
    return;
  onValueModifyed(v,commit);
  onValueChanged(v);
  }
