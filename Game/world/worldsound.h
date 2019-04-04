#pragma once

#include <zenload/zTypes.h>

class GameSession;
class World;
class Npc;

class WorldSound final {
  public:
    WorldSound(GameSession& game,World& world);

    void seDefaultZone(const ZenLoad::zCVobData &vob);
    void addZone(const ZenLoad::zCVobData &vob);

    void tick(Npc& player);

  private:
    struct Zone final {
      ZMath::float3 bbox[2]={};
      std::string   name;
      };

    GameSession&      game;
    World&            owner;
    std::vector<Zone> zones;
    Zone              def;

    std::array<float,3> plPos;
  };
