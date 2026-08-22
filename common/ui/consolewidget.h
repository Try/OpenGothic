#pragma once

#include <Tempest/Widget>
#include <Tempest/Dialog>
#include <Tempest/Shortcut>

#include "marvin.h"

class MainWindow;

class ConsoleWidget : public Tempest::Widget {
  public:
    ConsoleWidget(const MainWindow& owner);
    ~ConsoleWidget();

    void printLine(std::string_view s);
    void close();
    int  exec();

  protected:
    void paintEvent    (Tempest::PaintEvent& e) override;
    void keyDownEvent  (Tempest::KeyEvent&   e) override;
    void keyRepeatEvent(Tempest::KeyEvent&   e) override;
    using Tempest::Widget::keyUpEvent;

    void updateSizeHint();

  private:
    struct Overlay;

    const MainWindow&         mainWindow;
    Tempest::UiOverlay*       overlay    = nullptr;
    const Tempest::Texture2d* background = nullptr;
    Tempest::Shortcut         closeSk;
    std::vector<std::string>  log, cmdHist;
    std::string               currCmd;
    size_t                    histPos = size_t(-1);
    size_t                    cursPos = 0;
    Marvin                    marvin;
  };

