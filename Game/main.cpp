
#include <Tempest/Window>
#include <Tempest/Application>

#include <Tempest/VulkanApi>

#if defined(_MSC_VER)
#include <Tempest/DirectX12Api>
#endif

#include "utils/crashlog.h"
#include "gothic.h"
#include "mainwindow.h"

const char* selectDevice(const Tempest::AbstractGraphicsApi& api) {
  auto d = api.devices();

  static Tempest::Device::Props p;
  if(0) {
    p = d[1];
    return p.name;
    }

  for(auto& i:d)
    if(i.type==Tempest::AbstractGraphicsApi::Discrete) {
      p = i;
      return p.name;
      }
  if(d.size()>0) {
    p = d[0];
    return p.name;
    }
  return nullptr;
  }

std::unique_ptr<Tempest::AbstractGraphicsApi> mkApi(Gothic& g) {
  Tempest::ApiFlags flg = g.isDebugMode() ? Tempest::ApiFlags::Validation : Tempest::ApiFlags::NoFlags;
  switch(g.graphicsApi()) {
    case Gothic::DirectX12:
#if defined(_MSC_VER)
      return std::make_unique<Tempest::DirectX12Api>(flg);
#else
      break;
#endif
    case Gothic::Vulkan:
      return std::make_unique<Tempest::VulkanApi>(flg);
    }
  return std::make_unique<Tempest::VulkanApi>(flg);
  }

int main(int argc,const char** argv) {
  CrashLog::setup();
  VDFS::FileIndex::initVDFS(argv[0]);

  Gothic               gothic{argc,argv};
  auto                 api = mkApi(gothic);

  Tempest::Device      device{*api,selectDevice(*api),Resources::MaxFramesInFlight};
  Resources            resources{gothic,device};
  GameMusic            music(gothic);

  MainWindow           wx(gothic,device);
  Tempest::Application app;
  return app.exec();
  }
