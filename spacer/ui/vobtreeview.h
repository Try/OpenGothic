#pragma once

#include <Tempest/ScrollBar>
#include <Tempest/Widget>

#include <unordered_set>
#include <vector>

#include "objects/worldedit.h"

class VobTreeView : public Tempest::Widget {
  public:
    VobTreeView();

    void setWorld  (WorldEdit& world);
    void setVob    (const WorldEdit::Vob* vob);
    void updateView();

    Tempest::Signal<void(WorldEdit::Vob*)> onVobSelected;

  protected:
    void paintEvent     (Tempest::PaintEvent& e) override;
    void resizeEvent    (Tempest::SizeEvent&  e) override;
    void mouseDownEvent (Tempest::MouseEvent& e) override;
    void mouseUpEvent   (Tempest::MouseEvent& e) override;
    void mouseMoveEvent (Tempest::MouseEvent& e) override;
    void mouseLeaveEvent(Tempest::MouseEvent& e) override;
    void mouseWheelEvent(Tempest::MouseEvent& e) override;

  private:
    struct Row {
      WorldEdit::Vob* vob;
      size_t          depth;

      std::string_view label() const;
      std::string_view fallbackLabel() const;
      };

    void                   appendRows(WorldEdit::Vob& vob, size_t depth);
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

    std::unordered_set<const WorldEdit::Vob*> collapsed;
    std::vector<Row>                          rows;
  };
