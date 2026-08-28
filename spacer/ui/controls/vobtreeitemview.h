#pragma once

#include <Tempest/Widget>

class VobTreeDelegate;

class VobTreeItemView : public Tempest::Widget {
  public:
    VobTreeItemView(VobTreeDelegate& owner, size_t id);
    ~VobTreeItemView();

    void setText(std::string_view txt);
    void setTextAlt(std::string_view txt);
    void setDepth(size_t d);
    void setAsOpen(bool open);
    void setAsGroup(bool g);

    Tempest::Signal<void(Widget*,size_t)> onClick;

    const size_t id = {};

  protected:
    void mouseDownEvent (Tempest::MouseEvent &event) override;
    void mouseUpEvent   (Tempest::MouseEvent &event) override;
    void mouseEnterEvent(Tempest::MouseEvent &event) override;
    void mouseLeaveEvent(Tempest::MouseEvent &event) override;
    void mouseMoveEvent (Tempest::MouseEvent &event) override;

    void paintEvent    (Tempest::PaintEvent &event) override;
    auto text() const -> std::string_view;

  private:
    VobTreeDelegate& owner;
    std::string      txt, txtAlt;
    size_t           depth  = 0;
    bool             closed = true;
    bool             group = false;
  };
