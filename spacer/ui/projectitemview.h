#pragma once

#include <Tempest/Widget>

#include "dragdrop.h"
#include "project/projectitem.h"

class ProjectItemView : public Tempest::Widget {
  public:
    ProjectItemView(const ProjectItem& id);
    ~ProjectItemView();

    void setText(std::string_view txt);
    void setDepth(size_t d);
    void setAsOpen(bool open);

    Tempest::Signal<void(Widget*, const ProjectItem&)> onClick;

    const ProjectItem it = {};

  protected:
    void mouseDownEvent (Tempest::MouseEvent &event) override;
    void mouseDragEvent (Tempest::MouseEvent &event) override;
    void mouseUpEvent   (Tempest::MouseEvent &event) override;
    void mouseEnterEvent(Tempest::MouseEvent &event) override;
    void mouseLeaveEvent(Tempest::MouseEvent &event) override;
    void mouseMoveEvent (Tempest::MouseEvent &event) override;

    void paintEvent    (Tempest::PaintEvent &event) override;
    auto text() const -> std::string_view;
    bool isDrag() const;

  private:
    std::string txt;
    DragDrop    dd;
    size_t      depth=0;
    bool        closed=true;
  };

