#include "savegameheader.h"
#include "serialize.h"

SaveGameHeader::SaveGameHeader(Serialize &fin) {
  fin.read(name,priview,world,pcTime,wrldTime,isGothic2);
  }

void SaveGameHeader::save(Serialize &fout) const {
  fout.write(name,priview,world,pcTime,wrldTime,isGothic2);
  }
