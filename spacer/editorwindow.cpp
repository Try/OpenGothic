#include "editorwindow.h"

#include <Tempest/Device>

#include <Tempest/Panel>
#include <Tempest/ComboBox>

using namespace Tempest;

EditorWindow::EditorWindow(Tempest::Device& device)
  : Window(Maximized), device(device), swapchain(device,hwnd()), texAtlass(device) {
  resetSwapchain();
  setupUi();
  }

EditorWindow::~EditorWindow() {
  device.waitIdle();
  }

void EditorWindow::setupUi() {
  auto& p = addWidget(new Panel());
  p.setDragable(true);

  auto& cb0 = p.addWidget(new ComboBox());
  cb0.setItems({"Item0", "Item1"});

  auto& cb1 = p.addWidget(new ComboBox());
  cb1.setItems({"Item0", "Item1", "Item2"});

  p.addWidget(new Widget());
  p.setLayout(Vertical);
  }

void EditorWindow::render() {
  try {
    static uint64_t time = Application::tickCount();

    cmdId = (cmdId+1)%MaxFramesInFlight;

    auto&       sync = fence   [cmdId];
    auto&       cmd  = commands[cmdId];
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
    resetSwapchain();
    update();
    }
  }

void EditorWindow::resetSwapchain() {
  swapchain.reset();
  }
