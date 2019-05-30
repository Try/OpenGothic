#include "chapterscreen.h"

#include <Tempest/Painter>
#include <Tempest/SoundEffect>

#include "gothic.h"
#include "resources.h"

using namespace Tempest;

ChapterScreen::ChapterScreen(Gothic& gothic)
  :gothic(gothic){
  }

void ChapterScreen::show(const Show& s) {
  back = Resources::loadTexture(s.img);
  if(!active)
    gothic.pushPause();
  active   = true;
  title    = s.title;
  subTitle = s.subtitle;
  if(s.time>0)
    timer.start(uint64_t(s.time));
  timer.timeout.bind(this,&ChapterScreen::close);
  gothic.emitGlobalSoundWav(s.sound);
  update();
  }

void ChapterScreen::paintEvent(Tempest::PaintEvent &e) {
  if(!active || !back)
    return;

  Painter p(e);
  int x = (w()-back->w())/2;
  int y = (h()-back->h())/2;
  p.setBrush(*back);
  p.drawRect(x,y,back->w(),back->h());

  Color clNormal = {1.f,0.87f,0.67f,1.f};
  p.setFont(Resources::menuFont());
  auto sz = p.font().textSize(title);
  p.setBrush(clNormal);
  p.drawText(x+(back->w()-sz.w)/2,y+50+int(p.font().pixelSize()),title);

  p.setFont(Resources::dialogFont());
  p.setBrush(Color(1.f));
  sz = p.font().textSize(subTitle);
  p.drawText(x+(back->w()-sz.w)/2,y+back->h()-50,subTitle);
  }

void ChapterScreen::close() {
  if(!active)
    return;
  gothic.popPause();
  active=false;
  }

void ChapterScreen::keyDownEvent(KeyEvent &e) {
  if(!active){
    e.ignore();
    return;
    }

  if(e.key!=Event::K_ESCAPE){
    e.ignore();
    return;
    }
  close();
  }

void ChapterScreen::keyUpEvent(KeyEvent&) {
  }
