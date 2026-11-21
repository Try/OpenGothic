#include "vobtreeview.h"

#include <Tempest/Painter>
#include <Tempest/TextModel>

#include <algorithm>
#include <cctype>

#include "assets.h"
#include "objects/vobreflection.h"

using namespace Tempest;

namespace {
constexpr int rowPadding    = 4;
constexpr int arrowLabelGap = 4;
}

VobTreeView::VobTreeView()
  : verticalBar(addWidget(new ScrollBar(Vertical))),
    horizontalBar(addWidget(new ScrollBar(Horizontal))) {
  setFocusPolicy(ClickFocus);
  verticalBar.onValueChanged.bind(this, &VobTreeView::scrollChanged);
  horizontalBar.onValueChanged.bind(this, &VobTreeView::scrollChanged);
  updateScrollBars();
  }

void VobTreeView::setWorld(WorldEdit& w) {
  world = &w;
  selected = nullptr;
  collapsed.clear();
  filteredCollapsed.clear();
  verticalBar.setValue(0);
  horizontalBar.setValue(0);
  verticalWheelRemainder   = 0;
  horizontalWheelRemainder = 0;
  updateView();
  }

void VobTreeView::setVob(const WorldEdit::Vob* vob) {
  selected = vob;
  update();
  }

void VobTreeView::setFilter(const TextModel& text) {
  std::string query(text.c_str(), text.size());
  for(auto& ch : query)
    ch = char(std::tolower(static_cast<unsigned char>(ch)));
  if(query==filter)
    return;

  filter = std::move(query);
  updateFilter();
  }

bool VobTreeView::hasClassFilters() const {
  return !classFilters.empty();
  }

bool VobTreeView::isClassFiltered(zenkit::VirtualObjectType type) const {
  return classFilters.contains(type);
  }

void VobTreeView::toggleClassFilter(zenkit::VirtualObjectType type) {
  if(classFilters.erase(type)==0)
    classFilters.insert(type);
  updateFilter();
  }

void VobTreeView::clearClassFilters() {
  if(classFilters.empty())
    return;
  classFilters.clear();
  updateFilter();
  }

bool VobTreeView::isFiltering() const {
  return !filter.empty() || hasClassFilters();
  }

void VobTreeView::updateFilter() {
  filteredCollapsed.clear();
  verticalBar.setValue(0);
  horizontalBar.setValue(0);
  updateView();
  }

void VobTreeView::updateView() {
  pressed = nullptr;
  rows.clear();
  if(world!=nullptr) {
    if(!isFiltering())
      appendRows(world->root(), 0); else
      appendFilteredRows(world->root(), 0);
    }

  treeWidth = 0;
  if(!rows.empty()) {
    const auto& assets = Assets::inst();
    const int   height = rowHeight();
    auto        font   = assets.fntSmall;
    font.setBold(true);
    const int indent          = assets.ic.up.sprite(height, height, Icon::ST_Normal).w();
    const int collapsedIndent = assets.ic.down.sprite(height, height, Icon::ST_Normal).w();

    for(const auto& row : rows) {
      const int arrowWidth = isCollapsed(row.vob) ? collapsedIndent : indent;
      const int groupWidth = row.hasVisibleChildren ? arrowWidth+arrowLabelGap : 0;
      treeWidth = std::max(treeWidth, 2*rowPadding + int(row.depth)*arrowWidth + groupWidth + font.textSize(row.label()).w);
      }
    }
  updateScrollBars();
  update();
  }

