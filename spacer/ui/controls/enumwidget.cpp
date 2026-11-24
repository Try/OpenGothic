#include "enumwidget.h"

#include <Tempest/Application>
#include <Tempest/Painter>

#include "assets.h"
#include "ui/uistyle.h"

using namespace Tempest;

namespace {
  constexpr int segmentGap     = 1;
  constexpr int segmentPadding = 4;
  }

EnumWidget::EnumWidget(std::string_view name)
  : title(name) {
  addWidget(&combo);
  combo.onItemSelected    .bind(&onItemSelected,    &Signal<void(size_t)>::operator());
  combo.onSelectionChanged.bind(&onSelectionChanged,&Signal<void(size_t)>::operator());
  title.setFont(Application::font());

  setSizeHint(0,UiStyle::propertyRowHeight);
  setSizePolicy(Preferred,Fixed);
  updateLayout();
  }

void EnumWidget::setDisplayPolicy(DisplayPolicy policy) {
  if(displayPolicy==policy)
    return;
  displayPolicy = policy;
  updateLayout();
  update();
  }

void EnumWidget::setItems(const std::vector<std::string>& values) {
  if(currentIndex()>=values.size())
    combo.setCurrentIndex(0);
  combo.setItems(values);
  items.clear();
  items.reserve(values.size());
  int segmentWidth = UiStyle::propertyRowHeight;
  for(const auto& value : values) {
    auto& item = items.emplace_back(value);
    item.setFont(title.font());
    segmentWidth = std::max(segmentWidth,item.wrapSize().w + 2 * segmentPadding);
    }
  const int count = int(items.size());
  requiredWidth = count * segmentWidth + std::max(0,count - 1) * segmentGap;
  updateLayout();
  update();
  }

void EnumWidget::setCurrentIndex(size_t index) {
  if(index>=items.size() || index==currentIndex())
    return;
  combo.setCurrentIndex(index);
  update();
  }

size_t EnumWidget::currentIndex() const {
  return combo.currentIndex();
  }

void EnumWidget::resizeEvent(SizeEvent&) {
  updateLayout();
  }

void EnumWidget::updateLayout() {
  const auto client     = clientRect();
  const int  labelWidth = std::max(0,(client.w - spacing()) / 2);
  const int  offset     = labelWidth + spacing();

  area = Rect(client.x + offset,client.y,std::max(0,client.w - offset),client.h);
  const bool fallback = displayPolicy==DisplayPolicy::Combo || items.empty() || area.w<requiredWidth;
  combo.setVisible(fallback);
  combo.setGeometry(area);
  setFocusPolicy(fallback ? NoFocus : StrongFocus);
  hovered = pressed = -1;
  }

Rect EnumWidget::segmentRect(int index) const {
  const int count = int(items.size());
  const int width = area.w - (count - 1) * segmentGap;
  const int start = index * width / count;
  const int end   = (index + 1) * width / count;
  return Rect(area.x + start + index * segmentGap,area.y,end - start,area.h);
  }

int EnumWidget::segmentAt(int x, int y) const {
  if(combo.isVisible() || !isEnabled() || y<area.y || y>=area.y + area.h)
    return -1;
  for(int i=0; i<int(items.size()); ++i) {
    const auto rect = segmentRect(i);
    if(x>=rect.x && x<rect.x + rect.w)
      return i;
    }
  return -1;
  }

void EnumWidget::paintEvent(PaintEvent& e) {
  auto localEvent = [&e](const Rect& rect) {
    const auto clip = e.viewPort().intersected(rect);
    return PaintEvent(e,rect.x,rect.y,clip.x - rect.x,clip.y - rect.y,clip.w,clip.h);
    };
  auto textState = state();
  textState.focus = false;
  const Style::Extra extra(*this);

  const auto client     = clientRect();
  const int  labelWidth = area.x - client.x - spacing();
  if(labelWidth>0) {
    auto event = localEvent(Rect(client.x,client.y,labelWidth,client.h));
    Painter p(event);
    style().draw(p,title,Style::TE_LabelTitle,textState,Rect(0,0,labelWidth,client.h),extra);
    }

  if(combo.isVisible())
    return;

  const auto& colors = Assets::inst().colors;
  for(int i=0; i<int(items.size()); ++i) {
    const auto rect = segmentRect(i);
    auto event = localEvent(rect);
    Painter p(event);
    const auto bounds = Rect(0,0,rect.w,rect.h);
    const bool selected = size_t(i)==currentIndex();
    p.setBrush(selected ? colors.workspace : i==hovered ? colors.panel : colors.workspaceD);
    p.drawRect(bounds);
    style().draw(p,items[i],Style::TE_ButtonTitle,textState,bounds,extra);
    if(selected && hasFocus()) {
      p.setPen(colors.highlight);
      p.drawLine(0,rect.h - 1,rect.w - 1,rect.h - 1);
      }
    }
  }

void EnumWidget::mouseDownEvent(MouseEvent& e) {
  if(e.button==Event::ButtonLeft) {
    pressed = segmentAt(e.x,e.y);
    if(pressed>=0)
      return;
    }
  e.ignore();
  }

void EnumWidget::mouseUpEvent(MouseEvent& e) {
  if(e.button!=Event::ButtonLeft || pressed<0) {
    e.ignore();
    return;
    }
  const int index = pressed;
  pressed = -1;
  if(segmentAt(e.x,e.y)==index)
    select(index);
  }

void EnumWidget::mouseMoveEvent(MouseEvent& e) {
  const int index = segmentAt(e.x,e.y);
  if(hovered!=index) {
    hovered = index;
    update();
    }
  }

void EnumWidget::mouseLeaveEvent(MouseEvent&) {
  hovered = -1;
  update();
  }

void EnumWidget::keyDownEvent(KeyEvent& e) {
  if(isEnabled() && !combo.isVisible() && (e.key==Event::K_Left || e.key==Event::K_Right)) {
    const int step = e.key==Event::K_Left ? -1 : 1;
    const int count = int(items.size());
    select((int(currentIndex()) + step + count) % count);
    return;
    }
  Widget::keyDownEvent(e);
  }

void EnumWidget::select(int index) {
  if(size_t(index)==currentIndex())
    return;
  setCurrentIndex(size_t(index));
  onItemSelected(size_t(index));
  }
