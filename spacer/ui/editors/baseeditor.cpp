#include "baseeditor.h"

using namespace Tempest;

BaseEditor::BaseEditor() {
  setMargins(Margin(0,0,0,0));
  }

void BaseEditor::mouseDownEvent(Tempest::MouseEvent& e) {
  e.accept();
  }

void BaseEditor::mouseMoveEvent(Tempest::MouseEvent& e) {
  e.accept();
  }


BaseEditor::BaseTool::BaseTool() {
  setMargins(Margin(0,0,0,0));
  }