void VobTreeView::revealSelection() {
  if(world==nullptr || selected==nullptr)
    return;

  const auto isSelected = [this](const Row& row) { return row.vob==selected; };
  auto       row        = std::find_if(rows.begin(), rows.end(), isSelected);
  if(row==rows.end()) {
    if(!openSelectionPath(world->root()))
      return;
    updateView();
    row = std::find_if(rows.begin(), rows.end(), isSelected);
    if(row==rows.end())
      return;
    }

  const auto area   = viewport();
  const int  height = rowHeight();
  const int  top    = int(row-rows.begin())*height;
  if(top<verticalBar.value() || top+height>verticalBar.value()+area.h)
    verticalBar.setValue(top-std::max(0, (area.h-height)/2));

  const auto& arrow = arrowSprite(*row);
  const int   x     = arrowRect(*row, 0, arrow).x;
  const int   label = x+(row->hasVisibleChildren ? arrow.w()+arrowLabelGap : 0);
  if(label<0 || label>=area.w)
    horizontalBar.setValue(horizontalBar.value()+x-rowPadding);
  }

bool VobTreeView::openSelectionPath(const WorldEdit::Vob& node) {
  if(&node==selected)
    return true;

  for(size_t i=0; i<node.size(); ++i) {
    if(openSelectionPath(node[i])) {
      (isFiltering() ? filteredCollapsed : collapsed).erase(&node);
      return true;
      }
    }
  return false;
  }

void VobTreeView::appendRows(WorldEdit::Vob& vob, size_t depth) {
  rows.push_back({&vob, depth, vob.size()>0});
  if(collapsed.count(&vob)!=0)
    return;

  for(size_t i=0; i<vob.size(); ++i)
    appendRows(vob[i], depth+1);
  }

void VobTreeView::appendFilteredRows(WorldEdit::Vob& vob, size_t depth) {
  const size_t index = rows.size();
  rows.push_back({&vob, depth, false});

  for(size_t i=0; i<vob.size(); ++i)
    appendFilteredRows(vob[i], depth+1);

  auto&      row                = rows[index];
  const bool hasVisibleChildren = rows.size()>index+1;
  const auto* object = vob.get();
  if(!hasVisibleChildren && (object==nullptr || !matchesClassFilter(object->type) ||
                            (!filter.empty() && !matchesFilter(row.label(), filter)))) {
    rows.pop_back();
    return;
    }

  row.hasVisibleChildren = hasVisibleChildren;
  if(isCollapsed(&vob))
    rows.resize(index+1);
  }

bool VobTreeView::matchesClassFilter(zenkit::VirtualObjectType type) const {
  if(classFilters.empty() || classFilters.contains(type))
    return true;

  const auto* cls = VobReflection::find(type);
  for(auto base = cls!=nullptr ? cls->baseClass() : nullptr; base!=nullptr; base = base->baseClass())
    if(base->type!=VobReflection::Type::zCVob && classFilters.contains(base->type))
      return true;
  return false;
  }

bool VobTreeView::matchesFilter(std::string_view row, std::string_view pattern) {
  return std::search(row.begin(), row.end(), pattern.begin(), pattern.end(),
                     [](unsigned char a, unsigned char b) { return std::tolower(a)==std::tolower(b); })!=row.end();
  }

bool VobTreeView::isCollapsed(const WorldEdit::Vob* vob) const {
  return (isFiltering() ? filteredCollapsed : collapsed).count(vob)!=0;
  }

int VobTreeView::rowHeight() const {
  return std::max(1, style().metrics().buttonSize);
  }

Rect VobTreeView::viewport() const {
  return Rect(0, 0, verticalBar.isVisible() ? verticalBar.x() : w(),
                    horizontalBar.isVisible() ? horizontalBar.y() : h());
  }

