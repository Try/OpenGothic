#pragma once

#include "ui/editors/baseeditor.h"
#include "ui/property/propertylist.h"
#include "ui/dragdrop.h"

class WorldEditor: public BaseEditor,
                   public DropReciver  {
  public:
    WorldEditor();
    ~WorldEditor() override;

  protected:
    std::string_view   title  () const override;
    BaseTool*          createToolpanel(ToolWindow::Tool tool) override;

    void               undo() override;
    void               redo() override;

    void               moveDropOver (DropOverEvent& ev) override;
    void               dropDone     (DropOverEvent& ev) override;

    void paintEvent(Tempest::PaintEvent& e) override;

  private:
    std::vector<PropertyList::Prop> props;
  };