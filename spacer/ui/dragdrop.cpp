#include "dragdrop.h"

#include <Tempest/Layout>

using namespace Tempest;

DragDrop::DragDrop() {
  }

DragDrop::~DragDrop() {
  if(drItem==nullptr)
    return;
  Tempest::MouseEvent e;
  end(e);
  }

Widget& DropOverEvent::drop() {
  return *dropable;
  }

void DropOverEvent::setDropLocation(size_t pos) {
  dpos = pos;
  }

size_t DropOverEvent::dropLocation() const {
  return dpos;
  }

void DropOverEvent::setPosition(const Point& p) {
  mpos = p;
  }

const Point& DropOverEvent::pos() const {
  return mpos;
  }

void DragDrop::begin(MouseEvent &e, Tempest::Widget& w) {
  if(drItem!=nullptr)
    return;

  overlay.reset(new Overlay());
  SystemApi::addOverlay(overlay.get());

  drItem  = &w;
  mOwner   = w.owner();
  if(mOwner!=nullptr) {
    for(size_t i=0;i<mOwner->widgetsCount();++i)
      if(&w==&mOwner->widget(i)) {
        mOwnerAt = i;
        break;
        }
    }
  state   = PreDrag;
  mDrop   = nullptr;
  mDropAt = 0;
  dpos    = e.pos();
  }

void DragDrop::drag(MouseEvent& e) {
  switch(state) {
    case PreDrag: {
      Point diff = e.pos()-dpos;
      if(diff.length()>15){
        state=Drag;
        Point pos = drItem->mapToRoot(e.pos());

        overlay->addWidget(drItem);
        drItem->setPosition(pos-dpos);
        }
      break;
      }
    case Drag: {
      Point pos = drItem->mapToRoot(e.pos());
      drItem->setPosition(pos-dpos);

      DropOverEvent ev(drItem);
      if(Widget* d=solveDrop(ev,pos)) {
        Widget* curDrop = mDrop;
        setDrop(d,ev.dropLocation());
        if(DropReciver* dr = dynamic_cast<DropReciver*>(curDrop)) {
          if(curDrop!=mDrop)
            dr->moveDropLeave(ev);
          }
        }
      break;
      }
    }
  }

bool DragDrop::end(MouseEvent& e) {
  bool dropped = false;
  if(mDrop!=nullptr && state!=PreDrag) {
    Point pos = drItem->mapToRoot(e.pos());
    DropOverEvent ev(drItem);
    if(DropReciver* dr = dynamic_cast<DropReciver*>(mDrop)){
      if(Widget* w = dynamic_cast<Widget*>(dr))
        ev.setPosition( pos-w->mapToRoot(Point(0,0)) );
      ev.ignore();
      dr->dropDone(ev);
      if(ev.isAccepted())
        dropped = true;
      }
    }

  if(mOwner!=nullptr && state!=PreDrag && !dropped){
    mOwner->addWidget(drItem,mOwnerAt);
    }
  drItem = nullptr;
  mDrop  = nullptr;
  mOwner = nullptr;
  overlay.reset();
  return dropped;
  }

Widget *DragDrop::dragable() {
  return drItem;
  }

const Widget *DragDrop::dragable() const {
  return drItem;
  }

void DragDrop::setDrop(Widget *owner, size_t p) {
  mDrop   = owner;
  mDropAt = p;
  }

Widget* DragDrop::drop() const {
  return mDrop;
  }


Widget* DragDrop::solveDrop(DropOverEvent& ev, const Point& p) {
  Widget* root = mOwner;
  while(root->owner()!=nullptr)
    root = root->owner();
  return implSolveDrop(ev,root,p);
  }

Widget* DragDrop::implSolveDrop(DropOverEvent& ev, Widget* wx, const Point& p) {
  if(!wx->size().toRect().contains(p,true))
    return nullptr;

  if(!wx->isVisible())
    return nullptr;

  size_t count=wx->widgetsCount();
  for(size_t i=0; i<count; ++i){
    Widget* w      = &wx->widget(count-i-1);
    Widget* nested = implSolveDrop(ev,w,p-w->pos());
    if(nested)
      return nested;
    }

  if(DropReciver* dr=dynamic_cast<DropReciver*>(wx)){
    ev.ignore();
    ev.setPosition(p);
    dr->moveDropOver(ev);
    if(ev.isAccepted())
      return wx;
    }

  return nullptr;
  }
