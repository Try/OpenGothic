#pragma once

#include <Tempest/Point>

class Interactive;
class Npc;
class Item;

class Focus final {
  public:
    Focus()=default;
    Focus(Interactive& i);
    Focus(Npc& i);
    Focus(Item& i);

    operator bool() const;
    Tempest::Vec3       displayPosition() const;
    const char*         displayName() const;

    Interactive* interactive=nullptr;
    Npc*         npc        =nullptr;
    Item*        item       =nullptr;
  };
