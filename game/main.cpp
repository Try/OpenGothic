#include <Tempest/Window>
#include <Tempest/Application>
#include <Tempest/Log>

#include <Tempest/VulkanApi>

#include <phoenix/phoenix.hh>

#if defined(_MSC_VER)
#include <Tempest/DirectX12Api>
#endif

#if defined(__OSX__)
#include <Tempest/MetalApi>
#endif

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
  Tempest::ApiFlags flg = g.isDebugMode() ||
                          Gothic::inst().settingsGetI("COMMANDLINE", "vulkanValidationMode")!=0 ?
                          Tempest::ApiFlags::Validation : Tempest::ApiFlags::NoFlags;
  auto gApi             = g.graphicsApi()==CommandLine::DirectX12 ||
                          Gothic::inst().settingsGetI("COMMANDLINE", "dx12")!=0 ?
                          CommandLine::DirectX12 : CommandLine::Vulkan;
  switch(gApi) {
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

    phoenix::logging::use_logger([] (phoenix::logging::level lvl, const std::string& message) {
      switch (lvl) {
        case phoenix::logging::level::error:
          Tempest::Log::e("[phoenix] ", message);
          break;
        case phoenix::logging::level::warn:
          Tempest::Log::e("[phoenix] ", message);
          break;
        case phoenix::logging::level::info:
          Tempest::Log::i("[phoenix] ", message);
          break;
        case phoenix::logging::level::debug:
          Tempest::Log::d("[phoenix] ", message);
          break;
        }
      });
    }
  catch(...) {
    Tempest::Log::e("unable to setup logfile - fallback to console log");
    }
  CrashLog::setup();

  Tempest::Log::i(appBuild);

  CommandLine          cmd{argc,argv};

  Gothic               gothic;

  auto                 api = mkApi(cmd);

  Tempest::Device      device{*api,selectDevice(*api)};
  Resources            resources{device};

  gothic.initialSetup();
  GameMusic            music;
  gothic.setupGlobalScripts();

  MainWindow           wx(device);
  Tempest::Application app;
  return app.exec();
  }
