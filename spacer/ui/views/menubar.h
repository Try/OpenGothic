#pragma once

#include <Tempest/Shortcut>
#include <Tempest/Widget>

class MenuBar : public Tempest::Widget {
  public:
    MenuBar();

  protected:
    void    paintEvent(Tempest::PaintEvent &event) override;

  private:
    struct Menu;

    void    onFileMenu();
    void    onEditMenu();
    void    onExtraMenu();

    void    onNewItem();
    void    onOpenProj();

    void    onSave();
    void    onUndo();
    void    onRedo();

    Tempest::Button* file = nullptr;
    Tempest::Button* edit = nullptr;
    Tempest::Button* more = nullptr;
  };

