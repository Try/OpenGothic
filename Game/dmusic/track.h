#pragma once

#include <memory>
#include <vector>

#include "reference.h"
#include "riff.h"
#include "structs.h"

namespace Dx8 {

class Track final {
  public:
    Track(Riff& input);

    struct StyleRef  {
      StyleRef(Riff &chunk);

      uint16_t  stmp=0;
      Reference reference;
      };

    class StyleTrack {
      public:
        StyleTrack(Riff &chunk);
        std::vector<StyleRef> styles;
      };

    DMUS_IO_TRACK_HEADER        head;
    std::shared_ptr<StyleTrack> sttr;

  private:
    void implReadList(Riff &input);
  };

}
