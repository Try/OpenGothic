#pragma once

#include <Tempest/ODevice>

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

class World;
class GameSession;
class RendererStorage;

class WorldStateStorage final {
  public:
    WorldStateStorage()=default;
    WorldStateStorage(World &w);
    WorldStateStorage(const WorldStateStorage&)=delete;
    WorldStateStorage(WorldStateStorage&&)=default;
    WorldStateStorage& operator = (WorldStateStorage&&)=default;

    bool                 isEmpty() const { return storage.empty(); }
    const std::string&   name()    const { return wname; }
    std::vector<uint8_t> storage;

  private:
    std::string          wname;
  };
