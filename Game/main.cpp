#include <Tempest/VulkanApi>

#include <Tempest/Window>
#include <Tempest/Application>

#include <vector>

#include "utils/crashlog.h"
#include "gothic.h"
#include "mainwindow.h"

int main(int argc,const char** argv) {
  CrashLog::setup();
  VDFS::FileIndex::initVDFS(argv[0]);

  Gothic               gothic{argc,argv};
  Tempest::VulkanApi   api   {gothic.isDebugMode() ? Tempest::ApiFlags::Validation : Tempest::ApiFlags::NoFlags};
  Tempest::Device      device{api};

  Resources            resources{gothic,device};

  MainWindow           wx(gothic,device);
  Tempest::Application app;
  return app.exec();
  }
