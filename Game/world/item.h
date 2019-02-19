#pragma once

#include <daedalus/DaedalusVM.h>

#include "graphics/staticobjects.h"

class WorldScript;

class Item final {
  public:
    Item(WorldScript& owner,Daedalus::GameState::ItemHandle hnpc);
    Item(Item&&)=default;
    Item& operator=(Item&&)=default;

    void setView      (StaticObjects::Mesh&& m);
    void setPosition  (float x,float y,float z);
    void setDirection (float x,float y,float z);

    void setMatrix(const Tempest::Matrix4x4& m);

  private:
    void updateMatrix();

    Daedalus::GameState::ItemHandle hitem={};
    StaticObjects::Mesh             view;
    std::array<float,3>             pos={};
  };
