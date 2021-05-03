#pragma once

#include <Tempest/Pixmap>

#include "gametime.h"

class Serialize;

class SaveGameHeader final {
  public:
    SaveGameHeader() = default;
    SaveGameHeader(Serialize& fin);

    void save(Serialize& fout) const;

    std::string     name;
    Tempest::Pixmap priview;
    std::string     world;
    gtime           pcTime;
    gtime           wrldTime;
    uint8_t         isGothic2=0;
  };

