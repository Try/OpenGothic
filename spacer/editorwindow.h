#pragma once

#include <Tempest/Window>
#include <Tempest/Swapchain>
#include <Tempest/VectorImage>
#include <Tempest/Fence>
#include <Tempest/CommandBuffer>

class EditorWindow : public Tempest::Window {
  public:
    explicit EditorWindow(Tempest::Device& device);
    ~EditorWindow() override;

    enum {
      MaxFramesInFlight = 2
      };

  protected:
    void render() override;

  private:
    Tempest::Device&           device;
    Tempest::Swapchain         swapchain;
    Tempest::TextureAtlas      texAtlass;

    Tempest::VectorImage       uiLayer;
    Tempest::VectorImage::Mesh uiMesh[MaxFramesInFlight];

    Tempest::Fence             fence   [MaxFramesInFlight];
    Tempest::CommandBuffer     commands[MaxFramesInFlight];
    uint8_t                    cmdId = 0;
  };
