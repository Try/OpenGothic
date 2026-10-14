#pragma once

#include <Tempest/Point>

#include <cstdint>

class Npc;
class World;

// gothic-diablo: the "director" of cinematic dialogs (like Mass Effect).
// A wide shot of the place to open the talk; then shot / reverse shot over the listener's shoulder, close-ups and
// medium shots of the one who speaks, a reaction of the listener in the middle of long lines; a fast exchange of short
// lines stays in one shot of both; while the player picks an answer: over the hero's shoulder on the other one.
// The camera stays on one side of the line between the two (180 degree rule), every take moves a little in its own way
// (push-in, slide, arc, rise), the listener never covers the face of the one who speaks and walls pull the camera in.
class ArpgDialogCam final {
  public:
    struct Input {
      const Npc*    pl       = nullptr;
      const Npc*    other    = nullptr;
      const Npc*    speaker  = nullptr; // who says the current line (nullptr: nobody)
      uint32_t      line     = 0;       // changes with every new line
      float         lineLen  = 0;       // seconds
      bool          choosing = false;   // the player picks an answer
      const Npc*    third    = nullptr; // a third one who speaks in the talk
      Tempest::Vec3 viewFrom = {};      // where the camera was before the talk (picks the side)
      };

    struct Shot {
      Tempest::Vec3 origin = {};
      Tempest::Vec3 target = {};
      float         focus  = 0;         // cm from the camera to the face in focus
      };

    void reset();
    Shot tick(float dt, const Input& in, World& world);

  private:
    enum Kind : uint8_t {
      K_None,
      K_Wide,         // the place: both, far, a bit from above (opens the talk)
      K_Two,          // both, from the side
      K_OverShoulder, // the subject, over the shoulder of the other
      K_Medium,       // the subject from the waist up, 3/4 from the front
      K_Close,        // the subject's face
      };
    enum Move : uint8_t {
      M_PushIn,
      M_Slide,
      M_Arc,
      M_Rise,
      };

    struct Take {
      Kind       kind      = K_None;
      const Npc* subject   = nullptr;
      Move       move      = M_PushIn;
      float      extraSide = 0;   // cm further to the side: the listener does not cover the face
      float      t         = 0;   // seconds since the cut
      };

    Take          take;
    uint32_t      line        = 0;
    float         lineT       = 0;
    float         lineLen     = 0;
    float         prevLineLen = 0;
    const Npc*    lineSpeaker = nullptr;
    const Npc*    pendingSpeaker = nullptr; // a line came during the opening shot
    uint32_t      cuts        = 0;
    bool          reacting    = false;
    bool          reactDone   = false;
    bool          fastTalk    = false;    // short lines back and forth: one shot of both
    Kind          beforeReact = K_None;
    float         side        = 0;        // +1 / -1: the side of the line the camera stays on
    bool          faceInit    = false;
    Tempest::Vec3 facePl      = {};       // smoothed head positions
    Tempest::Vec3 faceOther   = {};
    Tempest::Vec3 faceThird   = {};
    const Npc*    thirdSeen   = nullptr;
    Shot          last;

    void  cut      (Kind k, const Npc* who, const Input& in, World& world);
    Shot  base     (Kind k, const Npc* who, const Input& in) const;
    void  pair     (const Npc* who, const Input& in, Tempest::Vec3& x, Tempest::Vec3& y, Tempest::Vec3& n) const;
    Shot  film     (const Take& tk, const Input& in, World& world, float* quality) const;
    void  pickSide (const Input& in, World& world);
    bool  showsFace(const Npc* who) const;
    void  cutToSpeaker(const Input& in, World& world, bool newSpeaker);
  };
