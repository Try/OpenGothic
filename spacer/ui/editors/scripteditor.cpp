#include "scripteditor.h"

#include <Tempest/TextEdit>

using namespace Tempest;

ScriptEditor::ScriptEditor() {
  text = &addWidget(new TextEdit());
  setLayout(Vertical);
  setMargins(4);
  }

ScriptEditor::~ScriptEditor() {
  }

std::string_view ScriptEditor::title() const {
  return item.name();
  }

BaseEditor::BaseTool* ScriptEditor::createToolpanel(ToolWindow::Tool tool) {
  return nullptr;
  }

bool ScriptEditor::load(ProjectItem& it) {
  if(auto p = it.get<std::string>()) {
    text->setText(*p);
    return true;
    }
  return false;
  }

void ScriptEditor::moveDropOver(DropOverEvent& ev) {
  }
