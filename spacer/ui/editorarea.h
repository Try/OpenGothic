#pragma once

#include <Tempest/Shortcut>
#include <Tempest/Widget>

#include "ui/toolwindow.h"
#include "ui/dragdrop.h"

class BaseEditor;
class Tabs;
class ToolGroup;
class ResizableArea;
class ProjectTree;

class EditorArea : public Tempest::Widget,
                   public DropReciver {
  public:
    EditorArea();
    ~EditorArea();

    void               load();
    void               save();
    void               undo();
    void               redo();
    bool               closeApp();

    void               openApp();
    void               closeAll();

    size_t             editorsCount() const { return editor.size(); }
    std::string_view   editorTitle(size_t i) const;
    bool               hasUnsavedChanges(size_t i) const;

  private:
    enum {
      DragPadding = 20
      };

    struct EditorWrapper;
    struct ToolArea;
    struct TopBar;

    void moveDropOver(DropOverEvent& ev) override;
    void moveDropLeave(DropOverEvent&) override;
    void dropDone(DropOverEvent& ev) override;

    void pokeLoading();
    void showEditor (size_t i);
    void closeEditor(size_t i);
    void openFile   (size_t id);
    void onFilesysChange();
    void invalidateTools();
    void saveUiLayout();
    void updateOpenFileList();

    EditorWrapper* currentEditor();

    template<class T>
    void implLoad();

    std::vector<EditorWrapper*> editor;

    Tabs*          tabs     = nullptr;
    ResizableArea* main     = nullptr;
    Widget*        central  = nullptr;

    ProjectTree*   projTree = nullptr;
    ToolArea*      areaL    = nullptr;
    ToolArea*      areaR    = nullptr;
    ToolArea*      areaB    = nullptr;
    ResizableArea* areaM   = nullptr;

    ToolWindow*    window[ToolWindow::T_Count] = {};

    Tempest::Timer loadTimer;

    struct {
      ToolArea* area  = nullptr;
      size_t    pos   = size_t(-1);
      bool      merge = false;
      } aboutToDrop;
  };

