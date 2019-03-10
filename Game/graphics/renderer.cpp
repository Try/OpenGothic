#include "renderer.h"

#include <Tempest/Color>

#include "graphics/submesh/staticmesh.h"
#include "camera.h"
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
  if(auto wview=gothic.worldView())
    wview->initPipeline(w,h);
  }

void Renderer::onWorldChanged() {
  if(auto wview=gothic.worldView()){
    if(zbuffer.w()>0 && zbuffer.h()>0)
      wview->initPipeline(uint32_t(zbuffer.w()),uint32_t(zbuffer.h()));
    }
  }

void Renderer::setCameraView(const Camera& camera) {
  view = camera.view();
  }

bool Renderer::needToUpdateCmd() {
  if(auto wview=gothic.worldView()) {
    return wview->needToUpdateCmd();
    }
  return false;
  }

void Renderer::updateCmd() {
  if(auto wview=gothic.worldView()) {
    wview->updateCmd(*gothic.world());
    }
  }

void Renderer::draw(CommandBuffer &cmd, uint32_t imgId, const Gothic &gothic) {
  FrameBuffer& fbo = fbo3d[imgId];

  if(auto wview=gothic.worldView()) {
    wview->updateCmd(*gothic.world());

    wview->updateUbo(view,device.frameId());
    wview->draw(cmd,fbo);
    } else {
    cmd.setPass(fbo,storage().pass());
    }
  }
