#pragma once

#include "soundfx.h"

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
class GSoundEffect;

class WorldSound final {
  public:
    WorldSound(Gothic &gothic, GameSession& game,World& world);

    void setDefaultZone(const ZenLoad::zCVobData &vob);
    void addZone       (const ZenLoad::zCVobData &vob);
    void addSound      (const ZenLoad::zCVobData &vob);

    void emitSound   (const char *s, float x, float y, float z, float range, bool freeSlot);
    void emitSound3d (const char *s, float x, float y, float z, float range);
    void emitSoundRaw(const char *s, float x, float y, float z, float range, bool freeSlot);
    void emitDlgSound(const char *s, float x, float y, float z, float range, uint64_t &timeLen);
    void takeSoundSlot(GSoundEffect &&eff);
    void aiOutput(const Tempest::Vec3& pos, const std::string& outputname);

    void tick(Npc& player);
    void tickSlot(GSoundEffect &slot);
    bool isInListenerRange(const Tempest::Vec3& pos, float sndRgn) const;

    static const float talkRange;

  private:
    struct Zone final {
      ZMath::float3 bbox[2]={};
      std::string   name;
      bool          checkPos(float x,float y,float z) const;
      };

    struct WSound final {
      WSound(SoundFx&& s):proto(std::move(s)){}
      SoundFx      proto;
      GSoundEffect eff;

      bool         loop          =false;
      bool         active        =false;
      uint64_t     delay         =0;
      uint64_t     delayVar      =0;
      uint64_t     restartTimeout=0;


      gtime        sndStart;
      gtime        sndEnd;
      GSoundEffect eff2;
      };

    void tickSoundZone(Npc& player);
    bool setMusic(const char* zone, GameMusic::Tags tags);

    Gothic&                                 gothic;
    GameSession&                            game;
    World&                                  owner;
    std::vector<Zone>                       zones;
    Zone                                    def;

    uint64_t                                nextSoundUpdate=0;
    Zone*                                   currentZone = nullptr;
    GameMusic::Tags                         currentTags = GameMusic::Tags::Std;

    Tempest::Vec3                           plPos;

    std::unordered_map<std::string,GSoundEffect> freeSlot;
    std::vector<GSoundEffect>               effect;
    std::vector<GSoundEffect>               effect3d; // snd_play3d
    std::vector<WSound>                     worldEff;

    std::mutex                              sync;

    static const float maxDist;
  };
