#pragma once

#include <Tempest/ScrollBar>
#include <Tempest/Widget>

#include <string>
#include <unordered_set>
#include <vector>

#include "objects/worldedit.h"

class VobTreeView : public Tempest::Widget {
  public:
    VobTreeView();

    void setWorld  (WorldEdit& world);
    void setVob    (const WorldEdit::Vob* vob);
    void setFilter (const Tempest::TextModel& text);
    void updateView();
    void revealSelection();

    bool hasClassFilters() const;
    bool isClassFiltered(zenkit::VirtualObjectType type) const;
    void toggleClassFilter(zenkit::VirtualObjectType type);
    void clearClassFilters();

    Tempest::Signal<void(WorldEdit::Vob*)> onVobSelected;
    Tempest::Signal<void()>                onVobDoubleClicked;

  protected:
    void paintEvent           (Tempest::PaintEvent& e) override;
    void resizeEvent          (Tempest::SizeEvent&  e) override;
    void mouseDownEvent       (Tempest::MouseEvent& e) override;
    void mouseDoubleClickEvent(Tempest::MouseEvent& e) override;
    void mouseUpEvent         (Tempest::MouseEvent& e) override;
    void mouseMoveEvent       (Tempest::MouseEvent& e) override;
    void mouseLeaveEvent      (Tempest::MouseEvent& e) override;
    void mouseWheelEvent      (Tempest::MouseEvent& e) override;

  private:
    struct Row {
      WorldEdit::Vob* vob;
      size_t          depth;
      bool            hasVisibleChildren;

      std::string_view label() const;
      std::string_view vobName() const;
      };

    void                   appendRows(WorldEdit::Vob& vob, size_t depth);
    void                   appendFilteredRows(WorldEdit::Vob& vob, size_t depth);
    static bool            matchesFilter(std::string_view row, std::string_view pattern);
    bool                   matchesClassFilter(zenkit::VirtualObjectType type) const;
    bool                   isFiltering() const;
    void                   updateFilter();
    bool                   isCollapsed(const WorldEdit::Vob* vob) const;
    bool                   openSelectionPath(const WorldEdit::Vob& node);
    void                   paintRow(Tempest::Painter& p, const Row& row, const Tempest::Rect& rect, bool hovered) const;
    void                   updateScrollBars();
    void                   scrollChanged(int value);
    int                    rowHeight() const;
    Tempest::Rect          viewport() const;
    const Row*             rowAt(Tempest::Point point) const;
    bool                   isArrowHit(const Row& row, Tempest::Point point) const;
    const Tempest::Sprite&  arrowSprite(const Row& row) const;
    Tempest::Rect          arrowRect(const Row& row, int y, const Tempest::Sprite& arrow) const;

    Tempest::ScrollBar&   verticalBar;
    Tempest::ScrollBar&   horizontalBar;
    int                   treeWidth                = 0;
    int                   verticalWheelRemainder   = 0;
    int                   horizontalWheelRemainder = 0;
    WorldEdit*            world                    = nullptr;
    const WorldEdit::Vob* selected                 = nullptr;
    const Row*            pressed                  = nullptr;
    bool                  pressedArrow             = false;
    Tempest::Point        mouse                    = {-1, -1};

    std::string                               filter;
    std::unordered_set<zenkit::VirtualObjectType> classFilters;
    std::unordered_set<const WorldEdit::Vob*> collapsed;
    std::unordered_set<const WorldEdit::Vob*> filteredCollapsed;
    std::vector<Row>                          rows;
  };
