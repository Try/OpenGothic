#include "searchline.h"

#include <Tempest/Application>
#include <Tempest/Painter>

#include "assets.h"

using namespace Tempest;

namespace {
constexpr int actionWidth = 22;
}

SearchLine::SearchLine(std::string_view placeholder)
  : placeholder(placeholder) {
  setFont(Application::font());
  auto margin = margins();
  margin.right += actionWidth;
  setMargins(margin);
  invalidateSizeHint();
  }

Rect SearchLine::actionRect() const {
  return Rect(w()-actionWidth, 0, actionWidth, h());
  }

void SearchLine::paintEvent(PaintEvent& e) {
  LineEdit::paintEvent(e);

  Painter p(e);
  const bool empty = text().isEmpty();
  const auto area  = actionRect();
  const auto muted = Color(0.65f);

  if(empty && !hasFocus() && w()>margins().xMargin()) {
    const auto clip = p.scissor();
    p.setScissor(clip.intersected(Rect(margins().left, 0, w()-margins().xMargin(), h())));
    p.setFont(font());
    p.setBrush(muted);
    p.drawText(margins().left, margins().top+int(font().metrics().ascent), placeholder);
    p.setScissor(clip);
    }

  const auto& icons = Assets::inst().ic;
  const auto& icon  = (empty ? icons.search : icons.close).sprite(area.w, area.h, Icon::ST_Normal);
  const int   x     = area.x+(area.w-icon.w())/2;
  const int   y     = area.y+(area.h-icon.h())/2;
  p.setBrush(icon);
  p.drawRect(x, y, icon.w(), icon.h());
  }

void SearchLine::mouseDownEvent(MouseEvent& e) {
  if(e.button==Event::ButtonLeft && !text().isEmpty() && actionRect().contains(e.pos())) {
    setText(std::string_view{});
    e.accept();
    return;
    }
  LineEdit::mouseDownEvent(e);
  }
