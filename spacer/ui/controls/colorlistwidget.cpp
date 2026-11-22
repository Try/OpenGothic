#include "colorlistwidget.h"

#include "assets.h"
#include "colorwidget.h"
#include "numberedit.h"
#include "ui/uihelper.h"

#include <Tempest/Painter>

#include <algorithm>
#include <cmath>

using namespace Tempest;

static Color brush(const zenkit::Color& a, const zenkit::Color& b, float blend) {
  return Color(std::lerp(a.r, b.r, blend) / 255.f,
               std::lerp(a.g, b.g, blend) / 255.f,
               std::lerp(a.b, b.b, blend) / 255.f);
  }

struct ColorListWidget::Ramp : Widget {
  static constexpr int StopSize    = 10;
  static constexpr int RampHeight  = 14;
  static constexpr int ArrowPadding = 8;

  Ramp(ColorListWidget& owner, bool editable) : owner(owner), editable(editable) {
    auto& m = style().metrics();
    setSizeHint(Size(m.buttonSize,m.buttonSize));
    setSizePolicy(Preferred,Fixed);
    setLayout(Horizontal);

    if(editable)
      setFocusPolicy(ClickFocus);
    }

  int inset() const {
    return editable ? StopSize / 2 : 0;
    }

  int span() const {
    return std::max(1, w() - 2 * inset() - arrowWidth() - 1);
    }

  int arrowWidth() const {
    if(editable)
      return 0;
    return arrow().w() + 2 * ArrowPadding;
    }

  const Sprite& arrow() const {
    return Assets::inst().ic.tri_open.sprite(w(), h(), Icon::ST_Normal);
    }

  int stopX(size_t index) const {
    const auto count = owner.colors.size();
    return inset() + (count > 1 ? int(std::lround(span() * (double(index) / (count - 1)))) : 0);
    }

  void paintEvent(PaintEvent& event) override {
    Painter p(event);
    const auto& colors = owner.colors;
    const int y = editable ? 0 : (h() - RampHeight) / 2;

    if(!editable) {
      const bool expanded = owner.ramp->isVisible();
      const auto& icon = arrow();
      p.setBrush(icon);
      p.drawRect(w() - icon.w() - ArrowPadding, (h() - icon.h()) / 2, icon.w(), icon.h(),
                 0, expanded ? icon.h() : 0, icon.w(), expanded ? 0 : icon.h());
      if(expanded || w() <= arrowWidth())
        return;
      }

    const int width = span();
    if(colors.empty()) {
      p.setBrush(Color(0.15f, 0.15f, 0.15f));
      p.drawRect(inset(), y, width + 1, RampHeight);
      return;
      }

    for(int x = 0; x <= width; ++x) {
      const double frame = (double(x) / width) * (colors.size() - 1);
      const auto left = size_t(frame);
      const auto right = std::min(left + 1, colors.size() - 1);
      const float blend = owner.smooth ? float(frame - left) : 0.f;
      p.setBrush(brush(colors[left], colors[right], blend));
      p.drawRect(inset() + x, y, 1, RampHeight);
      }

    if(!editable)
      return;
    for(size_t i = 0; i < colors.size(); ++i)
      paintStop(p, i, i==owner.selected);
    }

  void paintStop(Painter& p, size_t index, bool selected) const {
    const auto& color = owner.colors[index];
    const int x = stopX(index) - StopSize / 2;
    p.setBrush(selected ? Color(0.12f, 0.56f, 1.f) : Color(0.55f, 0.55f, 0.55f));
    p.drawRect(x, RampHeight, StopSize, StopSize);
    const int border = selected ? 2 : 1;
    p.setBrush(brush(color, color, 0.f));
    p.drawRect(x + border, RampHeight + border, StopSize - 2 * border, StopSize - 2 * border);
    }

