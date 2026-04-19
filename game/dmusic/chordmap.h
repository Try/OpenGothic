#pragma once

#include "riff.h"
#include "structs.h"

#include <vector>
#include <string>

namespace Dx8 {

class ChordMap final {
  public:
    ChordMap()=default;
    ChordMap(Riff& riff);

    bool resolve(const std::u16string& chordName, std::vector<DMUS_IO_SUBCHORD>& out) const;

  private:
    struct Chord final {
      std::u16string         name;
      std::vector<uint16_t>  subchordIds;
      };

    struct ChordEntry final {
      DMUS_IO_CHORDENTRY io;
      Chord              chord;
      };

    void implRead(Riff& input);
    void readChord(Riff& input, Chord& out);
    bool resolve(const Chord& chord, std::vector<DMUS_IO_SUBCHORD>& out) const;

    DMUS_IO_CHORDMAP                     header;
    std::vector<DMUS_IO_CHORDMAP_SUBCHORD> subchords;
    std::vector<Chord>                   chordPalette;
    std::vector<ChordEntry>              chordEntries;
  };

}
