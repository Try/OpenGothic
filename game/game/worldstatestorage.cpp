#include "worldstatestorage.h"

#include <Tempest/MemWriter>
#include <Tempest/MemReader>

#include "gamesession.h"
#include "world/world.h"
#include "serialize.h"

WorldStateStorage::WorldStateStorage(World &w)
  :name(w.name()){
  Tempest::MemWriter wr{storage};
  Serialize          sr{wr};
  w.save(sr);
  }

void WorldStateStorage::save(Serialize &fout) const {
  fout.setEntry("worlds/",name,".zip");
  fout.write(storage);
  }

void WorldStateStorage::load(Serialize& fin) {
  fin.setEntry("worlds/",name,".zip");
  fin.read(storage);
  }
