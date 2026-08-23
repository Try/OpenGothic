#include "assets.h"

#include <Tempest/Application>
#include <Tempest/MemReader>

#include "assetfile.h"

using namespace Tempest;

static Assets* instance = nullptr;

Assets::Assets(TextureAtlas& atlas) {
  instance = this;

  fntSmall = Application::font();
  fntApp   = Application::font();

  ic.close      = Icon(AssetFile::pixmap("close.png"), atlas);
  ic.close_save = Icon(AssetFile::pixmap("close_save.png"), atlas);
  ic.more       = Icon(AssetFile::pixmap("more.png"), atlas);

  ic.add        = Icon(AssetFile::pixmap("add.png"), atlas);

  ic.down       = Icon(AssetFile::pixmap("down.png"), atlas);
  ic.up         = Icon(AssetFile::pixmap("up.png"), atlas);

  ic.tri_close      = Icon(AssetFile::pixmap("tri_close.png"), atlas);
  ic.tri_open       = Icon(AssetFile::pixmap("tri_open.png"), atlas);
  ic.tri_open_small = Icon(AssetFile::pixmap("tri_open_small.png"), atlas);

  ic.check_off      = Icon(AssetFile::pixmap("check_off.png"), atlas);
  ic.check_on       = Icon(AssetFile::pixmap("check_on.png"), atlas);

  ic.file_project   = Icon(AssetFile::pixmap("file_project.png"), atlas);
  ic.file_large     = Icon(AssetFile::pixmap("file_large.png"), atlas);
  ic.folder_large   = Icon(AssetFile::pixmap("folder_large.png"), atlas);
  }

Assets::~Assets() {
  instance = nullptr;
  }

const Assets& Assets::inst() {
  return *instance;
  }
