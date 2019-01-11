#include <Tempest/VulkanApi>

#include <Tempest/Window>
#include <Tempest/Application>

#include <vector>

#include "gothic.h"
#include "mainwindow.h"

int main(int argc,const char** argv) {
  VDFS::FileIndex::initVDFS(argv[0]);

  Tempest::VulkanApi api;
  Gothic             g2(argc,argv);
  MainWindow         wx(g2,api);

  Tempest::Application app;
  return app.exec();
  }
