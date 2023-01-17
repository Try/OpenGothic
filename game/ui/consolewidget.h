#pragma once

#include <Tempest/Widget>
#include <Tempest/Dialog>
#include <Tempest/Shortcut>

#include "marvin.h"

class ConsoleWidget : public Tempest::Widget {
  public:
    ConsoleWidget();
    ~ConsoleWidget();

    void close();
    int  exec();

  protected:
    void paintEvent    (Tempest::PaintEvent& e) override;
    void keyDownEvent  (Tempest::KeyEvent&   e) override;
    void keyRepeatEvent(Tempest::KeyEvent&   e) override;
    using Tempest::Widget::keyUpEvent;

    void printLine(std::string_view s);

  private:
    struct Overlay;

    Tempest::UiOverlay*       overlay    = nullptr;
    const Tempest::Texture2d* background = nullptr;
    Tempest::Shortcut         closeSk;
    std::vector<std::string>  log, cmdHist;
    std::string               currCmd;
    size_t                    histPos = size_t(-1);
    size_t                    cursPos = 0;
    Marvin                    marvin;
  };

