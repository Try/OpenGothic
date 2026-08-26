#pragma once

#include <Tempest/Window>
#include <Tempest/Swapchain>
#include <Tempest/VectorImage>
#include <Tempest/Fence>
#include <Tempest/CommandBuffer>

#include "graphics/shaders.h"

#include "assets.h"

class EditorWindow : public Tempest::Window {
  public:
    explicit EditorWindow(Tempest::Device& device);
    ~EditorWindow() override;

    static Tempest::Signal<void(Tempest::Encoder<Tempest::CommandBuffer>&,uint8_t)> onUpdate3D;

    enum {
      MaxFramesInFlight = 2
      };

  protected:
    void render() override;

  private:
    Tempest::Device&           device;
    Tempest::Swapchain         swapchain;
    Tempest::TextureAtlas      texAtlass;
    Assets                     assets;
    Shaders                    shaders;

    Tempest::VectorImage       uiLayer;
    Tempest::VectorImage::Mesh uiMesh[MaxFramesInFlight];

    Tempest::Fence             fence   [MaxFramesInFlight];
    Tempest::CommandBuffer     commands[MaxFramesInFlight];
    uint8_t                    cmdId = 0;
  };
