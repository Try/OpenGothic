#include "worldstatestorage.h"

#include <Tempest/MemWriter>
#include <Tempest/MemReader>

#include "world/world.h"
#include "serialize.h"

WorldStateStorage::WorldStateStorage(World &w)
  :name(w.name()){
  Tempest::MemWriter wr{storage};
  Serialize          sr{wr};
  w.save(sr);
  }

std::unique_ptr<World> WorldStateStorage::load(GameSession &game, const RendererStorage& rs,
                                               uint8_t ver, std::function<void(int)> loadProgress) {
  Tempest::MemReader rd {storage.data(),storage.size()};
  Serialize          fin{rd};

  auto ret = std::unique_ptr<World>(new World(game,rs,fin,ver,loadProgress));
  // ret->load(fin); //TODO: WorldView ptr
  return ret;
  }
