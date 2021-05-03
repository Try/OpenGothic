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

WorldStateStorage::WorldStateStorage(Serialize &fin)
  :wname(fin.read<std::string>()){
  fin.read(storage);
  }

void WorldStateStorage::save(Serialize &fout) const {
  fout.write(wname);
  fout.write(storage);
  }
