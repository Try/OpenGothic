#pragma once

#include <Tempest/Fence>

#include "ui/editors/baseeditor.h"
#include "ui/property/propertylist.h"
#include "ui/dragdrop.h"

#include "graphics/renderer.h"
#include "camera.h"

class WorldEdit;

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

    void moveDropOver (DropOverEvent& ev) override;
    void dropDone     (DropOverEvent& ev) override;

    void paintEvent(Tempest::PaintEvent& e) override;
    void resizeEvent(Tempest::SizeEvent& e) override;

  private:
    void load(std::string_view wname);
    void update3d(Tempest::Encoder<Tempest::CommandBuffer>& cmd, uint8_t cmdId);

    std::vector<PropertyList::Prop> props;

    Camera                     camera;
    std::unique_ptr<WorldEdit> level;

    Tempest::Fence         fence   [Resources::MaxFramesInFlight];
    Tempest::CommandBuffer commands[Resources::MaxFramesInFlight];
    uint8_t                cmdId = 0;

    Tempest::Attachment    sceneImage;
    Renderer               renderer;
  };