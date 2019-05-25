#pragma once

#include <cstdint>

namespace Dx8 {

#pragma pack(push,1)
struct GUID final {
  uint32_t Data1 = 0;
  uint16_t Data2 = 0;
  uint16_t Data3 = 0;
  uint64_t Data4 = 0;

  bool operator == (const GUID& g) const {
    return Data1==g.Data1 &&
           Data2==g.Data2 &&
           Data3==g.Data3 &&
           Data4==g.Data4;
    }
  };

struct DMUS_IO_TRACK_HEADER final {
  GUID     guidClassID;
  uint32_t dwPosition = 0;
  uint32_t dwGroup    = 0;
  char     ckid[4]    = {};
  char     fccType[4] = {};
  };

struct DMUS_IO_REFERENCE final {
  GUID          guidClassID;
  //uint32_t dwValidData = 0;
  };

struct DMUS_IO_VERSION final {
  uint32_t dwVersionMS = 0;
  uint32_t dwVersionLS = 0;
  };

struct DMUS_IO_TIMESIG final {
  uint8_t  bBeatsPerMeasure = 0;
  uint8_t  bBeat            = 0;
  uint16_t wGridsPerBeat    = 0;
  };

struct DMUS_IO_PATTERN final {
  DMUS_IO_TIMESIG timeSig;
  uint8_t         bGrooveBottom  = 0;
  uint8_t         bGrooveTop     = 0;
  uint16_t        wEmbellishment = 0;
  uint16_t        wNbrMeasures   = 0;
  // uint8_t  bDestGrooveBottom = 0;
  // uint8_t  bDestGrooveTop    = 0;
  // uint32_t dwFlags           = 0;
  };

struct DMUS_IO_INSTRUMENT final {
  uint32_t dwPatch           = 0;
  uint32_t dwAssignPatch     = 0;
  uint32_t dwNoteRanges[4]   = {};
  uint32_t dwPChannel        = 0;
  uint32_t dwFlags           = 0;
  uint8_t  bPan              = 0;
  uint8_t  bVolume           = 0;
  //uint16_t nTranspose        = 0;
  //uint32_t dwChannelPriority = 0;
  //uint16_t nPitchBendRange   = 0;
  };

enum DMUS_PLAYMODE_FLAGS : uint8_t {
  DMUS_PLAYMODE_FIXED           = 0,
  DMUS_PLAYMODE_KEY_ROOT        = 1,
  DMUS_PLAYMODE_CHORD_ROOT      = 2,
  DMUS_PLAYMODE_SCALE_INTERVALS = 4,
  DMUS_PLAYMODE_CHORD_INTERVALS = 8,
  DMUS_PLAYMODE_NONE            = 16,
  };

enum DMUS_NOTEF_FLAGS : uint8_t {
  DMUS_NOTEF_NOTEON               = 1,
  DMUS_NOTEF_NOINVALIDATE         = 2,
  DMUS_NOTEF_NOINVALIDATE_INSCALE = 4,
  DMUS_NOTEF_NOINVALIDATE_INCHORD = 8,
  DMUS_NOTEF_REGENERATE           = 16
  };

struct DMUS_IO_STYLEPART final {
  DMUS_IO_TIMESIG     timeSig;
  uint32_t            dwVariationChoices[32]={};
  GUID                guidPartID;
  uint16_t            wNbrMeasures=0;
  DMUS_PLAYMODE_FLAGS bPlayModeFlags=DMUS_PLAYMODE_FIXED;
  // uint8_t             bInvertUpper=0;
  // uint8_t             bInvertLower=0;
  // uint8_t             bPad[3]={};
  // uint32_t            dwFlags=0;
  };

struct DMUS_IO_PARTREF final {
  GUID     guidPartID;
  uint16_t wLogicalPartID=0;
  //uint8_t  bVariationLockID=0;
  //uint8_t  bSubChordLevel=0;
  //uint8_t  bPriority=0;
  //uint8_t  bRandomVariation=0;
  //uint16_t wPad=0;
  //uint32_t dwPChannel=0;
  };

struct DMUS_IO_STYLENOTE final {
  uint32_t            mtGridStart    = 0;
  uint32_t            dwVariation    = 0;
  uint32_t            mtDuration     = 0;
  int16_t             nTimeOffset    = 0;
  uint16_t            wMusicValue    = 0;
  uint8_t             bVelocity      = 0;
  uint8_t             bTimeRange     = 0;
  uint8_t             bVelRange      = 0;
  uint8_t             bInversionID   = 0;
  DMUS_PLAYMODE_FLAGS bPlayModeFlags = DMUS_PLAYMODE_FLAGS(0);
  DMUS_NOTEF_FLAGS    bNoteFlags     = DMUS_NOTEF_FLAGS(0);
  };

struct DMUS_IO_STYLECURVE final {
  uint32_t mtGridStart     = 0;
  uint32_t dwVariation     = 0;
  uint32_t mtDuration      = 0;
  uint32_t mtResetDuration = 0;
  uint16_t nTimeOffset     = 0;
  uint16_t nStartValue     = 0;
  uint16_t nEndValue       = 0;
  uint16_t nResetValue     = 0;
  uint8_t  bEventType      = 0;
  uint8_t  bCurveShape     = 0;
  uint8_t  bCCData         = 0;
  uint8_t  bFlags          = 0;
  uint16_t wParamType      = 0;
  uint16_t wMergeIndex     = 0;
  };

#pragma pack(pop)
}
