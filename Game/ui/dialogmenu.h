#pragma once

#include <Tempest/Texture2d>
#include <Tempest/Widget>

class DialogMenu : public Tempest::Widget {
  public:
    DialogMenu();

    void start();

  protected:
    void paintEvent(Tempest::PaintEvent& e);

  private:
    const Tempest::Texture2d* tex=nullptr;
  };
