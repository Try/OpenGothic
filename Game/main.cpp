#include <Tempest/VulkanApi>

#include <Tempest/Window>
#include <Tempest/Application>

#include <vector>

#include "gothic.h"
#include "mainwindow.h"

int main(int argc,const char** argv) {
  VDFS::FileIndex::initVDFS(argv[0]);

  Tempest::VulkanApi   api(Tempest::ApiFlags::NoFlags);
  //Tempest::VulkanApi api(Tempest::ApiFlags::Validation);
  Tempest::SoundDevice sound;

  Gothic             g2(argc,argv);
  MainWindow         wx(g2,api,sound);

  Tempest::Application app;
  return app.exec();
  }
