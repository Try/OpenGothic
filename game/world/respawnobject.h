#pragma once

#include <string>
#include <vector>

class GameScript;
class Serialize;

class RespawnObject final {
  public:
    RespawnObject() = default;
    RespawnObject(Serialize& fin);
    void save(Serialize& fout) const;

    static void saveRespawnState(Serialize &fout);
    static void loadRespawnState(Serialize &fin);

    static void registerObject(size_t inst, std::string wp, uint32_t guild);
    static void processRespawnList();

    static bool handleCommand(std::string_view cmd);

  private:
    size_t inst;         // NPC monster instance
    std::string wp;      // waypoint
    uint32_t respawnDay; // Day of respawn
    std::string world;   // World name

    static bool npcShouldRespawn(uint32_t guild);

    // Print output methods
    static void printMsg(std::string msg);
    static void printMsg(std::string msg, int y);
    static void printMsg(std::string msg, int y, bool forcePrint);

    static std::vector<RespawnObject> respawnList;
    static uint32_t minDays;
    static uint32_t maxDays;
};