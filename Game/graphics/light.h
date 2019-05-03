#pragma once

#include <array>

class Light final {
  public:
    Light();

    std::array<float,3> dir() const { return ldir; }
    void                setDir(const std::array<float,3>& d);
    void                setDir(float x,float y,float z);

  private:
    std::array<float,3> ldir;
  };

