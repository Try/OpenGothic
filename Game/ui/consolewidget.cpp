#include "consolewidget.h"

#include <Tempest/Painter>
#include <Tempest/UiOverlay>
#include <Tempest/Application>

#include "utils/gthfont.h"
#include "resources.h"

using namespace Tempest;

struct ConsoleWidget::Overlay : public Tempest::UiOverlay {
  ConsoleWidget& owner;

  Overlay(ConsoleWidget& owner):owner(owner){}

  void mouseDownEvent(MouseEvent& e) override {
    e.accept();
    }

  void mouseMoveEvent(MouseEvent& e) override {
    e.accept();
    }

  void mouseWheelEvent(MouseEvent& e) override {
    e.accept();
    }

  void paintEvent(PaintEvent&) override {
    }

  void keyDownEvent(Tempest::KeyEvent& e) override {
    owner.keyDownEvent(e);
    e.accept();
    }

  void keyRepeatEvent(Tempest::KeyEvent& e) override {
    owner.keyRepeatEvent(e);
    e.accept();
    }

  void keyUpEvent(Tempest::KeyEvent& e) override {
    owner.keyUpEvent(e);
    e.accept();
    }

  void closeEvent(Tempest::CloseEvent& e) override {
    e.ignore();
    }
  };

ConsoleWidget::ConsoleWidget(Gothic& gothic)
  :gothic(gothic) {
  setSizeHint(1024,256);
  setMargins(Margin(8,8,8,8));
  setSizePolicy(Fixed);

  log.emplace_back("OpenGothic v1.0.dev");
  log.emplace_back("");

  closeSk = Shortcut(*this,Event::M_NoModifier,Event::K_F2);
  closeSk.onActivated.bind(this,&ConsoleWidget::close);

  background = Resources::loadTexture("CONSOLE.TGA");
  }

ConsoleWidget::~ConsoleWidget() {
  close();
  }

void ConsoleWidget::close() {
  if(overlay==nullptr)
    return;
  Tempest::UiOverlay* ov = overlay;
  overlay = nullptr;

  setVisible(false);
  CloseEvent e;
  this->closeEvent(e);

  ov->takeWidget(this);
  delete ov;
  log.back().clear();
  }

int ConsoleWidget::exec() {
  if(overlay==nullptr){
    overlay = new Overlay(*this);

    SystemApi::addOverlay(std::move(overlay));
    overlay->setLayout(Vertical);
    overlay->addWidget(this);
    overlay->addWidget(new Widget());
    }

  setVisible(true);
  while(overlay && Application::isRunning()) {
    Application::processEvents();
    }
  return 0;
  }

void ConsoleWidget::paintEvent(PaintEvent& e) {
  Painter p(e);
  if(background!=nullptr)
    p.setBrush(*background);
  p.drawRect(0,0,w(),h(),
             0,0,p.brush().w(),p.brush().h());

  auto& fnt = Resources::font();
  int   y   = h() - margins().bottom;
  for(size_t i=log.size(); i>0;) {
    --i;
    fnt.drawText(p, margins().left, y, log[i]);
    y-=fnt.pixelSize();
    if(i+1==log.size()) {
      int x = margins().left + fnt.textSize(log[i]).w;
      float a = float(Application::tickCount()%2000)/2000.f;
      p.setBrush(Color(1,1,1,a));
      p.drawRect(x,y,1,fnt.pixelSize());
      update();
      }
    if(y<0)
      break;
    }
  }

void ConsoleWidget::keyDownEvent(KeyEvent& e) {
  if(Event::K_F1<=e.key && e.key<=Event::K_F12) {
    e.ignore();
    return;
    }

  if(e.key==Event::K_ESCAPE) {
    close();
    }

  if(e.key==Event::K_Up) {
    histPos++;
    if(histPos<cmdHist.size())
      log.back() = cmdHist[cmdHist.size()-1-histPos]; else
      histPos    = size_t(cmdHist.size()-1);
    return;
    }
  if(e.key==Event::K_Down) {
    if(histPos==size_t(-1))
      return;
    histPos--;
    if(histPos<cmdHist.size())
      log.back() = cmdHist[cmdHist.size()-1-histPos]; else
      log.back() = "";
    return;
    }

  if(e.key==Event::K_C && (e.modifier&Event::M_Command)==Event::M_Command) {
    log.emplace_back("");
    return;
    }

  if(e.key==Event::K_Back) {
    if(log.back().size()>0)
      log.back().pop_back();
    return;
    }
  if(e.key==Event::K_Tab) {
    if(log.back().size()>0)
      marvin.autoComplete(log.back());
    return;
    }
  if(e.key==Event::K_Return) {
    if(log.back().size()>0) {
      cmdHist.emplace_back(log.back());
      if(!marvin.exec(gothic,log.back()))
        log.back() = "Unknown command : " + log.back();
      log.emplace_back("");
      }
    return;
    }

  char ch = '\0';
  if(('a'<=e.code && e.code<='z') || ('A'<=e.code && e.code<='Z')  ||
     ('0'<=e.code && e.code<='9') || e.code==' ' || e.code=='.')
    ch = char(e.code);
  if(ch=='\0')
    return;
  log.back().push_back(ch);
  histPos = size_t(-1);
  }

void ConsoleWidget::keyRepeatEvent(KeyEvent& e) {
  keyDownEvent(e);
  }


