#include "colorlistwidget.h"

#include <Tempest/Painter>

#include <algorithm>
#include <cmath>
#include <string>

#include "assets.h"
#include "colorwidget.h"
#include "ui/uihelper.h"

using namespace Tempest;

struct ColorListWidget::Ramp : Widget {
  explicit Ramp(ColorListWidget& owner) : owner(owner) {
    setSizeHint(0,UiStyle::propertyRowHeight);
    setSizePolicy(Preferred,Fixed);
    }

  void paintEvent(PaintEvent& event) override {
    constexpr int inset   = 3;
    constexpr int padding = 4;

    Painter p(event);

    const auto& icons = Assets::inst().ic;
    const auto icon = (owner.list->isVisible() ? icons.up : icons.down).sprite(w(),h(),Icon::ST_Normal);
    const int x = std::max(0,w()-icon.w()-padding);
    p.setBrush(icon);
    p.drawRect(x,(h()-icon.h())/2,icon.w(),icon.h());

    const int width = x-padding-inset;
    const int rampHeight = h()-2*inset;
    if(width<=0 || rampHeight<=0)
      return;

    const auto& colors = owner.colors;
    if(colors.empty()) {
      p.setBrush(Assets::inst().colors.workspaceD);
      p.drawRect(inset,inset,width,rampHeight);
      return;
      }

    for(int x=0; x<width; ++x) {
      const double frame = owner.smooth ? (double(x) / std::max(1, width - 1)) * (colors.size() - 1)
                                        : (double(x) / width) * colors.size();
      const auto   left  = std::min(size_t(frame), colors.size() - 1);
      const auto   right = std::min(left + 1, colors.size() - 1);
      const float  blend = owner.smooth ? float(frame - left) : 0.f;
      const auto&  a     = colors[left];
      const auto&  b     = colors[right];
      p.setBrush(Color(std::lerp(a.r,b.r,blend)/255.f,
                       std::lerp(a.g,b.g,blend)/255.f,
                       std::lerp(a.b,b.b,blend)/255.f));
      p.drawRect(inset+x,inset,1,rampHeight);
      }
    }

  void mouseDownEvent(MouseEvent& event) override {
    if(event.button==Event::ButtonLeft)
      owner.toggleExpanded(); else
      event.ignore();
    }

  ColorListWidget& owner;
  };

struct ColorListWidget::Frame : ColorWidget {
  Frame(ColorListWidget& owner, size_t index)
    : ColorWidget("",std::to_string(index+1)), owner(owner), index(index) {
    onValueModifyed.bind(this,&Frame::modify);
    }

  void modify(Vec4 value, bool commit) {
    owner.modifyColor(index,value,commit);
    }

  ColorListWidget& owner;
  size_t           index;
  };

ColorListWidget::ColorListWidget(std::string_view name) {
  auto& heading = addWidget(new Widget());
  heading.setLayout(Horizontal);
  heading.setSizePolicy(Preferred,Fixed);
  heading.addWidget(new Label()).setText(name);
  preview = &heading.addWidget(new Ramp(*this));
  heading.setMinimumSize(UiHelper::wrapContent(heading,Horizontal).w,UiStyle::propertyRowHeight);

  list = &addWidget(new Widget());
  list->setLayout(Vertical);
  list->setSpacing(0);
  list->setSizePolicy(Preferred,Fixed);

  auto& append = list->addWidget(UiHelper::btn("Add color"));
  append.setMinimumSize(0,UiStyle::propertyRowHeight);
  append.setMaximumSize(append.maxSize().w,UiStyle::propertyRowHeight);
  append.onClick.bind(this,&ColorListWidget::addFrame);

  setLayout(Vertical);
  setSizePolicy(Preferred,Fixed);
  setExpanded(false);
  }

void ColorListWidget::setValue(std::vector<zenkit::Color> value) {
  colors = std::move(value);
  if(list->isVisible())
    syncFrames();
  preview->update();
  }

const std::vector<zenkit::Color>& ColorListWidget::value() const {
  return colors;
  }

void ColorListWidget::setTiming(float, bool interpolate) {
  smooth = interpolate;
  preview->update();
  }

void ColorListWidget::setExpanded(bool expanded) {
  list->setVisible(expanded);
  preview->update();
  if(expanded)
    syncFrames();
  else
    adjustSize();
  }

void ColorListWidget::toggleExpanded() {
  setExpanded(!list->isVisible());
  }

void ColorListWidget::resizeEvent(SizeEvent&) {
  alignList();
  }

void ColorListWidget::alignList() {
  if(!list->isVisible())
    return;
  const int left  = preview->x() + widget(0).x() - list->x();
  const int right = std::max(0, list->w() - left - preview->w());
  list->setMargins(Margin(left, 0, right, 0));
  }

void ColorListWidget::adjustSize() {
  alignList();
  list->setMinimumSize(0,UiHelper::wrapContent(*list).h);
  setSizeHint(UiHelper::wrapContent(*this));
  onResize();
  }

void ColorListWidget::syncFrames() {
  while(list->widgetsCount()-1>colors.size())
    delete list->takeWidget(&list->widget(colors.size()));

  for(size_t i=0; i<colors.size(); ++i) {
    if(i==list->widgetsCount()-1)
      list->addWidget(new Frame(*this,i),i);
    static_cast<Frame&>(list->widget(i)).setValue(colors[i]);
    }
  adjustSize();
  }

void ColorListWidget::modifyColor(size_t index, Vec4 value, bool commit) {
  if(index>=colors.size())
    return;
  auto& color = colors[index];
  color.r     = uint8_t(value.x);
  color.g     = uint8_t(value.y);
  color.b     = uint8_t(value.z);
  preview->update();
  onValueModifyed(commit);
  }

void ColorListWidget::addFrame() {
  colors.push_back(colors.empty() ? zenkit::Color(255,255,255,255) : colors.back());
  syncFrames();
  preview->update();
  onValueModifyed(true);
  }
