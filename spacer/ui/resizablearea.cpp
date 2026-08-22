#include "resizablearea.h"

#include <Tempest/Painter>
#include <Tempest/Layout>

#include "ui/editorarea.h"
#include "assets/assets.h"

using namespace Tempest;

struct ResizableArea::Layout : public Tempest::Layout {
  Layout(ResizableArea& owner):owner(owner){}

  void applyLayout() override {
    owner.relayout();
    }

  ResizableArea& owner;
  };

ResizableArea::ResizableArea(Tempest::Orientation ori) : ori(ori){
  Widget::setLayout(new Layout(*this));
  setSpacing(3);
  setOrientation(ori);
  }

void ResizableArea::setOrientation(Orientation x) {
  ori = x;
  applyLayout();
  if(ori==Vertical)
    setCursorShape(CursorShape::SizeVer); else
    setCursorShape(CursorShape::SizeHor);
  }

Orientation ResizableArea::orientation() const {
  return ori;
  }

void ResizableArea::setWeights(std::initializer_list<float> w) {
  size_t id = 0;
  for(auto& i:w) {
    if(id>=widgetsCount())
      break;
    weight.val[&widget(id)] = i;
    id++;
    }
  updateWeightVec(weight);
  }

void ResizableArea::setWeights(const std::vector<float>& w) {
  size_t id = 0;
  for(auto& i:w) {
    if(id>=widgetsCount())
      break;
    weight.val[&widget(id)] = i;
    id++;
    }
  updateWeightVec(weight);
  }

void ResizableArea::setWeights(const State& w) {
  weight = w;
  relayout();
  }

std::vector<float> ResizableArea::weights() const {
  std::vector<float> ret;
  for(size_t i=0; i<widgetsCount(); ++i) {
    auto w = weight.val.find(&widget(i));
    if(w==weight.val.end())
      ret.push_back(0); else
      ret.push_back(w->second);
    }
  return ret;
  }

void ResizableArea::paintEvent(Tempest::PaintEvent& e) {
  Painter p(e);
  p.setBrush(Assets::inst().colors.workspaceD);

  if(ori==Tempest::Horizontal) {
    for(size_t i=1;i<widgetsCount();++i)
      if(widget(i).isVisible())
        p.drawRect(widget(i).x()-spacing(),0,spacing(),h());
    } else {
    for(size_t i=1;i<widgetsCount();++i)
      if(widget(i).isVisible())
        p.drawRect(0,widget(i).y()-spacing(),w(),spacing());
    }
  }

void ResizableArea::mouseMoveEvent(Tempest::MouseEvent& event) {
  event.accept();
  }

void ResizableArea::mouseDownEvent(Tempest::MouseEvent& e) {
  auto ex = (ori==Tempest::Horizontal ? e.x : e.y);

  dr.prev = nullptr;
  dr.curr = nullptr;
  for(size_t i=0; i<widgetsCount(); ++i) {
    if(!widget(i).isVisible())
      continue;
    dr.curr = &widget(i);
    if(dr.prev!=nullptr) {
      auto p0 = dr.prev->pos();
      auto w0 = dr.prev->size();
      auto p1 = dr.curr->pos();

      auto p0x = (ori==Tempest::Horizontal ? p0.x : p0.y);
      auto p0w = (ori==Tempest::Horizontal ? w0.w : w0.h);
      auto p1x = (ori==Tempest::Horizontal ? p1.x : p1.y);
      if(p0x+p0w<=ex && ex<=p1x)
        break;
      }
    dr.prev = dr.curr;
    }
  if(dr.curr==nullptr || dr.prev==nullptr)
    return;

  x0 = ex;
  r0 = dr.prev->rect();
  r1 = dr.curr->rect();
  }

