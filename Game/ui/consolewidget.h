#pragma once

#include <Tempest/Widget>
#include <Tempest/Dialog>
#include <Tempest/Shortcut>

#include "marvin.h"

class Gothic;

class ConsoleWidget : public Tempest::Widget {
  public:
    ConsoleWidget(Gothic& gothic);
    ~ConsoleWidget();

    void close();
    int  exec();

  protected:
    void paintEvent    (Tempest::PaintEvent& e) override;
    void keyDownEvent  (Tempest::KeyEvent&   e) override;
    void keyRepeatEvent(Tempest::KeyEvent&   e) override;
    using Tempest::Widget::keyUpEvent;

  private:
    struct Overlay;

    Gothic&                   gothic;

    Tempest::UiOverlay*       overlay;
    const Tempest::Texture2d* background;
    Tempest::Shortcut         closeSk;
    std::vector<std::string>  log, cmdHist;
    size_t                    histPos;
    Marvin                    marvin;
  };

