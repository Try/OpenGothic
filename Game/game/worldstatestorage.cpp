#include "worldstatestorage.h"

#include <Tempest/MemWriter>
#include <Tempest/MemReader>

#include "gamesession.h"
#include "world/world.h"
#include "serialize.h"

WorldStateStorage::WorldStateStorage(World &w)
  :wname(w.name()){
  Tempest::MemWriter wr{storage};
  Serialize          sr{wr};
  w.save(sr);
  }
