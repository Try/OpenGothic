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

    const std::string name;
    std::vector<uint8_t> storage;
  };
