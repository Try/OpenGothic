#pragma once

#include <Tempest/ListDelegate>

#include "objects/worldedit.h"

class VobTreeDelegate : public Tempest::ListDelegate {
  public:
    VobTreeDelegate(WorldEdit& world);

    void             setVob(const WorldEdit::Vob* vob);

    size_t           size() const override;
    Tempest::Widget* createView(size_t position) override;
    void             removeView(Tempest::Widget* w, size_t /*position*/) override;

    Tempest::Signal<void(const WorldEdit::Vob&)> onVobSelected;

  private:
    struct Item {
      const WorldEdit::Vob* vob = {};
      size_t item  = 0;
      size_t depth = 0;

      std::string_view textAlt() const;
      };

    void             emitClick(Tempest::Widget* w, size_t id);
    void             refreshFileTree();
    void             toogleFolder(const WorldEdit::Vob& itm);
    void             mkIndex();
    void             mkIndex(const WorldEdit::Vob& v, std::vector<Item>& index, size_t depth);

    bool             isOpen(const WorldEdit::Vob*) const;
    bool             isSelected(size_t id) const;

    WorldEdit& world;
    std::set<const WorldEdit::Vob*> closedDir;
    std::vector<Item>               index;

    const WorldEdit::Vob*           vob = nullptr;

  friend class VobTreeItemView;
  };

