#include "enumwidget.h"

#include <Tempest/ComboBox>
#include <Tempest/Label>

using namespace Tempest;

EnumWidget::EnumWidget(std::string_view hint) {
  auto& lbl = addWidget(new Label());
  val = &addWidget(new ComboBox());

  lbl.setText(hint);

  setSizeHint(0,27);
  setSizePolicy(Preferred,Fixed);
  setLayout(Horizontal);

  val->onItemSelected    .bind(&onItemSelected,    &Signal<void(size_t)>::operator());
  val->onSelectionChanged.bind(&onSelectionChanged,&Signal<void(size_t)>::operator());
  }

void EnumWidget::setItems(const std::vector<std::string>& it) {
  val->setItems(it);
  eltCount = it.size();
  }

void EnumWidget::setCurrentIndex(size_t id) {
  if(id>=eltCount)
    return;
  val->setCurrentIndex(id);
  }

size_t EnumWidget::currentIndex() const {
  return val->currentIndex();
  }
