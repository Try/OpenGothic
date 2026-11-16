#include "meshwidget.h"

#include <Tempest/Label>
#include <Tempest/Painter>

#include "ui/projectitemview.h"
#include "utils/fileext.h"

using namespace Tempest;


struct MeshWidget::Combo : Tempest::Label {
  void paintEvent(Tempest::PaintEvent& e) override {
    Tempest::Painter p(e);
    style().draw(p, (ComboBox*)nullptr, Style::E_Background, state(),Rect(0,0,w(),h()),Style::Extra(*this));
    style().draw(p, text(),             Style::TE_LabelTitle,state(),Rect(0,0,w(),h()),Style::Extra(*this));
    }
  };

MeshWidget::MeshWidget() : MeshWidget("") {
  }

MeshWidget::MeshWidget(std::string_view name) {
  lbl = &addWidget(new Label());
  txt = &addWidget(new Combo());

  setMargins(0);
  auto& m = style().metrics();
  setSizeHint(Size(m.buttonSize,m.buttonSize));
  setSizePolicy(Preferred,Fixed);
  setLayout(Horizontal);

  lbl->setText(name);
  // txt->setUndoRedoEnabled(false);
  // txt->onTextEdited.bind(this,&MeshWidget::onModifyed);
  }

void MeshWidget::setValue(std::shared_ptr<zenkit::Visual> v) {
  if(v!=nullptr)
    vis = *v; else
    vis = zenkit::Visual();
  txt->setText(vis.name);
  onValueChanged(vis);
  }

const zenkit::Visual& MeshWidget::value() const {
  return vis;
  }

void MeshWidget::moveDropOver(DropOverEvent& ev) {
  if(auto itm = dynamic_cast<ProjectItemView*>(&ev.drop())) {
    if(itm->it.type()==ProjectItem::T_StaticMesh ||
       itm->it.type()==ProjectItem::T_AnimMesh) {
      ev.accept();
      }
    }
  }

void MeshWidget::dropDone(DropOverEvent& ev) {
  if(auto itm = dynamic_cast<ProjectItemView*>(&ev.drop())) {
    if(itm->it.type()==ProjectItem::T_StaticMesh) {
      auto name = std::string(itm->it.name());
      if(FileExt::exchangeExt(name,"MRM","3DS") && vis.name!=name) {
        vis.name = name;
        vis.type = zenkit::VisualType::MESH;
        txt->setText(vis.name);
        onModifyed();
        }
      update();
      }
    if(itm->it.type()==ProjectItem::T_AnimMesh) {
      auto name = std::string(itm->it.name());
      if(FileExt::exchangeExt(name,"MDL","ASC") && vis.name!=name) {
        vis.name = name;
        vis.type = zenkit::VisualType::MODEL;
        txt->setText(vis.name);
        onModifyed();
        }
      update();
      }
    }
  }

void MeshWidget::onModifyed() {
  onValueModifyed(vis, true);
  onValueChanged(vis);
  }
