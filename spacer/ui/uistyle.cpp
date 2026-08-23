#include "uistyle.h"

#include <Tempest/Button>
#include <Tempest/Painter>
#include <Tempest/WidgetState>

#include "assets/assets.h"

using namespace Tempest;

UiStyle::UiStyle() {
  met.scrollbarSize = 8;
  }

void UiStyle::draw(Painter& p, Tempest::Panel*, Style::Element e,
                   const WidgetState& /*st*/, const Rect& r, const Style::Extra&) const {
  if(e==E_MenuBackground)
    p.setBrush(Brush(Assets::inst().colors.menu, Painter::NoBlend)); else
    p.setBrush(Brush(Assets::inst().colors.panel,Painter::NoBlend));
  p.drawRect(r.x,r.y,r.w,r.h);
  if(e==E_MenuBackground) {
    p.setPen(Color(1,1,1,0.05f));
    p.drawLine(r.x,      r.y,       r.x+r.w,   r.y      );
    p.drawLine(r.x,      r.y+r.h-1, r.x+r.w,   r.y+r.h-1);
    p.drawLine(r.x,      r.y,       r.x,       r.y+r.h  );
    p.drawLine(r.x+r.w-1,r.y,       r.x+r.w-1, r.y+r.h  );
    }
  }

void UiStyle::draw(Painter& p, Dialog*, Style::Element /*e*/,
                   const WidgetState&, const Rect& r, const Style::Extra&) const {
  p.setBrush(Assets::inst().colors.panel);
  p.drawRect(r.x,r.y,r.w,r.h);
  }

void UiStyle::draw(Painter& p, Button* w, Style::Element e,
                   const WidgetState& st, const Rect& r, const Style::Extra& extra) const {
  if(e!=Style::E_Background)
    Style::draw(p,w,e,st,r,extra);

  auto pen   = p.pen();
  auto brush = p.brush();
  p.translate(r.x,r.y);

  const Button::Type buttonType=st.button;

  const bool drawBackFrame = (buttonType!=Button::T_ToolButton || st.moveOver) &&
                             (buttonType!=Button::T_FlatButton || st.moveOver || st.pressed) &&
                             (e!=E_MenuItemBackground || st.moveOver || st.pressed);
  if(drawBackFrame) {
    Color brush = Assets::inst().colors.highlight;
    Color pen   = Color(0,0,0,1);

    if(buttonType==Button::T_FlatButton || e==E_MenuItemBackground) {
      brush = Color(1,1,1,0);
      pen   = Color(0,0,0,0);
      }
    else if(buttonType==Button::T_CheckableButton) {
      if(st.checked!=WidgetState::Unchecked) {
        pen   = Color(0.f,0.f,1.f,1);
        brush = Assets::inst().colors.highlight;
        } else {
        pen   = Color(0.2f,0.2f,0.2f,0.5f);
        brush = Color(0.1f,0.1f,0.15f,0.5f);
        }
      }

    if(st.pressed) {
      brush -= Color(0.1f,0.1f,0.1f,0.f);
      if(brush.a()<0.2f){
        brush.set(brush.r(),brush.g(),brush.b(),0.2f);
        }
      }
    else if(st.moveOver) {
      brush += Color(0.2f,0.2f,0.2f,0.f);
      if(brush.a()<0.2f){
        brush.set(brush.r(),brush.g(),brush.b(),0.2f);
        }
      }
    if(brush.a()>0) {
      p.setBrush(brush);
      p.drawRect(0,0,r.w,r.h);
      }

    if(buttonType==Button::T_ToolButton) {
      p.setPen  (pen);
      p.drawLine(0,0,    r.w-1,0    );
      p.drawLine(0,r.h-1,r.w-1,r.h-1);
      p.drawLine(0,      0,    0,r.h-1);
      p.drawLine(r.w-1,  0,r.w-1,r.h  );
      }
    }

  p.translate(-r.x,-r.y);
  p.setBrush(brush);
  p.setPen(pen);
  }

void UiStyle::draw(Painter& p, AbstractTextInput*, Style::Element /*e*/, const WidgetState& /*st*/, const Rect& r, const Style::Extra& /*extra*/) const {
  // p.setBrush(Brush(Assets::inst().textEdit,Assets::inst().colors.workspaceD));
  p.setBrush(Brush(Assets::inst().colors.workspaceD));
  draw9Path(p,r);
  }

