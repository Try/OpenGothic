#pragma once

#include <Tempest/Panel>
#include <Tempest/Widget>

#include "ui/toolwindow.h"

class BaseEditor : public Tempest::Widget {
  public:
    BaseEditor();

    enum ToolType {
      Left,
      Right,
      Bottom,
      Count
      };

    class BaseTool : public Tempest::Panel {
      public:
        BaseTool();
      };

    virtual auto      title() const -> std::string_view = 0;
    virtual BaseTool* createToolpanel(ToolWindow::Tool tool) = 0;
    // virtual void      preload(ProjectItem& it) const = 0;
    // virtual bool      load(ProjectItem& it) = 0;
    virtual void      save() {}
    virtual bool      hasUnsavedChanges() const { return false; }

    virtual void      undo() {}
    virtual void      redo() {}

    Tempest::Signal<void()> invalidateTab;

  protected:
    void mouseDownEvent(Tempest::MouseEvent& e) override;
    void mouseMoveEvent(Tempest::MouseEvent& e) override;
  };

