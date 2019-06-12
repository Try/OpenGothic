#include <Tempest/VulkanApi>

#include <Tempest/Window>
#include <Tempest/Application>

#include <vector>

#include "gothic.h"
#include "mainwindow.h"

int main(int argc,const char** argv) {
  VDFS::FileIndex::initVDFS(argv[0]);

  Gothic             g2(argc,argv);
  Tempest::VulkanApi api(g2.isDebugMode() ? Tempest::ApiFlags::Validation : Tempest::ApiFlags::NoFlags);
  MainWindow         wx(g2,api);

  Tempest::Application app;
  return app.exec();
  }
