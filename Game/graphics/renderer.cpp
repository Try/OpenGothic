#include "renderer.h"

#include <Tempest/Color>

#include "graphics/submesh/staticmesh.h"
#include "gothic.h"

using namespace Tempest;

Renderer::Renderer(Tempest::Device &device,Gothic& gothic)
  :device(device),gothic(gothic),stor(device) {
  view.identity();
  }

void Renderer::initSwapchain(uint32_t w,uint32_t h) {
  if(zbuffer.w()==int(w) && zbuffer.h()==int(h))
    return;

  const size_t imgC=device.swapchainImageCount();
  fbo3d.clear();

  zbuffer  = device.createTexture(TextureFormat::Depth16,w,h,false);
  mainPass = device.pass(Color(0.0),1.f,zbuffer.format());

  for(size_t i=0;i<imgC;++i) {
    Tempest::Frame frame=device.frame(i);
    fbo3d.emplace_back(device.frameBuffer(frame,zbuffer,mainPass));
    }

  stor.initPipeline(mainPass,w,h);
  if(auto wview=gothic.world().view())
    wview->initPipeline(w,h);
  }

void Renderer::onWorldChanged() {
  auto wview=gothic.world().view();
  if(wview!=nullptr && zbuffer.w()>0 && zbuffer.h()>0)
    wview->initPipeline(uint32_t(zbuffer.w()),uint32_t(zbuffer.h()));
  }

void Renderer::setDebugView(const std::array<float,3>& cam, const PointF &spin, const float zoom) {
  this->cam =cam;
  this->spin=spin;
  this->zoom=zoom;
  }

void Renderer::draw(CommandBuffer &cmd, uint32_t imgId, const Gothic &gothic) {
  FrameBuffer& fbo = fbo3d[imgId];

  view.identity();
  view.translate(0,0,0.4f);
  view.rotate(spin.y, 1, 0, 0);
  view.rotate(spin.x, 0, 1, 0);
  view.scale(zoom);
  view.translate(cam[0],1000,cam[1]);
  view.scale(-1,-1,-1);

  if(auto wview=gothic.world().view()) {
    wview->updateCmd(gothic.world());

    wview->updateUbo(view,device.frameId());
    wview->draw(cmd,fbo);
    } else {
    cmd.setPass(fbo,storage().pass());
    }
  }
