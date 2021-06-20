#pragma once

#include <Tempest/Point>
#include <string_view>

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
    Tempest::Vec3    displayPosition() const;
    std::string_view displayName() const;

    Interactive* interactive=nullptr;
    Npc*         npc        =nullptr;
    Item*        item       =nullptr;
  };
