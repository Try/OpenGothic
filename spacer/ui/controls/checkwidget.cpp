#include "checkwidget.h"

using namespace Tempest;

CheckWidget::CheckWidget(std::string_view hint) {
  auto& lbl = addWidget(new Label());
  val = &addWidget(new CheckBox());

  lbl.setText(hint);

  setSizeHint(0,27);
  setSizePolicy(Preferred,Fixed);
  setLayout(Horizontal);

  val->onChecked    .bind(&onValueChanged,    &Signal<void(bool)>::operator());
  // val->onSelectionChanged.bind(&onSelectionChanged,&Signal<void(size_t)>::operator());
  }

void CheckWidget::setValue(bool v) {
  val->setChecked(v);
  }

bool CheckWidget::value() const {
  return val->isChecked();
  }
