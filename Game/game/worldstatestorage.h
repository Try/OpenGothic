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
    WorldStateStorage(World &w);

    std::unique_ptr<World> load(GameSession &game, const RendererStorage& rs, uint8_t ver, std::function<void(int)> loadProgress);

    const std::string name;

  private:
    std::vector<uint8_t> storage;
  };
