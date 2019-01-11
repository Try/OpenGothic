#include "resources.h"

#include <Tempest/MemReader>
#include <Tempest/Pixmap>
#include <Tempest/Device>

#include "gothic.h"

using namespace Tempest;

Resources* Resources::inst=nullptr;

Resources::Resources(Gothic &gothic, Tempest::Device &device)
  : device(device), asset("data",device),gothic(gothic) {
  inst=this;

  menuFnt = Font("data/font/menu.ttf");
  menuFnt.setPixelSize(48);

  mainFnt = Font("data/font/main.ttf");
  mainFnt.setPixelSize(28);

  //__PHYSFS_platformEnumerate();
  addVdf("Data/Textures.vdf");
  addVdf("Data/Textures_Addon_Menu_English.vdf");

  gothicAssets.finalizeLoad();
  }

Resources::~Resources() {
  inst=nullptr;
  }

void Resources::addVdf(const char *vdf) {
  gothicAssets.loadVDF(gothic.path() + vdf);
  }

Tempest::Texture2d Resources::implLoadTexture(const std::string& name) {
  std::vector<uint8_t> data;
  if(!gothicAssets.getFileData(name,data))
    return Texture2d();

  try {
    Tempest::MemReader rd(data.data(),data.size());
    Tempest::Pixmap    pm(rd);

    return device.loadTexture(pm);
    }
  catch(...){
    //TODO: log
    return Texture2d();
    }
  }

Tempest::Texture2d Resources::loadTexture(const char *name) {
  return inst->implLoadTexture(name);
  }

Tempest::Texture2d Resources::loadTexture(const std::string &name) {
  return inst->implLoadTexture(name);
  }
