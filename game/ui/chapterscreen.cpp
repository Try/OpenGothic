#include "chapterscreen.h"

#include <Tempest/Painter>
#include <Tempest/SoundEffect>

#include "utils/gthfont.h"
#include "gothic.h"
#include "resources.h"

using namespace Tempest;

ChapterScreen::ChapterScreen() {
  }

void ChapterScreen::show(const Show& s) {
  back = Resources::loadTexture(s.img);
  if(!active)
    Gothic::inst().pushPause();
  active   = true;
  title    = s.title;
  subTitle = s.subtitle;
  if(s.time>0)
    timer.start(uint64_t(s.time));
  timer.timeout.bind(this,&ChapterScreen::close);
  Gothic::inst().emitGlobalSoundWav(s.sound);
  update();
  }

void ChapterScreen::paintEvent(Tempest::PaintEvent &e) {
  if(!active || !back)
    return;

  enum {
    width  = 800,
    height = 600
    };

  Painter p(e);
  int x = (w()-width )/2;
  int y = (h()-height)/2;
  p.setBrush(*back);
  p.drawRect(x,y,width,height,
             0,0,back->w(),back->h());

  const float scale = Gothic::interfaceScale(this);
  {
  auto& fnt = Resources::font("font_old_20_white.tga",Resources::FontType::Normal,scale);
  auto  sz  = fnt.textSize(title);
  fnt.drawText(p,x+(width-sz.w)/2,y+50+int(fnt.pixelSize()),title);
  }

  {
  auto& fnt = Resources::font(scale);
  auto  sz  = fnt.textSize(subTitle);
  fnt.drawText(p,x+(width-sz.w)/2,y+height-50,subTitle);
  }
  }

void ChapterScreen::close() {
  if(!active)
    return;
  Gothic::inst().popPause();
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
