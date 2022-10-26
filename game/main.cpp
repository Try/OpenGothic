#include <Tempest/Window>
#include <Tempest/Application>
#include <Tempest/Log>

#include <Tempest/VulkanApi>

#if defined(_MSC_VER)
#include <Tempest/DirectX12Api>
#endif

#if defined(__OSX__)
#include <Tempest/MetalApi>
#endif

// CI testing
#include <filesystem>
extern "C" void f() {
  std::filesystem::path a, b;
  bool v = a!=b;
  (void)v;
  }

#include "utils/crashlog.h"
#include "mainwindow.h"
#include "gothic.h"
#include "build.h"
#include "commandline.h"

const char* selectDevice(const Tempest::AbstractGraphicsApi& api) {
  auto d = api.devices();

  static Tempest::Device::Props p;
  for(auto& i:d)
    //if(i.type==Tempest::DeviceType::Integrated) {
    if(i.type==Tempest::DeviceType::Discrete) {
      p = i;
      return p.name;
      }
  if(d.size()>0) {
    p = d[0];
    return p.name;
    }
  return nullptr;
  }

std::unique_ptr<Tempest::AbstractGraphicsApi> mkApi(const CommandLine& g) {
  Tempest::ApiFlags flg = g.isDebugMode() ? Tempest::ApiFlags::Validation : Tempest::ApiFlags::NoFlags;
  switch(g.graphicsApi()) {
    case CommandLine::DirectX12:
#if defined(_MSC_VER)
      return std::make_unique<Tempest::DirectX12Api>(flg);
#else
      break;
#endif
    case CommandLine::Vulkan:
#if !defined(__OSX__)
      return std::make_unique<Tempest::VulkanApi>(flg);
#else
      break;
#endif
    }

#if defined(__OSX__)
  return std::make_unique<Tempest::MetalApi>(flg);
#else
  return std::make_unique<Tempest::VulkanApi>(flg);
#endif
  }

int main(int argc,const char** argv) {
  try {
    static Tempest::WFile logFile("log.txt");
    Tempest::Log::setOutputCallback([](Tempest::Log::Mode mode, const char* text) {
      logFile.write(text,std::strlen(text));
      logFile.write("\n",1);
      logFile.flush();
      });
    }
  catch(...) {
    Tempest::Log::e("unable to setup logfile - fallback to console log");
    }
  CrashLog::setup();
  VDFS::FileIndex::initVDFS(argv[0]);

  Tempest::Log::i(appBuild);

  CommandLine          cmd{argc,argv};
  auto                 api = mkApi(cmd);

  Tempest::Device      device{*api,selectDevice(*api)};
  Resources            resources{device};

  Gothic               gothic;
  GameMusic            music;
  gothic.setupGlobalScripts();

  MainWindow           wx(device);
  Tempest::Application app;
  return app.exec();
  }
