#pragma once

#include <Tempest/Menu>
#include <zenkit/vobs/VirtualObject.hh>

namespace Tempest {
class CheckBox;
}

class VobTreeView;

class FilterMenu final : public Tempest::Menu {
  public:
    explicit FilterMenu(VobTreeView& tree);
    void show(Tempest::Widget& anchor);

  private:
    class Popup;
    class MenuStyle;
    class ClassListDelegate;

    Tempest::Widget* createItems(const std::vector<Item>& items) override;
    void clearFilters();
    void updateFilters(bool);

    VobTreeView& tree;
    Tempest::Button* clearButton = nullptr;
    std::vector<std::pair<zenkit::VirtualObjectType, Tempest::CheckBox*>> checkboxes;
  };
