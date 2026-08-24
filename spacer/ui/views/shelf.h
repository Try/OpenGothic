#pragma once

#include <Tempest/ListView>
#include <Tempest/Widget>

#include "project/projectitem.h"

class Shelf : public Tempest::Widget {
  public:
    Shelf();

    Tempest::Signal<void(const ProjectItem&)> onItemSelected;

  private:
    struct Item;
    struct Central;
    struct Box;
    struct Row;
    struct PathItem;
    struct Delegate;
    struct PathDelegate;

    void onItem(Widget*, const ProjectItem& it);
    void resizeEvent(Tempest::SizeEvent&) override;
    void mouseDownEvent(Tempest::MouseEvent &e) override;
    void mouseUpEvent(Tempest::MouseEvent &e) override;
    void mouseWheelEvent(Tempest::MouseEvent &event) override;
    void onScroll(int v);
    void setCategory(size_t id);
    void setSubdir(size_t id);

    size_t findRootItem() const;

    Tempest::ListView*  category = nullptr;
    Tempest::ListView*  path     = nullptr;
    Widget*             box      = nullptr;
    Central*            cen      = nullptr;
    Tempest::ScrollBar* scroll   = nullptr;

    Tempest::Button*    addItm   = nullptr;

    std::vector<ProjectItem> subdir;

    size_t              categoryId = -1;
    bool                showByType = false;
  };

