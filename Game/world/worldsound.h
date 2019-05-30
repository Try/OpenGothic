#pragma once

#include "soundfx.h"

#include <Tempest/SoundDevice>
#include <Tempest/SoundEffect>
#include <zenload/zTypes.h>
#include <mutex>

class GameSession;
class World;
class Npc;
class GSoundEffect;

class WorldSound final {
  public:
    WorldSound(GameSession& game,World& world);

    void seDefaultZone(const ZenLoad::zCVobData &vob);
    void addZone(const ZenLoad::zCVobData &vob);

    void emitSound(const char *s, float x, float y, float z, float range, GSoundEffect *slot);
    void emitDlgSound(const char *s, float x, float y, float z, float range);
    void takeSoundSlot(GSoundEffect &&eff);
    void aiOutput(const std::array<float,3> &pos, const std::string& outputname);

    void tick(Npc& player);
    void tickSlot(GSoundEffect &slot);
    bool isInListenerRange(const std::array<float,3> &pos) const;

    static const float talkRange;

  private:
    struct Zone final {
      ZMath::float3 bbox[2]={};
      std::string   name;
      };

    static float qDist(const std::array<float,3>& a,const std::array<float,3>& b);

    GameSession&      game;
    World&            owner;
    std::vector<Zone> zones;
    Zone              def;

    std::array<float,3>                     plPos;

    Tempest::SoundDevice                    dev;
    std::vector<GSoundEffect>               effect;

    std::mutex                              sync;

    static const float maxDist;
  };
