#include "assets.h"

#include <Tempest/Application>

using namespace Tempest;

const std::byte close_png[] = {
  // #embed "close.png"
  };

Assets::Assets() {
  fntSmall = Application::font();
  fntApp   = Application::font();
  }

const Assets& Assets::inst() {
  static Assets assets;
  return assets;
  }
