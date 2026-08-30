#pragma once

#include <Tempest/Fence>

#include "ui/editors/baseeditor.h"
#include "ui/dragdrop.h"

#include "graphics/renderer.h"
#include "objects/worldedit.h"
#include "utils/keycodec.h"
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

    void keyDownEvent(Tempest::KeyEvent& e) override;
    void keyUpEvent(Tempest::KeyEvent& e) override;

    void mouseDownEvent(Tempest::MouseEvent& e) override;
    void mouseDragEvent(Tempest::MouseEvent& e) override;

    void moveDropOver (DropOverEvent& ev) override;
    void dropDone     (DropOverEvent& ev) override;

    void paintEvent(Tempest::PaintEvent& e) override;
    void resizeEvent(Tempest::SizeEvent& e) override;

  private:
    void load(std::string_view wname);
    void update3d(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t cmdId);
    void processKeyboard(Tempest::KeyEvent& e);
    void tickCamera(uint64_t dt);

    auto rayQuery(Tempest::Point mpos) -> WorldEdit::Vob*;
    void selectVob(const WorldEdit::Vob& vob);

    Camera                     camera;
    std::unique_ptr<WorldEdit> level;

    Tempest::Fence         fence   [Resources::MaxFramesInFlight];
    Tempest::CommandBuffer commands[Resources::MaxFramesInFlight];
    uint8_t                cmdId = 0;

    Tempest::Attachment    sceneImage;
    Renderer               renderer;

    bool                   ctrl[KeyCodec::Last] = {};
    Tempest::Point         mpos = {};

    VobTreeDelegate*       treeDelegate = nullptr;
    PropertyDelegate*      propertyDelegate = nullptr;
  };