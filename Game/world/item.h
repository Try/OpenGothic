#pragma once

#include <daedalus/DaedalusVM.h>

class Item final {
  public:
    Item(Daedalus::GameState::ItemHandle hnpc);

  private:
    Daedalus::GameState::ItemHandle hitem={};
  };
