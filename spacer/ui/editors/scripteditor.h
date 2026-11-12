#pragma once

#include <Tempest/TextEdit>

#include "project/projectitem.h"
#include "ui/editors/baseeditor.h"
#include "ui/dragdrop.h"

class ScriptEditor : public BaseEditor,
                     public DropReciver {
  public:
    ScriptEditor();
    ~ScriptEditor() override;

  protected:
    std::string_view title() const override;
    BaseTool*        createToolpanel(ToolWindow::Tool tool) override;

    bool             load(ProjectItem& it) override;
    void             moveDropOver( DropOverEvent& ev ) override;

  private:
    ProjectItem        item;
    Tempest::TextEdit* text = nullptr;
  };