void VobTreeView::updateScrollBars() {
  const int height     = rowHeight();
  const int treeHeight = int(rows.size())*height;
  const int thickness  = style().metrics().scrollbarSize;
  const int barWidth   = std::clamp(thickness, 0, w());
  const int barHeight  = std::clamp(thickness, 0, h());

  bool showVertical = barWidth>0 && treeHeight>h();
  const bool showHorizontal = barHeight>0 && treeWidth>w()-(showVertical ? barWidth : 0);
  showVertical = barWidth>0 && treeHeight>h()-(showHorizontal ? barHeight : 0);
  const int width      = w()-(showVertical ? barWidth : 0);
  const int pageHeight = h()-(showHorizontal ? barHeight : 0);

  verticalBar.setGeometry(width, 0, barWidth, pageHeight);
  horizontalBar.setGeometry(0, pageHeight, width, barHeight);
  verticalBar.setVisible(showVertical);
  horizontalBar.setVisible(showHorizontal);

  // Leave enough space below the last row to scroll it to the top.
  const int maxY = showVertical ? treeHeight-std::min(height, pageHeight) : 0;
  const int maxX = showHorizontal ? treeWidth-width : 0;
  auto configure = [height](ScrollBar& bar, int end, int page) {
    bar.setRange(0, end);
    bar.setSmallStep(height);
    bar.setLargeStep(std::max(1, page));
    if(end>0)
      bar.setCentralButtonSize(int(int64_t(bar.centralAreaSize())*page/(int64_t(end)+page)));
    bar.update();
    };
  configure(verticalBar, maxY, pageHeight);
  configure(horizontalBar, maxX, width);
  }

void VobTreeView::resizeEvent(SizeEvent& e) {
  updateScrollBars();
  Widget::resizeEvent(e);
  }

void VobTreeView::scrollChanged(int) {
  pressed = nullptr;
  update();
  }

const VobTreeView::Row* VobTreeView::rowAt(Point point) const {
  const auto area = viewport();
  if(point.x<0 || point.x>=area.w || point.y<0 || point.y>=area.h)
    return nullptr;

  const size_t row = (size_t(verticalBar.value()) + size_t(point.y))/size_t(rowHeight());
  return row<rows.size() ? &rows[row] : nullptr;
  }

bool VobTreeView::isArrowHit(const Row& row, Point point) const {
  if(!row.hasVisibleChildren)
    return false;

  const int y = int(&row-rows.data())*rowHeight()-verticalBar.value();
  return arrowRect(row, y, arrowSprite(row)).contains(point);
  }

const Sprite& VobTreeView::arrowSprite(const Row& row) const {
  const auto& assets = Assets::inst();
  const auto& icon   = isCollapsed(row.vob) ? assets.ic.down : assets.ic.up;
  const int   height = rowHeight();
  return icon.sprite(height, height, Icon::ST_Normal);
  }

Rect VobTreeView::arrowRect(const Row& row, int y, const Sprite& arrow) const {
  const int x = rowPadding + int(row.depth)*arrow.w() - horizontalBar.value();
  return Rect(x, y+(rowHeight()-arrow.h())/2, arrow.w(), arrow.h());
  }

void VobTreeView::mouseDownEvent(MouseEvent& e) {
  if(e.button!=Event::ButtonLeft) {
    e.ignore();
    return;
    }

  pressed = rowAt(e.pos());
  if(pressed==nullptr) {
    e.ignore();
    return;
    }

  mouse        = e.pos();
  pressedArrow = isArrowHit(*pressed, mouse);
  e.accept();
  update();
  }

void VobTreeView::mouseDoubleClickEvent(MouseEvent& e) {
  mouseDownEvent(e);
  if(e.isAccepted() && pressed!=nullptr && !pressedArrow && pressed->vob==selected)
    onVobDoubleClicked();
  }

void VobTreeView::mouseUpEvent(MouseEvent& e) {
  if(e.button!=Event::ButtonLeft || pressed==nullptr) {
    e.ignore();
    return;
    }

  const auto* row = pressed;
  pressed = nullptr;
  mouse   = e.pos();
  e.accept();
  update();
  if(row!=rowAt(mouse) || pressedArrow!=isArrowHit(*row, mouse))
    return;

  auto* vob = row->vob;
  if(pressedArrow) {
    auto& branches = isFiltering() ? filteredCollapsed : collapsed;
    if(branches.erase(vob)==0)
      branches.insert(vob);
    updateView();
    } else if(vob->get()!=nullptr) {
    onVobSelected(vob);
    }
  }

