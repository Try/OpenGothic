#pragma once

#include <Tempest/SoundDevice>
#include <Tempest/SoundEffect>
#include <Tempest/Point>

#include <zenload/zTypes.h>
#include <mutex>

#include "game/gametime.h"
#include "gamemusic.h"

class GameSession;
class World;
class Npc;
class Sound;
class SoundFx;

class WorldSound final {
  public:
    WorldSound(Gothic &gothic, GameSession& game,World& world);
    ~WorldSound();

    void    setDefaultZone(const ZenLoad::zCVobData &vob);
    void    addZone       (const ZenLoad::zCVobData &vob);
    void    addSound      (const ZenLoad::zCVobData &vob);

    Sound   addSound   (const char *s, float x, float y, float z, float range, bool freeSlot);
    Sound   addSound3d (const char *s, float x, float y, float z, float range);
    Sound   addSoundRaw(const char *s, float x, float y, float z, float range, bool freeSlot);
    Sound   addDlgSound(const char *s, float x, float y, float z, float range, uint64_t &timeLen);

    void    aiOutput(const Tempest::Vec3& pos, const std::string& outputname);

    void    tick(Npc& player);
    bool    isInListenerRange(const Tempest::Vec3& pos, float sndRgn) const;

    static const float talkRange;

  private:
    struct WSound;
    struct Zone;

    struct Effect {
      Tempest::SoundEffect eff;
      Tempest::Vec3        pos;
      float                vol    = 1.f;
      float                occ    = 1.f;
      bool                 loop   = false;
      bool                 active = true;

      void setOcclusion(float occ);
      void setVolume(float v);
      };

    using PEffect = std::shared_ptr<Effect>;

    void    tickSoundZone(Npc& player);
    void    tickSlot(std::vector<PEffect>& eff);
    void    tickSlot(Effect& slot);
    void    initSlot(Effect& slot);
    bool    setMusic(const char* zone, GameMusic::Tags tags);
    Sound   implAddSound(Tempest::SoundEffect&& s, float x, float y, float z, float rangeRef, float rangeMax);

    Gothic&                                 gothic;
    GameSession&                            game;
    World&                                  owner;

    std::vector<Zone>                       zones;
    std::unique_ptr<Zone>                   def;

    uint64_t                                nextSoundUpdate=0;
    Zone*                                   currentZone = nullptr;
    GameMusic::Tags                         currentTags = GameMusic::Tags::Std;

    Tempest::Vec3                           plPos;

    std::unordered_map<std::string,PEffect> freeSlot;
    std::vector<PEffect>                    effect;
    std::vector<PEffect>                    effect3d; // snd_play3d
    std::vector<WSound>                     worldEff;

    std::mutex                              sync;

    static const float maxDist;

  friend class Sound;
  };
