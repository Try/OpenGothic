#pragma once

#include "item.h"

class ItemTorchBurning : public Item {
  public:
    ItemTorchBurning(World& owner, size_t inst, Item::Type type);

    void      clearView() override;
    bool      isTorchBurn() const override;

  protected:
    void      moveEvent() override;

  private:
    ObjVisual view;
  };

