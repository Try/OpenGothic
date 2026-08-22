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

using MUSIC_TIME=uint32_t;

struct DMUS_IO_TRACK_HEADER final {
  GUID     guidClassID;
  uint32_t dwPosition = 0;
  uint32_t dwGroup    = 0;
  char     ckid[4]    = {};
  char     fccType[4] = {};
  };

enum DMUS_CHORDKEYF_FLAGS : uint8_t {
  DMUS_CHORDKEYF_SILENT = 1
  };

struct DMUS_IO_CHORD final {
  char16_t             wszName[16]={};
  uint32_t             mtTime     =0;
  uint16_t             wMeasure   =0;
  uint8_t              bBeat      =0;
  DMUS_CHORDKEYF_FLAGS bFlags     =DMUS_CHORDKEYF_FLAGS(0);
  };

struct DMUS_IO_SUBCHORD final {
  uint32_t dwChordPattern    = 0;
  uint32_t dwScalePattern    = 0;
  uint32_t dwInversionPoints = 0;
  uint32_t dwLevels          = 0;
  uint8_t  bChordRoot        = 0;
  uint8_t  bScaleRoot        = 0;
  };

enum DMUS_COMMANDT_TYPES : uint8_t {
  DMUS_COMMANDT_GROOVE      = 0,
  DMUS_COMMANDT_FILL        = 1,
  DMUS_COMMANDT_INTRO       = 2,
  DMUS_COMMANDT_BREAK       = 3,
  DMUS_COMMANDT_END         = 4,
  DMUS_COMMANDT_ENDANDINTRO = 5
  };

struct DMUS_IO_COMMAND final {
  uint32_t            mtTime       = 0;
  uint16_t            wMeasure     = 0;
  uint8_t             bBeat        = 0;
  DMUS_COMMANDT_TYPES bCommand     = DMUS_COMMANDT_GROOVE;
  uint8_t             bGrooveLevel = 0;
  uint8_t             bGrooveRange = 0;
  uint8_t             bRepeatMode  = 0;
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

struct DMUS_IO_STYLE final {
  DMUS_IO_TIMESIG timeSig;
  double          dblTempo=0.0;
  };

enum DMUS_EMBELLISHT_TYPES : uint16_t {
  DMUS_EMBELLISHT_NORMAL = 0,
  DMUS_EMBELLISHT_FILL   = 1,
  DMUS_EMBELLISHT_BREAK  = 2,
  DMUS_EMBELLISHT_INTRO  = 4,
  DMUS_EMBELLISHT_END    = 8,
  DMUS_EMBELLISHT_MOTIF  = 16,
  DMUS_EMBELLISHT_ALL    = 0xFFFF
  };


struct DMUS_IO_PATTERN final {
  DMUS_IO_TIMESIG       timeSig;
  uint8_t               bGrooveBottom  = 0;
  uint8_t               bGrooveTop     = 0;
  DMUS_EMBELLISHT_TYPES wEmbellishment = DMUS_EMBELLISHT_NORMAL;
  uint16_t              wNbrMeasures   = 0;
  // uint8_t  bDestGrooveBottom = 0;
  // uint8_t  bDestGrooveTop    = 0;
  // uint32_t dwFlags           = 0;
  };

enum DMUS_IO_INST_FLSGS : uint32_t {
  DMUS_IO_INST_NONE               = 0,
  DMUS_IO_INST_PATCH              = 0x0001,
  DMUS_IO_INST_BANKSELECT         = 0x0002,
  DMUS_IO_INST_ASSIGN_PATCH       = 0x0008,
  DMUS_IO_INST_NOTERANGES         = 0x0010,
  DMUS_IO_INST_PAN                = 0x0020,
  DMUS_IO_INST_VOLUME             = 0x0040,
  DMUS_IO_INST_TRANSPOSE          = 0x0080,
  DMUS_IO_INST_GM                 = 0x0100,
  DMUS_IO_INST_GS                 = 0x0200,
  DMUS_IO_INST_XG                 = 0x0400,
  DMUS_IO_INST_CHANNEL_PRIORITY   = 0x0800,
  DMUS_IO_INST_USE_DEFAULT_GM_SET = 0x1000,
  DMUS_IO_INST_PITCHBENDRANGE     = 0x2000,
  };

struct DMUS_IO_INSTRUMENT final {
  uint32_t           dwPatch         = 0;
  uint32_t           dwAssignPatch   = 0;
  uint32_t           dwNoteRanges[4] = {};
  uint32_t           dwPChannel      = 0;
  DMUS_IO_INST_FLSGS dwFlags         = DMUS_IO_INST_NONE;
  uint8_t            bPan            = 0;
  uint8_t            bVolume         = 0;
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
  DMUS_NOTEF_FLAGS    bNoteFlags     = DMUS_NOTEF_FLAGS(0);
  };

enum Control : uint8_t {
  BankSelect      = 0x00,
  ModWheel        = 0x01,
  BreathCtl       = 0x02,
  FootCtl         = 0x04,
  Portamento      = 0x05,
  DataMsb         = 0x06,
  ChannelVolume   = 0x07,
  Balance         = 0x08,
  Pan             = 0x0A,
  ExpressionCtl   = 0x0B,
  Effect1Ctl      = 0x0C,
  Effect2Ctl      = 0x0D,
  GeneralPurpose1 = 0x10,
  GeneralPurpose2 = 0x11,
  GeneralPurpose3 = 0x12,
  GeneralPurpose4 = 0x13
  };

enum Shape : uint8_t {
  DMUS_CURVES_LINEAR = 0,
  DMUS_CURVES_INSTANT = 1,
  DMUS_CURVES_EXP = 2,
  DMUS_CURVES_LOG = 3,
  DMUS_CURVES_SINE = 4
  };

enum DMUS_CURVE_TYPE : uint8_t {
  DMUS_CURVET_NULL      = 0x0,
  DMUS_CURVET_PBCURVE   = 0x03, // Pitch bend curve.
  DMUS_CURVET_CCCURVE   = 0x04, // Control change curve.
  DMUS_CURVET_MATCURVE  = 0x05,
  DMUS_CURVET_PATCURVE  = 0x06,
  DMUS_CURVET_RPNCURVE  = 0x07,
  DMUS_CURVET_NRPNCURVE = 0x08
  };

struct DMUS_IO_STYLECURVE final {
  uint32_t mtGridStart     = 0;
  uint32_t dwVariation     = 0;
  uint32_t mtDuration      = 0;
  uint32_t mtResetDuration = 0;
  int16_t  nTimeOffset     = 0;
  uint16_t nStartValue     = 0;
  uint16_t nEndValue       = 0;
  uint16_t nResetValue     = 0;
  DMUS_CURVE_TYPE  bEventType = DMUS_CURVET_NULL;
  Shape    bCurveShape     = DMUS_CURVES_LINEAR;
  Control  bCCData         = BankSelect;
  uint8_t  bFlags          = 0;
  uint16_t wParamType      = 0;
  uint16_t wMergeIndex     = 0;
  };

#pragma pack(pop)
}
