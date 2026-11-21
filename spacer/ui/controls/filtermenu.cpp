#include "filtermenu.h"

#include <Tempest/Application>
#include <Tempest/CheckBox>
#include <Tempest/Label>
#include <Tempest/Painter>
#include <Tempest/ListView>

#include <algorithm>
#include <array>

#include "assets.h"
#include "objects/vobreflection.h"
#include "ui/uihelper.h"
#include "ui/uistyle.h"
#include "ui/vobtreeview.h"

using namespace Tempest;

class FilterMenu::MenuStyle final : public UiStyle {
  public:
    void draw(Painter& p, Panel*, Element, const WidgetState&, const Rect& r, const Extra&) const override {
      p.setBrush(Brush(Assets::inst().colors.workspaceD, Painter::NoBlend));
      p.drawRect(r.x, r.y, r.w, r.h);
      }

    void draw(Painter& p, CheckBox* box, Element e, const WidgetState& state,
              const Rect& r, const Extra& extra) const override {
      auto row = state;
      row.button = state.checked==WidgetState::Checked ? Button::T_PushButton : Button::T_FlatButton;
      UiStyle::draw(p, static_cast<Button*>(box), e, row, r, extra);

      auto marker = state;
      marker.moveOver = false;
      UiStyle::draw(p, box, e, marker, r, extra);
      }
  };

class FilterMenu::ClassListDelegate final : public ListDelegate {
  public:
    static constexpr int rowHeight = 30;

    explicit ClassListDelegate(FilterMenu& menu) : menu(menu) {
      menu.checkboxes.resize(classes.size());
      }

    size_t size() const override {
      return classes.size();
      }

    Widget* createView(size_t position) override {
      const auto type = classes[position];
      const auto* cls = VobReflection::find(type);
      auto checkbox = std::make_unique<CheckBox>();
      if(cls!=nullptr)
        checkbox->setText(cls->name);
      checkbox->setChecked(menu.tree.isClassFiltered(type));
      const int markerWidth = rowHeight + checkbox->spacing();
      checkbox->setMinimumSize(checkbox->sizeHint().w + markerWidth, rowHeight);
      checkbox->setSizePolicy(Preferred, Fixed);
      checkbox->onChecked.bind(&menu, &FilterMenu::updateFilters);
      menu.checkboxes[position] = {type, checkbox.get()};
      return checkbox.release();
      }

  private:
    using Type = VobReflection::Type;
    static constexpr std::array classes = {
      Type::zCVob,
      Type::oCMOB,
      Type::zCTrigger,
      Type::zCVobSound,
      Type::zCVobLight,
      Type::oCItem,
      Type::zCCSCamera,
      Type::oCZoneMusic,
      Type::zCZoneZFog,
      };

    FilterMenu& menu;
  };

class FilterMenu::Popup final : public UiOverlay {
  public:
    Popup(Widget& anchor, Widget* content)
      : anchor(anchor), panel(addWidget(content)), preferredSize(panel.size()) {
      setStyle(&anchor.style());
      }

    bool isOpen() const { return open; }

  private:
    void resizeEvent(SizeEvent&) override {
      const auto pos = anchor.mapToRoot(Point(anchor.w()-preferredSize.w, anchor.h()));
      const int x = std::clamp(pos.x, 0, w());
      const int y = std::clamp(pos.y, 0, std::max(0, h()-preferredSize.h));
      panel.setGeometry(x, y, std::min(preferredSize.w, w()-x), std::min(preferredSize.h, h()-y));
      panel.setFocus(true);
      }

    void mouseDownEvent(MouseEvent& e) override {
      if(panel.rect().contains(e.pos())) {
        e.accept();
        return;
        }

      open = false;
      if(Rect(0, 0, anchor.w(), anchor.h()).contains(e.pos()-anchor.mapToRoot(Point())))
        e.accept();
      else
        e.ignore();
      }

    Widget& anchor;
    Widget& panel;
    Size preferredSize;
    bool open = true;
  };

FilterMenu::FilterMenu(VobTreeView& tree) : Menu(Declarator()), tree(tree) {
  }

void FilterMenu::show(Widget& anchor) {
  checkboxes.clear();
  Popup popup(anchor, createDropList(anchor, false, {}));
  SystemApi::addOverlay(&popup);
  while(popup.isOpen() && Application::isRunning())
    Application::processEvents();
  }

Widget* FilterMenu::createItems(const std::vector<Item>&) {
  auto items = std::make_unique<Panel>();
  items->setStyle(new MenuStyle());
  items->setLayout(Vertical);
  items->setMargins(Margin(8, 8, 6, 8));
  items->setSpacing(6);

  auto& header = items->addWidget(new Widget());
  header.setLayout(Horizontal);
  header.setSizePolicy(Preferred, Fixed);
  header.setMinimumSize(0, ClassListDelegate::rowHeight);
  header.setMargins(Margin(8, 2, 0, 0));

  auto& title = header.addWidget(new Label());
  auto font = Assets::inst().fntSmall;
  font.setBold(true);
  title.setFont(font);
  title.setText("Filter by class");
  title.setSizePolicy(Expanding, Preferred);

  clearButton = &header.addWidget(new Button());
  clearButton->setText("Clear");
  clearButton->setButtonType(Button::T_FlatButton);
  clearButton->setMinimumSize(48, 0);
  clearButton->setSizePolicy(Fixed, Preferred);
  clearButton->setEnabled(tree.hasClassFilters());
  clearButton->onClick.bind(this, &FilterMenu::clearFilters);

  auto& list = items->addWidget(new ListView());
  list.centralWidget().setSpacing(0);
  list.setDelegate(new ClassListDelegate(*this));

  const auto headerSize = UiHelper::wrapContent(header, Horizontal);
  const auto listSize = UiHelper::wrapContent(list.centralWidget());
  const auto& margins = items->margins();
  items->resize(std::max(headerSize.w, std::max(listSize.w, 230)) + margins.xMargin(),
                std::max(headerSize.h, header.minSize().h) + listSize.h + items->spacing() + margins.yMargin());
  return items.release();
  }

void FilterMenu::clearFilters() {
  tree.clearClassFilters();
  clearButton->setEnabled(false);
  for(const auto& [type, checkbox] : checkboxes)
    checkbox->setChecked(false);
  }

void FilterMenu::updateFilters(bool) {
  for(const auto& [type, checkbox] : checkboxes) {
    if((checkbox->isChecked()==CheckBox::Checked)!=tree.isClassFiltered(type)) {
      tree.toggleClassFilter(type);
      clearButton->setEnabled(tree.hasClassFilters());
      return;
      }
    }
  }
