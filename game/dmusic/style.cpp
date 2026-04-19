#include "style.h"
#include "mixer.h"

#include <algorithm>
#include <stdexcept>
#include <Tempest/Log>

using namespace Tempest;
using namespace Dx8;

Style::Part::Part(Riff &input) {
  input.read([this](Riff& c){
    if(c.is("prth"))
      c.read(&header,sizeof(header));
    else if(c.is("note")) {
      #pragma pack(push,1)
      struct LegacyStyleNoteV1 final {
        MUSIC_TIME          mtGridStart    = 0;
        uint32_t            dwVariation    = 0;
        MUSIC_TIME          mtDuration     = 0;
        int16_t             nTimeOffset    = 0;
        uint16_t            wMusicValue    = 0;
        uint8_t             bVelocity      = 0;
        uint8_t             bTimeRange     = 0;
        uint8_t             bDurRange      = 0;
        uint8_t             bVelRange      = 0;
        uint8_t             bInversionID   = 0;
        DMUS_PLAYMODE_FLAGS bPlayModeFlags = DMUS_PLAYMODE_FLAGS(0);
        };
      #pragma pack(pop)
      static_assert(sizeof(LegacyStyleNoteV1)==22, "Unexpected legacy style note layout");

      uint32_t elemSize = 0;
      c.read(&elemSize,sizeof(elemSize));
      if(elemSize==0)
        return;

      const size_t cnt = c.remaning()/elemSize;
      notes.resize(cnt);
      for(size_t i=0;i<cnt;++i) {
        auto& out = notes[i];
        if(elemSize==sizeof(LegacyStyleNoteV1)) {
          LegacyStyleNoteV1 in = {};
          c.read(&in,sizeof(in));
          out.mtGridStart    = in.mtGridStart;
          out.dwVariation    = in.dwVariation;
          out.mtDuration     = in.mtDuration;
          out.nTimeOffset    = in.nTimeOffset;
          out.wMusicValue    = in.wMusicValue;
          out.bVelocity      = in.bVelocity;
          out.bTimeRange     = in.bTimeRange;
          out.bDurRange      = in.bDurRange;
          out.bVelRange      = in.bVelRange;
          out.bInversionID   = in.bInversionID;
          out.bPlayModeFlags = in.bPlayModeFlags;
          out.bNoteFlags     = DMUS_NOTEF_FLAGS(0);
          }
        else {
          const size_t readSz = std::min<size_t>(elemSize,sizeof(out));
          c.read(&out,readSz);
          if(elemSize>readSz)
            c.skip(elemSize-readSz);
          }
        }
      }
    else if(c.is("crve"))
      c.readAll(curves);
    });
  }

Style::Style(Riff &input) {
  if(!input.is("RIFF"))
    throw std::runtime_error("not a riff");
  input.readListId("DMST");
  input.read([this](Riff& c){
    implRead(c);
    });
  }

void Style::implRead(Riff &input) {
  if(input.is("RIFF")){
    if(input.isListId("DMBD"))
      band.emplace_back(input);
    }
  else if(input.is("guid")){
    input.read(&guid,sizeof(guid));
    }
  else if(input.is("styh")){
    input.read(&styh,sizeof(styh));
    }
  else if(input.is("LIST")){
    if(input.isListId("pttn"))
      patterns.emplace_back(input);
    else if(input.isListId("part"))
      parts.emplace_back(Part(input));
    else if(input.isListId("prrf")) {
      input.read([this](Riff& c){
        if(c.is("LIST") && c.isListId("DMRF"))
          chordMaps.emplace_back(c);
        });
      }
    }
  }

const Style::Part* Style::findPart(const GUID &partGuid) const {
  for(auto& i:parts)
    if(i.header.guidPartID==partGuid)
      return &i;
  return nullptr;
  }
