#pragma once

#include <Tempest/SoundDevice>
#include <Tempest/SoundEffect>
#include <zenload/zTypes.h>
#include <mutex>

class GameSession;
class World;
class Npc;

class WorldSound final {
  public:
    WorldSound(GameSession& game,World& world);

    void seDefaultZone(const ZenLoad::zCVobData &vob);
    void addZone(const ZenLoad::zCVobData &vob);

    void emitSound(const char *s, float x, float y, float z, float range);
    void aiOutput(const std::array<float,3> &pos, const std::string& outputname);

    void tick(Npc& player);

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

    std::array<float,3>               plPos;
    std::vector<Tempest::SoundEffect> effect;
    Tempest::SoundDevice              dev;

    std::mutex                        sync;

    static const float maxDist;
  };
