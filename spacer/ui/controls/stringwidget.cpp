#include "stringwidget.h"

#include <Tempest/Label>

using namespace Tempest;

StringWidget::StringWidget() : StringWidget("") {
  }

StringWidget::StringWidget(std::string_view name) {
  lbl = &addWidget(new Label());
  txt = &addWidget(new LineEdit());
  setLayout(Horizontal);
  setSizeHint(txt->sizeHint());
  setSizePolicy(Preferred,Fixed);

  lbl->setText(name);
  txt->setUndoRedoEnabled(false);

  txt->onTextEdited.bind(this,&StringWidget::onModifyed);
  }

void StringWidget::setValue(std::string_view v) {
  // if(v==val)
  //   return;
  txt->setText(v);
  onValueChanged(v);
  }

std::string_view StringWidget::value() const {
  return txt->text().c_str();
  }

void StringWidget::onModifyed(const TextModel& txt) {
  onValueModifyed(txt.c_str(), true);
  onValueChanged(txt.c_str());
  }