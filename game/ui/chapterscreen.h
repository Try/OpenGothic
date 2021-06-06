#pragma once

#include <Tempest/Widget>
#include <Tempest/Texture2d>
#include <Tempest/Timer>

class ChapterScreen : public Tempest::Widget {
  public:
    ChapterScreen();

    struct Show {
      std::string title;
      std::string subtitle;
      std::string img;
      std::string sound;
      int         time=0;
      };

    void show(const Show& image);
    bool isActive() const { return active; }

    void keyDownEvent(Tempest::KeyEvent &e);
    void keyUpEvent  (Tempest::KeyEvent &e);

  protected:
    void paintEvent(Tempest::PaintEvent& e);

  private:
    const Tempest::Texture2d* back=nullptr;
    bool                      active=false;
    Tempest::Timer            timer;

    std::string               title, subTitle;

    void                      close();
  };