void VobTreeView::mouseMoveEvent(MouseEvent& e) {
  mouse = e.pos();
  e.accept();
  update();
  }

void VobTreeView::mouseLeaveEvent(MouseEvent&) {
  mouse = {-1, -1};
  update();
  }

void VobTreeView::mouseWheelEvent(MouseEvent& e) {
  const bool horizontal = !verticalBar.isVisible() || (e.modifier & Event::M_Shift)!=0;
  auto&      bar        = horizontal ? horizontalBar : verticalBar;
  auto&      remainder  = horizontal ? horizontalWheelRemainder : verticalWheelRemainder;
  if(e.delta==0 || bar.maxValue()==0) {
    e.ignore();
    return;
    }

  constexpr int wheelUnitsPerNotch = 120;
  constexpr int pixelsPerNotch     = 48;
  const int64_t position           = int64_t(bar.value())*wheelUnitsPerNotch + remainder;
  const int64_t limit              = int64_t(bar.maxValue())*wheelUnitsPerNotch;
  const int64_t next               = std::clamp(position-int64_t(e.delta)*pixelsPerNotch, int64_t(0), limit);

  remainder = int(next%wheelUnitsPerNotch);
  bar.setValue(int(next/wheelUnitsPerNotch));
  e.accept();
  }

void VobTreeView::paintEvent(PaintEvent& e) {
  Painter p(e);
  const auto area = viewport();
  const auto clip = p.scissor().intersected(area);
  if(clip.w<=0 || clip.h<=0)
    return;
  p.setScissor(clip);

  const int    height  = rowHeight();
  const int    offset  = verticalBar.value();
  const auto*  hovered = rowAt(mouse);
  const size_t first   = (size_t(offset)+size_t(clip.y))/size_t(height);
  const size_t last    = std::min(rows.size(), (size_t(offset)+size_t(clip.y+clip.h-1))/size_t(height)+1);

  for(size_t i=first; i<last; ++i) {
    const int y = int(i)*height-offset;
    paintRow(p, rows[i], Rect(0, y, area.w, height), &rows[i]==hovered);
    }
  }

void VobTreeView::paintRow(Painter& p, const Row& row, const Rect& rect, bool hovered) const {
  const bool active = row.vob==selected;

  auto st     = state();
  st.button   = active ? WidgetState::T_PushButton : WidgetState::T_FlatButton;
  st.moveOver = hovered;
  st.pressed  = hovered && &row==pressed;
  style().draw(p, static_cast<Button*>(nullptr), Style::E_Background, st, rect, Style::Extra(*this));

  const auto& assets = Assets::inst();
  const auto& arrow  = arrowSprite(row);
  const auto  bounds = arrowRect(row, rect.y, arrow);
  int         x      = bounds.x;
  if(row.hasVisibleChildren) {
    p.setBrush(arrow);
    p.drawRect(bounds.x, bounds.y, bounds.w, bounds.h);
    x += bounds.w+arrowLabelGap;
    }

  const auto label    = row.label();
  const bool fallback = row.vob->get()!=nullptr && row.vobName().empty();

  auto font = assets.fntSmall;
  font.setBold(active);
  p.setFont(font);
  p.setBrush(fallback ? Color(0.8f) : style().colors().text);
  const int textHeight = font.textSize(label).h;
  p.drawText(x, rect.y+rect.h-(rect.h-textHeight)/2, label);
  }

std::string_view VobTreeView::Row::label() const {
  const auto* object = vob->get();
  if(object==nullptr)
    return "LEVEL";
  if(const auto name = vobName(); !name.empty())
    return name;
  if(!object->visual_name.empty())
    return object->visual_name;
  if(const auto* cls = VobReflection::find(object->type))
    return cls->name;
  return "Unknown Vob";
  }

std::string_view VobTreeView::Row::vobName() const {
  const auto* object = vob->get();
  return object!=nullptr ? std::string_view(object->vob_name) : std::string_view{};
  }
