#pragma once

#include <array>

class Interactive;
class Npc;

class Focus final {
  public:
    Focus()=default;
    Focus(Interactive& i);
    Focus(Npc& i);

    operator bool() const;
    std::array<float,3> displayPosition() const;
    const char*         displayName() const;

    Interactive* interactive=nullptr;
    Npc*         npc        =nullptr;
  };
