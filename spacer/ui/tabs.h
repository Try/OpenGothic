#pragma once

#include <Tempest/Widget>

class EditorArea;

class Tabs : public Tempest::Widget {
  public:
    Tabs(EditorArea& editor);

    void   invalidate();
    void   setSelection(size_t id);
    size_t selection() const { return sel; }

    Tempest::Signal<void(size_t)> onClicked;
    Tempest::Signal<void(size_t)> onClose;

  protected:
    void mouseDownEvent(Tempest::MouseEvent&e) override;
    void paintEvent(Tempest::PaintEvent &e) override;

  private:
    class Tab;
    class Lay;

    EditorArea& editor;
    size_t      sel;
  };

