#include "rangescalewidget.h"

#include "assets.h"
#include "numberedit.h"
#include "ui/uihelper.h"

#include <Tempest/Painter>

#include <algorithm>
#include <cmath>

using namespace Tempest;

struct RangeScaleWidget::Curve : Widget {
  static constexpr int PointSize = 7;
  static constexpr int ArrowPadding = 8;

  Curve(RangeScaleWidget& owner, bool interactive) : owner(owner), interactive(interactive) {
    const int rowHeight = style().metrics().buttonSize;
    setSizeHint(rowHeight, interactive ? 5 * rowHeight : rowHeight);
    setSizePolicy(Preferred, Fixed);
    setFocusPolicy(ClickFocus);
    }

  Rect plot() const {
    return Rect(PointSize, PointSize, std::max(1, w() - 2 * PointSize - 1),
                std::max(1, h() - 2 * PointSize - 1));
    }

  Vec2 limits() const {
    if(dragging)
      return dragLimits;

    float low = 0.f, high = 2.f;
    for(float value : owner.samples) {
      if(std::isfinite(value)) {
        low = std::min(low, value);
        high = std::max(high, value);
        }
      }
    return Vec2(low, high);
    }

  Point point(size_t index, const Rect& area, Vec2 range) const {
    const auto value = owner.samples[index];
    const auto fraction = std::isfinite(value) ? (double(value) - range.x) / (double(range.y) - range.x) : 0.0;
    const auto count = owner.samples.size();
    const int x = count > 1 ? int(std::lround(double(index) * area.w / (count - 1))) : 0;
    return Point(area.x + x, area.y + int(std::lround((1.0 - fraction) * area.h)));
    }

  void paintEvent(PaintEvent& event) override {
    Painter p(event);
    if(!interactive) {
      const auto& icon = Assets::inst().ic.tri_open.sprite(w(), h(), Icon::ST_Normal);
      p.setBrush(icon);
      p.drawRect(w() - icon.w() - ArrowPadding, (h() - icon.h()) / 2, icon.w(), icon.h(),
                 0, owner.expanded ? icon.h() : 0, icon.w(), owner.expanded ? 0 : icon.h());
      const int textWidth = w() - icon.w() - 2 * ArrowPadding;
      if(!owner.expanded && textWidth > 0) {
        p.setFont(Assets::inst().fntApp);
        p.setBrush(style().colors().text);
        p.drawText(0, 0, textWidth, h(), "...", AlignFlag(AlignHCenter | AlignVCenter));
        }
      return;
      }

    const auto area = plot();
    const auto range = limits();
    p.setBrush(Assets::inst().colors.workspaceD);
    p.drawRect(0, 0, w(), h());

    // The horizontal guide marks the unscaled light range (1x).
    const int y = area.y + int(std::lround((double(range.y) - 1.0) / (double(range.y) - range.x) * area.h));
    p.setPen(Color(0.45f, 0.45f, 0.45f));
    p.drawLine(area.x, y, area.x + area.w, y);

    const auto count = owner.samples.size();
    if(count == 0)
      return;

    const Color line(0.42f, 0.71f, 1.f);
    p.setPen(line);
    auto previous = point(0, area, range);
    if(count == 1)
      p.drawLine(previous.x, previous.y, area.x + area.w, previous.y);
    for(size_t i = 1; i < count; ++i) {
      const auto next = point(i, area, range);
      if(owner.smooth) {
        p.drawLine(previous, next);
        } else {
        p.drawLine(previous, Point(next.x, previous.y));
        p.drawLine(Point(next.x, previous.y), next);
        }
      previous = next;
      }

    p.setBrush(line);
    for(size_t i = 0; i < count; ++i) {
      const auto at = point(i, area, range);
      p.drawRect(at.x - 2, at.y - 2, 5, 5);
      }
    const auto at = point(owner.selected, area, range);
    p.setBrush(Color(1.f, 1.f, 1.f));
    p.drawRect(at.x - PointSize / 2, at.y - PointSize / 2, PointSize, PointSize);
    }

  void mouseDownEvent(MouseEvent& event) override {
    if(event.button != Event::ButtonLeft) {
      event.ignore();
      return;
      }
    if(!interactive) {
      owner.toggleExpanded();
      return;
      }
    if(owner.samples.empty()) {
      event.ignore();
      return;
      }

    const auto area = plot();
    const auto range = limits();
    size_t hit = owner.selected;
    int best = PointSize * PointSize;
    for(size_t i = 0; i < owner.samples.size(); ++i) {
      const auto at = point(i, area, range);
      const int dx = std::abs(event.x - at.x), dy = std::abs(event.y - at.y);
      if(dx > PointSize || dy > PointSize)
        continue;
      const int distance = dx * dx + dy * dy;
      if(distance < best) {
        hit = i;
        best = distance;
        }
      }
    if(best == PointSize * PointSize) {
      event.ignore();
      return;
      }
    setFocus(true);
    owner.select(hit);
    dragLimits = range;
    dragY = event.y;
    dragValue = std::isfinite(owner.samples[hit]) ? owner.samples[hit] : 0.f;
    dragging = true;
    }

