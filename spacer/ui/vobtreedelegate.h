#pragma once

#include <Tempest/ListDelegate>

#include "ui/objects/worldedit.h"

class VobTreeDelegate : public Tempest::ListDelegate {
  public:
    VobTreeDelegate(WorldEdit& world);

    size_t           size() const override;
    Tempest::Widget* createView(size_t position) override;
    void             removeView(Tempest::Widget* w, size_t /*position*/) override;

  private:
    struct Item {
      const WorldEdit::Vob* vob = {};
      size_t item  = 0;
      size_t depth = 0;
      };

    void             emitClick(Tempest::Widget* w, size_t id);
    void             refreshFileTree();
    void             toogleFolder(const WorldEdit::Vob& itm);
    void             mkIndex();
    void             mkIndex(const WorldEdit::Vob& v, std::vector<Item>& index, size_t depth);

    WorldEdit& world;
    std::set<const WorldEdit::Vob*> closedDir;
    std::vector<Item>               index;
  };

