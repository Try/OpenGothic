#include "toolgroup.h"

#include <Tempest/Panel>
#include <Tempest/Label>
#include <Tempest/Painter>

#include "editorsettings.h"

#include "toolwindow.h"
#include "assets/assets.h"
#include "resizablearea.h"

using namespace Tempest;

struct ToolGroup::Tab : Label {
  Tab(ToolGroup& owner, size_t id) : owner(owner), id(id) {
    setMargins(Margin(4,0,0,0));
    auto f = font();
    f.setBold(true);
    setFont(f);
    setSizePolicy(Preferred,Preferred);
    }

  void mouseDownEvent(MouseEvent& e) {
    auto st=state();
    st.pressed=true;
    setWidgetState(st);
    update();

    owner.setSelection(id);
    e.ignore();
    }

  ToolGroup&   owner;
  const size_t id = 0;
  };

struct ToolGroup::Header : public Tempest::Widget {
  Header(ToolGroup& owner):owner(owner) {
    setSizeHint(0,27);
    setSizePolicy(Preferred,Fixed);
    setLayout(Horizontal);
    }

  void paintEvent(PaintEvent& e) {
    Painter p(e);
    p.setBrush(Assets::inst().colors.workspaceD);
    p.drawRect(0,0,w(),h());

    if(owner.sel<widgetsCount()) {
      auto& w = widget(owner.sel);
      p.setBrush(Assets::inst().colors.panel);
      p.drawRect(w.x(),0,w.w(),h());
      }
    }

  void mouseDownEvent(MouseEvent& e) {
    owner.dd.begin(e,owner);

    auto ow = owner.owner();
    for(size_t i=0; i<ow->widgetsCount(); ++i)
      if(&ow->widget(i)==&owner) {
        orderId = i;
        break;
        }
    }

  void mouseDragEvent(MouseEvent& event) {
    auto ow = owner.owner();
    auto rw = dynamic_cast<ResizableArea*>(ow);

    ResizableArea::State weight;
    if(rw!=nullptr && !owner.dd.isDrag())
      weight = rw->weightVec();

    owner.dd.drag(event);
    auto& cen = *owner.central;

    if(owner.dd.isDrag() && cen.widgetsCount()>1 && ghost==nullptr) {
      auto* s = &cen.widget(owner.sel);

      ghost.reset(new ToolGroup());
      while(cen.widgetsCount()>1) {
        for(size_t i=0; i<cen.widgetsCount(); ++i) {
          if(s==&cen.widget(i))
            continue;
          ghost->central->addWidget(cen.takeWidget(&cen.widget(i)));
          }
        }

      owner.mkTabs();
      owner.setSelection(0);
      owner.invalidate();

      ghost->mkTabs();
      ghost->setSelection(0);
      ghost->invalidate();

      ow->addWidget(ghost.get(),orderId);
      if(rw!=nullptr) {
        weight.val[ghost.get()] = weight.val[&owner];
        weight.val.erase(&owner);
        rw->setWeights(weight);
        }
      }
    }

  void mouseUpEvent(MouseEvent& event) {
    if(ghost!=nullptr)
      owner.setVisible(false);
    if(!owner.dd.end(event) && ghost!=nullptr) {
      ghost->moveContent(owner);
      ghost.release();
      delete &owner;
      } else {
      ghost.release();
      }
    }

  ToolGroup&                 owner;
  std::unique_ptr<ToolGroup> ghost;
  size_t                     orderId = 0;
  };

ToolGroup::ToolGroup() {
  setMargins(Margin(0,0,0,0));
  header  = &addWidget(new Header(*this));
  central = &addWidget(new Widget);
  setLayout(Vertical);
  central->setLayout(Vertical);
  }

void ToolGroup::moveContent(ToolGroup& src) {
  auto w = src.central->takeWidget(&src.central->widget(src.sel));
  central->addWidget(w);

  mkTabs();
  setSelection(central->widgetsCount()-1);
  invalidate();

  src.mkTabs();
  src.setSelection(src.adjustSelection(src.sel));
  src.invalidate();
  }

void ToolGroup::mkTabs() {
  size_t cnt = central->widgetsCount();
  while(cnt<header->widgetsCount()) {
    header->takeWidget(&header->widget(header->widgetsCount()-1));
    }
  while(cnt>header->widgetsCount()) {
    header->addWidget(new Tab(*this,header->widgetsCount()));
    }
  }

void ToolGroup::setSelection(size_t i) {
  sel = i;
  for(size_t i=0; i<central->widgetsCount(); ++i)
    central->widget(i).setVisible(i==sel);
  if(i<central->widgetsCount()) {
    selOrder++;
    if(auto wx = dynamic_cast<ToolWindow*>(&central->widget(i))) {
      wx->setOrder(selOrder);
      }
    }
  update();
  }

size_t ToolGroup::adjustSelection(size_t sel) {
  size_t cnt     = central->widgetsCount();
  size_t nextSel = sel;
  size_t nextOrd = 0;

  for(size_t i=0; i<cnt; ++i) {
    if(auto wx = dynamic_cast<ToolWindow*>(&central->widget(i))) {
      if(wx->hasContent()) {
        if(i==sel)
          return i;
        if(nextOrd<wx->order()) {
          nextOrd = wx->order();
          nextSel = i;
          }
        }
      }
    }

  if(nextSel>=cnt)
    return 0;
  return nextSel;
  }

void ToolGroup::invalidate() {
  bool   vis = false;
  size_t cnt = central->widgetsCount();

  setSelection(adjustSelection(sel));

  for(size_t i=0; i<cnt; ++i) {
    if(auto wx = dynamic_cast<ToolWindow*>(&central->widget(i))) {
      wx->setVisible(sel==i && wx->hasContent());
      }
    vis |= central->widget(i).isVisible();
    }

  for(size_t i=0; i<cnt; ++i) {
    if(auto t = dynamic_cast<Tab*>(&header->widget(i))) {
      if(auto wx = dynamic_cast<ToolWindow*>(&central->widget(i))) {
        t->setText(wx->name());
        t->setVisible(wx->hasContent());
        } else {
        char buf[256] = {};
        std::snprintf(buf,sizeof(buf),"Tab #%d",int(i));
        t->setText(buf);
        t->setVisible(true);
        }
      }
    }
  setVisible(vis);
  }

void ToolGroup::saveUiLayout(BaseEditor::ToolType parent, size_t group) {
  size_t cnt = central->widgetsCount();
  for(size_t i=0; i<cnt; ++i) {
    if(auto wx = dynamic_cast<ToolWindow*>(&central->widget(i))) {
      EditorSettings::inst().setViewPosition(wx->tool(),parent,group,wx->order());
      }
    }
  }