void ResizableArea::mouseDragEvent(Tempest::MouseEvent& e) {
  if(dr.curr==nullptr || dr.prev==nullptr)
    return;

  auto ex = (ori==Tempest::Horizontal ? e.x : e.y);
  auto dx = ex - x0;

  auto& w0 = *dr.prev;
  auto& w1 = *dr.curr;

  std::lock_guard<LayLock> guard(lockLayout);
  if(ori==Tempest::Horizontal) {
    if(dx<0 && r0.w+dx<w0.minSize().w)
      dx = w0.minSize().w-r0.w;
    if(dx>0 && r1.w-dx<w1.minSize().w)
      dx = r1.w-w1.minSize().w;
    w0.setGeometry(r0.x,   r0.y,r0.w+dx,r0.h);
    w1.setGeometry(r1.x+dx,r1.y,r1.w-dx,r1.h);
    } else {
    if(dx<0 && r0.h+dx<w0.minSize().h)
      dx = w0.minSize().h-r0.h;
    if(dx>0 && r1.h-dx<w1.minSize().h)
      dx = r1.h-w1.minSize().h;
    w0.setGeometry(r0.x,r0.y,   r0.w,r0.h+dx);
    w1.setGeometry(r1.x,r1.y+dx,r1.w,r1.h-dx);
    }
  initWeightVec(weight);
  }

void ResizableArea::mouseUpEvent(Tempest::MouseEvent&) {
  if(dr.curr==nullptr || dr.prev==nullptr)
    return;
  dr.curr = nullptr;
  dr.prev = nullptr;
  onResizeFinished();
  }

void ResizableArea::resizeEvent(SizeEvent& e) {
  Widget::resizeEvent(e);
  relayout();
  }

void ResizableArea::relayout() {
  if(lockLayout.v)
    return;

  std::lock_guard<LayLock> guard(lockLayout);
  if(size().isEmpty())
    return;

  updateWeightVec(weight);
  if(weight.visSum<=0)
    return;

  int linSz = 0;
  int at    = 0;
  if(orientation()==Vertical) {
    linSz = h() - margins().yMargin();
    at    = margins().top;
    } else {
    linSz = w() - margins().xMargin();
    at    = margins().left;
    }

  if(weight.visCount>1)
    linSz -= spacing()*int(weight.visCount-1);

  int remain = linSz;
  int id=0;
  for(size_t i=0; i<widgetsCount(); ++i) {
    if(!widget(i).isVisible())
      continue;
    ++id;
    float w = weight.val[&widget(i)]/weight.visSum;

    int sz = int(float(linSz)*w);
    if(id==weight.visCount)
      sz = remain;
    emplace(widget(i),at,sz);
    remain -= sz;
    at += (sz+spacing());
    }
  }

void ResizableArea::initWeightVec(State& w) {
  if(size().isEmpty())
    return;

  int linSz = 0;
  if(orientation()==Vertical) {
    linSz = this->h() - margins().yMargin();
    } else {
    linSz = this->w() - margins().xMargin();
    }

  if(linSz<=0)
    return;
  for(size_t i=0; i<widgetsCount(); ++i) {
    auto& wx = widget(i);
    if(!wx.isVisible())
      continue;
    int sz = 0;
    if(orientation()==Vertical)
      sz = wx.h(); else
      sz = wx.w();
    if(sz<50)
      sz = 50;
    w.val[&wx] = float(sz)/float(linSz);
    }
  updateWeightVec(w);
  }

void ResizableArea::updateWeightVec(State& w) {
  for(size_t i=0; i<widgetsCount(); ++i) {
    if(widget(i).isVisible() && w.val.find(&widget(i))==w.val.end()) {
      initWeightVec(w);
      return;
      }
    }

  for(auto i=w.val.begin(); i!=w.val.end();) {
    bool rm = true;
    for(size_t r=0; r<widgetsCount(); ++r)
      rm &= (&widget(r)!=i->first);
    if(rm)
      i = w.val.erase(i); else
      ++i;
    }

  w.visCount = 0;
  w.visSum   = 0;
  for(size_t i=0; i<widgetsCount(); ++i) {
    if(!widget(i).isVisible())
      continue;
    float& wx = w.val[&widget(i)];
    if(wx<=0)
      wx = 1;
    w.visSum += wx;
    w.visCount++;
    }
  }

void ResizableArea::emplace(Widget& wx, int at, int sz) {
  if(!wx.isVisible())
    return;
  auto& m = margins();
  if(orientation()==Vertical)
    wx.setGeometry(m.left, at,    w()-m.xMargin(), sz); else
    wx.setGeometry(at,     m.top, sz,              h()-m.yMargin());
  }