  void mouseDragEvent(MouseEvent& event) override {
    if(!dragging)
      return;
    const auto area = plot();
    const double delta = double(dragY - event.y) / area.h * (double(dragLimits.y) - dragLimits.x);
    const auto value = float(std::clamp(double(dragValue) + delta, 0.0, double(dragLimits.y)));
    owner.scale->setValue(value);
    owner.changeScale(value, false);
    }

  void mouseUpEvent(MouseEvent&) override {
    if(!dragging)
      return;
    dragging = false;
    update();
    owner.changeScale(owner.samples[owner.selected], true);
    }

  RangeScaleWidget& owner;
  bool interactive = false;
  bool dragging = false;
  int dragY = 0;
  float dragValue = 0.f;
  Vec2 dragLimits;
  };

RangeScaleWidget::RangeScaleWidget(std::string_view name) {
  auto& heading = addWidget(new Widget());
  heading.setLayout(Horizontal);
  heading.setSizePolicy(Preferred, Fixed);
  heading.addWidget(new Label()).setText(name);
  auto& field = heading.addWidget(new Widget());
  field.setLayout(Horizontal);
  field.setSizePolicy(Preferred, Fixed);

  actions = &field.addWidget(new Widget());
  actions->setLayout(Horizontal);
  actions->setSizePolicy(Fixed);
  auto& append = actions->addWidget(UiHelper::btn("+"));
  append.onClick.bind(this, &RangeScaleWidget::addSample);
  remove = &actions->addWidget(UiHelper::btn("-"));
  remove->onClick.bind(this, &RangeScaleWidget::removeSample);
  const int buttonSize = style().metrics().buttonSize;
  for(auto* button : {&append, remove}) {
    button->setMargins(0);
    button->setSizePolicy(Fixed);
    button->setMinimumSize(buttonSize, buttonSize);
    button->setMaximumSize(Size(buttonSize));
    }
  actions->setMinimumSize(2 * buttonSize + actions->spacing(), buttonSize);

  preview = &field.addWidget(new Curve(*this, false));
  field.setMinimumSize(0, preview->sizeHint().h);
  heading.setMinimumSize(UiHelper::wrapContent(heading, Horizontal));

  curve = &addWidget(new Curve(*this, true));

  time = &UiHelper::row<NumberEdit>(this, "time");
  timeRow = time->owner();
  time->enableFloats(true);
  time->setEnabled(false);
  scale = &UiHelper::row<NumberEdit>(this, "scale");
  scaleRow = scale->owner();
  scale->enableFloats(true);
  scale->onValueModifyed.bind(this, &RangeScaleWidget::editScale);

  setLayout(Vertical);
  setSizePolicy(Preferred, Fixed);
  refresh();
  }

void RangeScaleWidget::setValue(std::vector<float> value) {
  samples = std::move(value);
  editing = false;
  curve->dragging = false;
  select(samples.empty() ? 0 : std::min(selected, samples.size() - 1));
  refresh();
  }

const std::vector<float>& RangeScaleWidget::value() const {
  return samples;
  }

void RangeScaleWidget::setTiming(float rate, bool interpolate) {
  fps = std::isfinite(rate) ? std::max(0.f, rate) : 0.f;
  smooth = interpolate;
  time->setValue(fps > 0.f ? double(selected) / fps : 0.f);
  preview->update();
  curve->update();
  }

void RangeScaleWidget::toggleExpanded() {
  expanded = !expanded;
  refresh();
  }

void RangeScaleWidget::refresh() {
  curve->setVisible(expanded);
  actions->setVisible(expanded);
  timeRow->setVisible(expanded && !samples.empty());
  scaleRow->setVisible(expanded && !samples.empty());
  remove->setEnabled(!samples.empty());
  preview->update();
  curve->update();
  setSizeHint(UiHelper::wrapContent(*this));
  onResize();
  }

void RangeScaleWidget::select(size_t index) {
  selected = index;
  time->setValue(fps > 0.f ? double(selected) / fps : 0.f);
  if(!samples.empty())
    scale->setValue(samples[selected]);
  curve->update();
  }

void RangeScaleWidget::addSample() {
  samples.push_back(samples.empty() ? 1.f : samples.back());
  select(samples.size() - 1);
  refresh();
  onValueModifyed(true);
  }

void RangeScaleWidget::removeSample() {
  if(samples.empty())
    return;
  samples.erase(samples.begin() + selected);
  select(samples.empty() ? 0 : std::min(selected, samples.size() - 1));
  refresh();
  onValueModifyed(true);
  }

void RangeScaleWidget::editScale(double value, bool commit) {
  if(!std::isfinite(value))
    return;
  const float next = float(std::max(0.0, value));
  if(commit)
    scale->setValue(next);
  changeScale(next, commit);
  }

void RangeScaleWidget::changeScale(float value, bool commit) {
  if(samples.empty() || (samples[selected] == value && !(commit && editing)))
    return;
  samples[selected] = value;
  editing = !commit;
  preview->update();
  curve->update();
  onValueModifyed(commit);
  }
