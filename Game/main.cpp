#include <Tempest/VulkanApi>

#include <Tempest/Window>
#include <Tempest/Application>

#include <vector>

#include "utils/crashlog.h"
#include "gothic.h"
#include "mainwindow.h"

const char* selectDevice(const Tempest::VulkanApi& api) {
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

int main(int argc,const char** argv) {
  CrashLog::setup();
#if defined(__WINDOWS__)
  ::ShowWindow(::GetConsoleWindow(), SW_HIDE);
#endif
  VDFS::FileIndex::initVDFS(argv[0]);

  Gothic               gothic{argc,argv};
  Tempest::VulkanApi   api   {gothic.isDebugMode() ? Tempest::ApiFlags::Validation : Tempest::ApiFlags::NoFlags};

  Tempest::Device      device{api,selectDevice(api)};
  Resources            resources{gothic,device};

  MainWindow           wx(gothic,device);
  Tempest::Application app;
  return app.exec();
  }
