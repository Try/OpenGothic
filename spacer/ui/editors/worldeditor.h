#pragma once

#include <Tempest/Fence>

#include "ui/editors/baseeditor.h"
#include "ui/dragdrop.h"

#include "graphics/renderer.h"
#include "objects/worldedit.h"
#include "utils/keycodec.h"
#include "command.h"
#include "camera.h"

class WorldEdit;
class PropertyDelegate;
class VobTreeDelegate;

class WorldEditor: public BaseEditor,
                   public DropReciver  {
  public:
    WorldEditor();
    ~WorldEditor() override;

  protected:
    std::string_view title() const override;
    BaseTool*        createToolpanel(ToolWindow::Tool tool) override;

    void undo() override;
    void redo() override;
    bool hasUnsavedChanges() const override;

    void keyDownEvent(Tempest::KeyEvent& e) override;
    void keyUpEvent  (Tempest::KeyEvent& e) override;

    void mouseDownEvent(Tempest::MouseEvent& e) override;
    void mouseUpEvent  (Tempest::MouseEvent& e) override;
    void mouseDragEvent(Tempest::MouseEvent& e) override;

    void moveDropOver(DropOverEvent& ev) override;
    void dropDone    (DropOverEvent& ev) override;

    void paintEvent(Tempest::PaintEvent& e) override;
    void resizeEvent(Tempest::SizeEvent& e) override;

  private:
    struct Gizmo;

    enum class State : uint32_t {
      T_Idle  = 0,
      T_WASD  = 1,
      T_DragX = 2,
      T_DragY = 3,
      T_DragZ = 4,
      };

    void load(std::string_view wname);
    void update3d(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t cmdId);
    void processKeyboard(Tempest::KeyEvent& e);
    void tickCamera(uint64_t dt);
    void tick();

    int  gizmoQuery(Tempest::Point mpos) const;
    auto rayQuery(Tempest::Point mpos) -> WorldEdit::Vob*;
    void dragVob(Tempest::Point mpos, const WorldEdit::Vob& vob, State st);
    void selectVob(WorldEdit::Vob* vob);
    bool setVobPosition(const WorldEdit::Vob* selVob, WorldEdit::Vob& root, Tempest::Vec3 pos);
    void updateGizmo();

    Tempest::Timer             timer;
    Camera                     camera;
    std::unique_ptr<WorldEdit> level;
    State                      state = State::T_Idle;

    Tempest::Fence         fence   [Resources::MaxFramesInFlight];
    Tempest::CommandBuffer commands[Resources::MaxFramesInFlight];
    uint8_t                cmdId = 0;

    Tempest::Attachment    sceneImage;
    Renderer               renderer;

    bool                   ctrl[KeyCodec::Last] = {};
    Tempest::Point         mpos = {};

    Command::UndoStack<WorldEdit> timeline;

    WorldEdit::Vob*        selVob = nullptr;
    VobTreeDelegate*       treeDelegate = nullptr;
    PropertyDelegate*      propertyDelegate = nullptr;

    MeshObjects::Mesh      selectedVobBevel;
  };