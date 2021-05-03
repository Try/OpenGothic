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

    struct StyleTrack {
      StyleTrack(Riff &chunk);
      std::vector<StyleRef> styles;
      };

    class Chord {
      public:
        Chord(Riff &chunk);

        uint32_t                      header=0;
        DMUS_IO_CHORD                 ioChord;
        std::vector<DMUS_IO_SUBCHORD> subchord;
      private:
        void implReadList(Riff &input);
      };

    class CommandTrack {
      public:
        CommandTrack(Riff &chunk);

        std::vector<DMUS_IO_COMMAND> commands;
      };

    DMUS_IO_TRACK_HEADER          head;
    std::shared_ptr<StyleTrack>   sttr;
    std::shared_ptr<Chord>        cord;
    std::shared_ptr<CommandTrack> cmnd;

  private:
    void implReadList(Riff &input);
  };

}
