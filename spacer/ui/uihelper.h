#pragma once

#include <Tempest/Button>
#include <Tempest/Label>

namespace UiHelper {
  Tempest::Button* toolBtn(const Tempest::Icon& ic);
  Tempest::Button* btn    (const char* name);
  template<class T>
  T&               row(Tempest::Widget* to, const char* name, int nameSz = -1) {
    auto w = new Tempest::Widget();
    w->setMinimumSize(0,27);
    w->setMargins(Tempest::Margin(0,0,0,0));
    w->setLayout(Tempest::Horizontal);
    w->setSizePolicy(Tempest::Preferred,Tempest::Fixed);

    auto& lbl = w->addWidget(new Tempest::Label());
    if(nameSz>0) {
      lbl.setMinimumSize(nameSz,0);
      lbl.setSizePolicy(Tempest::Fixed,Tempest::Preferred);
      }
    lbl.setText(name);

    auto& t = w->addWidget(new T);

    to->addWidget(w);
    return t;
    }

  inline Tempest::Size wrapContent(Tempest::Widget& widget, Tempest::Orientation ori = Tempest::Vertical) {
    int w = 0, h = 0, cnt=0;
    for(size_t i=0; i<widget.widgetsCount(); ++i) {
      auto& wx = widget.widget(i);
      if(!wx.isVisible())
        continue;
      auto hint = wx.sizeHint();
      int  ww = std::max(hint.w,wx.minSize().w);
      int  wh = std::max(hint.h,wx.minSize().h);

      if(ww<=0 && ori==Tempest::Horizontal)
        continue;
      if(wh<=0 && ori==Tempest::Vertical)
        continue;

      if(ori==Tempest::Horizontal) {
        w += ww;
        h  = std::max(h,wh);
        } else {
        w  = std::max(w,ww);
        h += wh;
        }
      cnt++;
      }

    if(cnt>1) {
      --cnt;
      if(ori==Tempest::Horizontal)
        w += cnt*widget.spacing(); else
        h += cnt*widget.spacing();
      }

    auto& m = widget.margins();
    w += m.xMargin();
    h += m.yMargin();
    return Tempest::Size(w,h);
    }
  }
