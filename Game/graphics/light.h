#pragma once

#include <Tempest/Point>
#include <array>

class Light final {
  public:
    Light();

    Tempest::Vec3        dir() const { return ldir; }
    void                 setDir(const Tempest::Vec3& d);
    void                 setDir(float x,float y,float z);

    const Tempest::Vec3& color() const { return clr; }
    void                 setColor(const Tempest::Vec3& cl){ clr=cl; }

    void                 setPosition(const Tempest::Vec3& p) { pos = p; }
    const Tempest::Vec3& position() const { return pos; }

    float                range() const { return rgn; }
    void                 setRange(float r) { rgn = r; }

  private:
    Tempest::Vec3  ldir;
    Tempest::Vec3  clr;
    Tempest::Vec3  pos;
    float          rgn = 0;
  };

