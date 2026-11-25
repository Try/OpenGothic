#include "rangescalewidget.h"

#include "assets.h"
#include "sliderwidget.h"
#include "ui/uihelper.h"

#include <Tempest/Button>
#include <Tempest/Painter>

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace Tempest;

struct RangeScaleWidget::Toggle : Button {
  explicit Toggle(RangeScaleWidget& owner) : owner(owner) {
    setButtonType(T_FlatButton);
    setSizeHint(0, UiStyle::propertyRowHeight);
    setSizePolicy(Preferred, Fixed);
    }

  void paintEvent(PaintEvent& event) override {
    Button::paintEvent(event);
    Painter p(event);
    const auto& icons = Assets::inst().ic;
    const auto  icon  = (owner.list->isVisible() ? icons.up : icons.down).sprite(w(), h(), Icon::ST_Normal);
    const int   x     = std::max(0, w() - icon.w() - 4);
    p.setBrush(icon);
    p.drawRect(x, (h() - icon.h()) / 2, icon.w(), icon.h());
    if(x <= 8)
      return;

    const auto& values = owner.samples;
    char        summary[64] = "Fixed";
    char        count[40]   = {};
    if(!values.empty()) {
      float low  = values.front();
      float high = values.front();
      for(float value : values) {
        low  = std::fmin(low, value);
        high = std::fmax(high, value);
        }
      if(low==high)
        std::snprintf(summary, sizeof(summary), "%.2f", low); else
        std::snprintf(summary, sizeof(summary), "%.2f ... %.2f", low, high);
      std::snprintf(count, sizeof(count), "[key count: %zu]", values.size());
      }
    const auto& font  = Assets::inst().fntApp;
    const int   width = x - 8;
    p.setFont(font);
    p.setBrush(style().colors().text);
    if(!values.empty() && font.textSize(summary).w + 8 + font.textSize(count).w <= width)
      p.drawText(4, 0, width, h(), count, AlignFlag(AlignRight | AlignVCenter));
    p.drawText(4, 0, width, h(), summary, AlignVCenter);
    }

  RangeScaleWidget& owner;
  };

struct RangeScaleWidget::Frame : SliderWidget {
  Frame(RangeScaleWidget& owner, size_t index) : owner(owner), index(index) {
    setSizePolicy(Preferred, Fixed);
    setMinimumSize(0, UiStyle::propertyRowHeight);
    setSliderMinMax(0.f, 2.f);
    enableFloats(true);
    onValueModifyed.bind(this, &Frame::modify);
    }

  void modify(float value, bool commit) {
    if(index >= owner.samples.size())
      return;
    const float current = owner.samples[index];
    if(commit && !editing && value == current)
      return;

    const float next = std::isnan(value) ? current : std::clamp(value, 0.f, 2.f);
    if(commit)
      setValue(next);
    if(next == current && !(commit && editing))
      return;
    owner.samples[index] = next;
    editing = !commit;
    owner.toggle->update();
    owner.onValueModifyed(commit);
    }

  RangeScaleWidget& owner;
  const size_t      index;
  bool              editing = false;
  };

RangeScaleWidget::RangeScaleWidget(std::string_view name) {
  auto& heading = addWidget(new Widget());
  heading.setLayout(Horizontal);
  heading.setSizePolicy(Preferred, Fixed);
  heading.addWidget(new Label()).setText(name);
  toggle = &heading.addWidget(new Toggle(*this));
  toggle->onClick.bind(this, &RangeScaleWidget::toggleExpanded);
  heading.setMinimumSize(UiHelper::wrapContent(heading, Horizontal).w, UiStyle::propertyRowHeight);

  list = &addWidget(new Widget());
  list->setLayout(Vertical);
  list->setSpacing(0);
  list->setSizePolicy(Preferred, Fixed);

  addRow = &list->addWidget(new Widget());
  addRow->setLayout(Vertical);
  addRow->setSizePolicy(Preferred, Fixed);
  addRow->setMinimumSize(0, UiStyle::propertyRowHeight);

  auto& append = addRow->addWidget(UiHelper::btn("Add scale"));
  append.setMinimumSize(0, UiStyle::propertyRowHeight);
  append.setMaximumSize(append.maxSize().w, UiStyle::propertyRowHeight);
  append.onClick.bind(this, &RangeScaleWidget::addSample);

  setLayout(Vertical);
  setSizePolicy(Preferred, Fixed);
  list->setVisible(false);
  adjustSize();
  }

void RangeScaleWidget::setValue(std::vector<float> value) {
  if(samples == value)
    return;
  samples = std::move(value);
  toggle->update();
  if(list->isVisible())
    syncFrames();
  }

const std::vector<float>& RangeScaleWidget::value() const {
  return samples;
  }

void RangeScaleWidget::toggleExpanded() {
  const bool expanded = !list->isVisible();
  list->setVisible(expanded);
  toggle->update();
  if(expanded)
    syncFrames();
  else
    adjustSize();
  }

void RangeScaleWidget::resizeEvent(SizeEvent&) {
  alignAddButton();
  }

void RangeScaleWidget::alignAddButton() {
  if(!list->isVisible())
    return;
  const int left  = std::max(0, widget(0).x() + toggle->x() - list->x() - addRow->x());
  const int right = std::max(0, addRow->w() - left - toggle->w());
  addRow->setMargins(Margin(left, right, 0, 0));
  }

void RangeScaleWidget::adjustSize() {
  list->setMinimumSize(0, UiHelper::wrapContent(*list).h);
  setSizeHint(UiHelper::wrapContent(*this));
  alignAddButton();
  onResize();
  }

void RangeScaleWidget::syncFrames() {
  while(list->widgetsCount() - 1 > samples.size())
    delete list->takeWidget(&list->widget(list->widgetsCount() - 2));

  for(size_t i = 0; i < samples.size(); ++i) {
    if(i == list->widgetsCount() - 1)
      list->addWidget(new Frame(*this, i), i);
    auto& frame = static_cast<Frame&>(list->widget(i));
    frame.editing = false;
    frame.setValue(samples[i]);
    }
  adjustSize();
  }

void RangeScaleWidget::addSample() {
  const float last = samples.empty() ? 1.f : samples.back();
  samples.push_back(std::isnan(last) ? 1.f : std::clamp(last, 0.f, 2.f));
  toggle->update();
  syncFrames();
  onValueModifyed(true);
  }
