#include "chapterscreen.h"

#include <Tempest/Painter>
#include <Tempest/SoundEffect>

#include "utils/gthfont.h"
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

  {
  auto& fnt = Resources::font();
  auto  sz  = fnt.textSize(title);
  fnt.drawText(p,x+(back->w()-sz.w)/2,y+50+int(fnt.pixelSize()),title);
  }

  {
  auto& fnt = Resources::font();
  auto  sz  = fnt.textSize(subTitle);
  fnt.drawText(p,x+(back->w()-sz.w)/2,y+back->h()-50,subTitle);
  }
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
