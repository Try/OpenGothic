#include "dialogmenu.h"

#include <Tempest/Painter>

#include "resources.h"

using namespace Tempest;

DialogMenu::DialogMenu() {
  tex = Resources::loadTexture("DLG_CHOICE.TGA");
  }

void DialogMenu::start() {

  }

void DialogMenu::paintEvent(Tempest::PaintEvent &e) {
  Painter p(e);

  if(tex) {
    int dw = std::min(w(),600);
    p.setBrush(*tex);
    p.drawRect((w()-dw)/2,20,dw,100,
               0,0,tex->w(),tex->h());
    }
  }
