#pragma once

#include "riff.h"

namespace Dx8 {

class Info final {
  public:
    Info()=default;
    Info(Dx8::Riff &input);

    std::string inam;

  private:
    void implRead(Dx8::Riff &input);
  };

class Unfo final {
  public:
    Unfo()=default;
    Unfo(Dx8::Riff &input);

    std::u16string unam;

  private:
    void implRead(Dx8::Riff &input);
  };

}
