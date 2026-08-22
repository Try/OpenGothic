#pragma once

#include "riff.h"
#include "structs.h"

namespace Dx8 {

struct Reference final {
  Reference()=default;
  Reference(Riff &chunk);

  DMUS_IO_REFERENCE header;
  GUID              guid;
  std::u16string    name, file, category;
  DMUS_IO_VERSION   version;
  };

}
