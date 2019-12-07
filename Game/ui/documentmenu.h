#pragma once

#include <Tempest/Widget>
#include <Tempest/Texture2d>
#include <Tempest/Timer>

class Gothic;

class DocumentMenu : public Tempest::Widget {
  public:
    DocumentMenu(Gothic& gothic);

    enum Flags : uint8_t {
      F_None,
      F_Margin=1,
      F_Backgr=2,
      F_Font  =4
      };

    struct Page {
      std::string     img;
      std::string     text;
      std::string     font;
      Tempest::Margin margins;
      Flags           flg=F_None;
      };

    struct Show {
      std::vector<Page> pages;
      std::string       font;
      Tempest::Margin   margins;
      std::string       img;
      };

    void show(const Show& doc);
    bool isActive() const { return active; }
    void close();
    void tick(uint64_t dt);

    void keyDownEvent(Tempest::KeyEvent &e);
    void keyUpEvent  (Tempest::KeyEvent &e);

  protected:
    void paintEvent(Tempest::PaintEvent& e);

  private:
    Gothic&            gothic;
    DocumentMenu::Show document;
    bool               active=false;
  };

