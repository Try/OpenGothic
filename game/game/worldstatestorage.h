#pragma once

#include <Tempest/ODevice>

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

class World;
class GameSession;
class RendererStorage;
class Serialize;

class WorldStateStorage final {
  public:
    WorldStateStorage()=default;
    WorldStateStorage(World &w);
    WorldStateStorage(Serialize &fin);
    WorldStateStorage(const WorldStateStorage&)=delete;
    WorldStateStorage(WorldStateStorage&&)=default;
    WorldStateStorage& operator = (WorldStateStorage&&)=default;

    bool                 isEmpty() const { return storage.empty(); }
    const std::string&   name()    const { return wname; }
    void                 save(Serialize& fout) const;

    std::vector<uint8_t> storage;

  private:
    std::string          wname;
  };
