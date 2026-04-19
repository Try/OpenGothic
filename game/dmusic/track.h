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

      uint32_t  stmp=0;
      Reference reference;
      };

    struct StyleTrack {
      StyleTrack(Riff &chunk);
      std::vector<StyleRef> styles;
      };

    struct ChordMapRef {
      ChordMapRef(Riff &chunk);

      uint32_t  stmp=0;
      Reference reference;
      };

    struct ChordMapTrack {
      ChordMapTrack(Riff &chunk);
      std::vector<ChordMapRef> chordMaps;
      };

    class Chord {
      public:
        struct Event final {
          uint32_t                      header=0;
          DMUS_IO_CHORD                 ioChord;
          std::vector<DMUS_IO_SUBCHORD> subchord;
          };

        Chord(Riff &chunk);

        uint32_t                      header=0;
        DMUS_IO_CHORD                 ioChord;
        std::vector<DMUS_IO_SUBCHORD> subchord;
        std::vector<Event>            events;
      private:
        void implReadList(Riff &input);
      };

    class CommandTrack {
      public:
        CommandTrack(Riff &chunk);

        std::vector<DMUS_IO_COMMAND> commands;
      };

    class TempoTrack {
      public:
        struct Event final {
          uint32_t time  = 0;
          double   tempo = 0.0;
          };

        TempoTrack(Riff &chunk);

        std::vector<Event> events;
      };

    DMUS_IO_TRACK_HEADER          head;
    std::shared_ptr<StyleTrack>   sttr;
    std::shared_ptr<ChordMapTrack> pftr;
    std::shared_ptr<Chord>        cord;
    std::shared_ptr<CommandTrack> cmnd;
    std::shared_ptr<TempoTrack>   tetr;

  private:
    void implReadList(Riff &input);
  };

}
