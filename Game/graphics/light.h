#pragma once

#include <Tempest/Point>
#include <array>

class Light final {
  public:
    Light();

    std::array<float,3>  dir() const { return ldir; }
    void                 setDir(const std::array<float,3>& d);
    void                 setDir(float x,float y,float z);

    const Tempest::Vec3& color() const { return clr; }
    void                 setColor(const Tempest::Vec3& cl){ clr=cl; }

  private:
    std::array<float,3> ldir;
    Tempest::Vec3       clr;
  };

