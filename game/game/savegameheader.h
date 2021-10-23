#pragma once

#include <Tempest/Pixmap>

#include "gametime.h"

class Serialize;

class SaveGameHeader final {
  public:
    SaveGameHeader() = default;
    SaveGameHeader(Serialize& fin);

    void save(Serialize& fout) const;

    uint16_t        version = 0;
    std::string     name;
    std::string     world;
    gtime           pcTime;
    gtime           wrldTime;
    uint8_t         isGothic2=0;

  private:
    static const char tag[];
  };

