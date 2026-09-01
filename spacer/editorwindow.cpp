#include "editorwindow.h"

#include <Tempest/Device>
#include <Tempest/Panel>
#include <Tempest/ComboBox>

#include "ui/rootview.h"

using namespace Tempest;

Tempest::Signal<void(Tempest::Encoder<Tempest::CommandBuffer>&,uint8_t)> EditorWindow::onUpdate3D;

EditorWindow::EditorWindow(Tempest::Device& device)
  : Window(Maximized), device(device), swapchain(device,hwnd()), texAtlass(device), assets(device,texAtlass) {
  setWindowTitle("Spacer");
  setLayout(Horizontal);
  addWidget(new RootView()).setFocus(true);
  }

EditorWindow::~EditorWindow() {
  device.waitIdle();
  }

void EditorWindow::render() {
  if(!needToUpdate())
    return;

  try {
    //static uint64_t time = Application::tickCount();

    cmdId = (cmdId+1)%MaxFramesInFlight;

    auto& sync = fence   [cmdId];
    auto& cmd  = commands[cmdId];
    sync.wait();
    Resources::resetRecycled(cmdId);

    {
      auto enc = cmd.startEncoding(device);

      onUpdate3D(enc, cmdId);

      dispatchPaintEvent(uiLayer,texAtlass);
      uiMesh[cmdId].update(device,uiLayer);

      enc.setFramebuffer({{swapchain[swapchain.currentImage()],Vec4(0),Tempest::Preserve}});
      enc.setViewport(0,0,w(),h());
      uiMesh[cmdId].draw(enc);
    }

    sync = device.submit(cmd);
    device.present(swapchain);
    }
  catch(const Tempest::SwapchainSuboptimal&) {
    swapchain.reset();
    update();
    }
  }