void UiStyle::draw(Painter& p, ComboBox*, Style::Element /*e*/,
                   const WidgetState& /*st*/, const Rect& r, const Style::Extra& /*extra*/) const {
  const bool large = r.h>25;
  if(large) {
    p.setPen(Assets::inst().colors.workspace);
    p.drawLine(r.x,r.y+r.h-1,r.x+r.w,r.y+r.h-1);
    auto& ic = Assets::inst().ic.tri_open;
    auto& px = ic.sprite(r.w,r.h,Icon::ST_Normal);
    p.setBrush(px);
    p.drawRect(r.x+r.w-px.w()-8, r.y+(r.h-px.h())/2, px.w(), px.h(),
               0,0,px.w(),px.h());
    } else {
    auto& ic = Assets::inst().ic.tri_open_small;
    auto& px = ic.sprite(r.w,r.h,Icon::ST_Normal);
    p.setBrush(px);
    p.drawRect(r.x+r.w-px.w()-8, r.y+(r.h-px.h())/2, px.w(), px.h(),
               0,0,px.w(),px.h());
    }
  }

void UiStyle::draw(Painter& p, CheckBox*, Style::Element /*e*/,
                   const WidgetState& st, const Rect& r, const Style::Extra&) const {
  const int  s  = std::min(r.w,r.h);
  const Size sz = Size(s,s);

  p.translate(r.x,r.y);

  const Sprite* b = nullptr;
  if(st.checked==WidgetState::Checked)
    b = &Assets::inst().ic.check_on.sprite(sz.w,sz.h,Icon::ST_Normal); else
    b = &Assets::inst().ic.check_off.sprite(sz.w,sz.h,Icon::ST_Normal);

  p.setBrush(*b);
  p.drawRect((sz.w-p.brush().w())/2,(sz.h-p.brush().h())/2,
             p.brush().w(),p.brush().h());

  p.translate(-r.x,-r.y);
  }

void UiStyle::draw(Painter& p, Dialog*, UiOverlay*, Style::Element /*e*/, const WidgetState& /*st*/,
                   const Rect& r, const Style::Extra& /*extra*/, const Rect& overlay) const {
  p.setBrush(Color(0.1f,0.1f,0.1f,0.55f));
  p.drawRect(overlay);

  p.setPen(Assets::inst().colors.workspace);
  p.drawLine(r.x-1, r.y-1,   r.x+r.w+1, r.y-1);
  p.drawLine(r.x-1, r.y+r.h, r.x+r.w+1, r.y+r.h);
  p.drawLine(r.x-1, r.y,     r.x-1,     r.y+r.h);
  p.drawLine(r.x+r.w, r.y, r.x+r.w, r.y+r.h);
  }

void UiStyle::draw(Painter& p, ScrollBar* w, Style::Element e,
                   const WidgetState& st, const Rect& r, const Style::Extra& extra) const {
  if(e==E_CentralButton) {
    auto brush = p.brush();
    if(st.pressed) {
      // p.setBrush(Brush(Assets::inst().scroll,Color(2.f,2.f,2.f,1.f)));
      p.setBrush(Color(2.f,2.f,2.f,1.f));
      } else {
      // p.setBrush(Assets::inst().scroll);
      p.setBrush(Color(1.f));
      }
    draw9Path(p,r);
    p.setBrush(brush);
    } else {
    Style::draw(p,w,e,st,r,extra);
    }
  }

const Style::UiMetrics& UiStyle::metrics() const {
  return met;
  }

Style::Element UiStyle::visibleElements() const {
  return Style::Element(Style::E_All ^
                        (Style::E_ArrowUp | Style::E_ArrowDown | Style::E_ArrowLeft | Style::E_ArrowRight));
  }

void UiStyle::draw9Path(Tempest::Painter& p, const Tempest::Rect& r) {
  p.translate(r.x,r.y);

  auto& b = p.brush();

  int pd = std::min(std::min(b.w()/2,r.w/2),std::min(b.h()/2,r.h/2));
  p.drawRect(0,0,pd,pd,
             0,0,pd,pd);
  p.drawRect(r.w-pd,0,pd,pd,
             b.w()-pd,0,b.w(),pd);
  p.drawRect(0,r.h-pd,pd,pd,
             0,b.h()-pd,pd,b.h());
  p.drawRect(r.w-pd,r.h-pd,pd,pd,
             b.w()-pd,b.h()-pd,b.w(),b.h());

  p.drawRect(pd, 0, r.w-pd*2, pd,
             pd, 0, b.w()-pd, pd);
  p.drawRect(pd, r.h-pd,   r.w-pd*2, pd,
             pd, b.h()-pd, b.w()-pd, b.h());

  p.drawRect(0, pd, pd, r.h-pd*2,
             0, pd, pd, b.h()-pd);
  p.drawRect(r.w-pd,   pd, pd,    r.h-pd*2,
             b.w()-pd, pd, b.w(), b.h()-pd);

  p.drawRect(pd, pd, r.w-pd*2, r.h-pd*2,
             pd, pd, b.w()-pd, b.h()-pd);

  p.translate(-r.x,-r.y);
  }
