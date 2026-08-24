#include "editorwindow.h"

#include <Tempest/Device>
#include <Tempest/Panel>
#include <Tempest/ComboBox>

#include "ui/rootview.h"

using namespace Tempest;

EditorWindow::EditorWindow(Tempest::Device& device)
  : Window(Maximized), device(device), swapchain(device,hwnd()), texAtlass(device), assets(texAtlass) {
  setWindowTitle("Spacer");
  setLayout(Horizontal);
  addWidget(new RootView()).setFocus(true);
  }

EditorWindow::~EditorWindow() {
  device.waitIdle();
  }

void EditorWindow::render() {
  try {
    static uint64_t time = Application::tickCount();

    cmdId = (cmdId+1)%MaxFramesInFlight;

    auto& sync = fence   [cmdId];
    auto& cmd  = commands[cmdId];
    sync.wait();

    dispatchPaintEvent(uiLayer,texAtlass);
    uiMesh[cmdId].update(device,uiLayer);

    {
      auto enc = cmd.startEncoding(device);
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
