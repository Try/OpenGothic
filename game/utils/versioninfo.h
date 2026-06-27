#pragma once

#include <cstdint>

class VersionInfo final {
  public:
    uint8_t game =2;
    int32_t patch=0;

    bool     hasZSStateLoop()     const { return game==2 && patch>=5; }
    // NOTE: in original-game oCNpc::StartDialogAni @0x00757de0 the talk-gesture index is rolled as
    // rand()%20+1 (a FIXED range, not the count of existing T_DIALOGGESTURE_* anims); indices that
    // don't resolve (12..20 in vanilla G2) play no gesture, so the original gestures on only ~55% of
    // dialog lines. OpenGothic used 11 (the exact resolvable count), so every roll resolved and NPCs
    // gestured on 100% of lines. Use the original's fixed roll range.
    uint16_t dialogGestureCount() const { return game==2 ? 20 : 21;   }
  };