  void mouseDownEvent(MouseEvent& event) override {
    if(event.button != Event::ButtonLeft) {
      event.ignore();
      return;
      }
    if(!editable) {
      owner.toggleExpanded();
      return;
      }
    if(event.y < RampHeight) {
      event.ignore();
      return;
      }

    size_t hit = NoSelection;
    int best = StopSize / 2 + 1;
    for(size_t i = 0; i < owner.colors.size(); ++i) {
      const int distance = std::abs(event.x - stopX(i));
      if(distance < best) {
        best = distance;
        hit = i;
        }
      }
    if(hit == NoSelection) {
      event.ignore();
      return;
      }

    setFocus(true);
    owner.select(hit);
    }

  ColorListWidget& owner;
  bool             editable = false;
  };

ColorListWidget::ColorListWidget(std::string_view name) {
  auto& heading = addWidget(new Widget());
  heading.setLayout(Horizontal);
  heading.setSizePolicy(Preferred, Fixed);
  heading.addWidget(new Label()).setText(name);
  preview = &heading.addWidget(new Ramp(*this, false));
  heading.setMinimumSize(UiHelper::wrapContent(heading, Horizontal));

  ramp = &addWidget(new Ramp(*this, true));

  timeRow = &addWidget(new Widget());
  timeRow->setLayout(Horizontal);
  timeRow->setSizePolicy(Preferred, Fixed);
  timeRow->addWidget(new Label()).setText("time");
  timeEdit = &timeRow->addWidget(new NumberEdit());
  timeEdit->enableFloats(true);
  timeEdit->setEnabled(false);
  timeRow->setMinimumSize(UiHelper::wrapContent(*timeRow, Horizontal));

  color = &addWidget(new ColorWidget("color"));
  color->onValueModifyed.bind(this, &ColorListWidget::modifyColor);
  color->onResize.bind(this, &ColorListWidget::adjustSize);

  setLayout(Vertical);
  setSizePolicy(Preferred, Fixed);
  setExpanded(false);
  }

void ColorListWidget::setValue(std::vector<zenkit::Color> value) {
  colors = std::move(value);
  select(colors.empty() ? NoSelection : (selected < colors.size() ? selected : 0));
  ramp->setEnabled(!colors.empty());
  if(colors.empty())
    setExpanded(false);
  }

const std::vector<zenkit::Color>& ColorListWidget::value() const {
  return colors;
  }

void ColorListWidget::setTiming(float framesPerSecond, bool interpolate) {
  fps = std::isfinite(framesPerSecond) && framesPerSecond > 0.f ? framesPerSecond : 0.f;
  smooth = interpolate;
  updateTime();
  updateRamps();
  }

void ColorListWidget::setExpanded(bool expanded) {
  ramp->setVisible(expanded);
  preview->update();
  timeRow->setVisible(expanded && selected != NoSelection);
  color->setVisible(expanded && selected != NoSelection);
  adjustSize();
  }

void ColorListWidget::toggleExpanded() {
  if(colors.empty()) {
    setExpanded(false);
    return;
    }
  setExpanded(!ramp->isVisible());
  }

void ColorListWidget::adjustSize() {
  setSizeHint(UiHelper::wrapContent(*this));
  onResize();
  }

void ColorListWidget::select(size_t index) {
  selected = index;
  updateTime();
  if(selected != NoSelection)
    color->setValue(colors[selected]);
  setExpanded(ramp->isVisible());
  updateRamps();
  }

void ColorListWidget::updateTime() {
  if(selected != NoSelection)
    timeEdit->setValue(fps > 0.f ? double(selected) / fps : 0.f);
  }

void ColorListWidget::modifyColor(Vec4 value, bool commit) {
  if(selected == NoSelection)
    return;
  auto& c = colors[selected];
  c.r = uint8_t(value.x);
  c.g = uint8_t(value.y);
  c.b = uint8_t(value.z);
  updateRamps();
  onValueModifyed(commit);
  }

void ColorListWidget::updateRamps() {
  preview->update();
  ramp->update();
  }
