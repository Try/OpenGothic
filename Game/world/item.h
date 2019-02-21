#pragma once

#include <daedalus/DaedalusVM.h>

#include "graphics/staticobjects.h"

class WorldScript;

class Item final {
  public:
    Item(WorldScript& owner,Daedalus::GameState::ItemHandle hnpc);
    Item(Item&&)=default;
    Item& operator=(Item&&)=default;

    enum { MAX_UI_ROWS=6 };

    void setView      (StaticObjects::Mesh&& m);
    void setPosition  (float x,float y,float z);
    void setDirection (float x,float y,float z);

    void setMatrix(const Tempest::Matrix4x4& m);

    bool isEquiped() const    { return equiped; }
    void setAsEquiped(bool e) { equiped=e;      }

    const char*         displayName() const;
    const char*         description() const;
    std::array<float,3> position() const;
    bool                isGold() const;

    const char*         uiText (size_t id) const;
    int32_t             uiValue(size_t id) const;
    size_t              count() const;

    Daedalus::GameState::ItemHandle handle() const { return hitem; }
    size_t                          clsId() const;

  private:
    void updateMatrix();

    WorldScript&                    owner;
    Daedalus::GameState::ItemHandle hitem={};
    StaticObjects::Mesh             view;
    std::array<float,3>             pos={};
    bool                            equiped=false;
  };
