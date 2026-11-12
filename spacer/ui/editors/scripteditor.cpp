#include "scripteditor.h"

#include <Tempest/Application>
#include <Tempest/ScrollWidget>
#include <Tempest/TextEdit>

using namespace Tempest;

ScriptEditor::ScriptEditor() {
  auto& sc = addWidget(new ScrollWidget(Tempest::Vertical));
  text = &sc.centralWidget().addWidget(new TextEdit());
  auto font = Application::defaultFont();
  font.setPixelSize(19);
  text->setFont(font);
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
