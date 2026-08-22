#include "menubar.h"

#include <Tempest/Menu>
#include <Tempest/Painter>

#include "ui/rootview.h"
#include "ui/uihelper.h"
// #include "ui/uistyle.h"
#include "editorsettings.h"
#include "assets/assets.h"

using namespace Tempest;

struct MenuBar::Menu : Tempest::Menu {
  using Tempest::Menu::Menu;

  Widget* createItem(const Menu::Item &decl) override {
    ItemButton* b = new ItemButton(decl.items);

    b->setText(decl.text);
    b->setExtraText(decl.text2);
    b->setIcon(decl.icon);

    // b->setFont(Resources::res().fntSmall);
    // b->setExtraFont(Resources::res().fntSmall);

    b->onClick.bind(&decl.activated,&Signal<void()>::operator());
    b->onClick.bind(this,&Menu::close);
    b->onMouseEnter.bind(this,&Menu::openSubMenu);

    b->setMinimumSize(0,30);
    b->setMargins(Margin(16,16,8,8));

    return b;
    }
  };

MenuBar::MenuBar() {
  using namespace UiHelper;

  setMaximumSize(maxSize().w,27);
  setLayout(Horizontal);

  file = &addWidget(btn("File"));
  edit = &addWidget(btn("Edit"));
  more = &addWidget(toolBtn(Assets::inst().ic.more));
  addWidget(new Widget());
  //addWidget(toolBtn(Assets::inst().ic.settings));

  file->setSizePolicy(Fixed,Preferred);
  edit->setSizePolicy(Fixed,Preferred);

  file->setFont(Assets::inst().fntSmall);
  edit->setFont(Assets::inst().fntSmall);

  file->setTextColor(Color(1,1,1,1));
  edit->setTextColor(Color(1,1,1,1));

  file->onClick.bind(this,&MenuBar::onFileMenu);
  edit->onClick.bind(this,&MenuBar::onEditMenu);
  more->onClick.bind(this,&MenuBar::onExtraMenu);

  auto sz = UiHelper::wrapContent(*this,Horizontal);
  setSizeHint(sz,margins());
  setSizePolicy(Fixed,Preferred);
  }

void MenuBar::paintEvent(PaintEvent& e) {
  Painter p(e);
  p.setBrush(Assets::inst().colors.workspaceD);
  p.drawRect(0,0,w(),h());
  }

void MenuBar::onFileMenu() {
  auto oid = EditorSettings::inst().shortcut(EditorSettings::S_Open);
  auto nid = EditorSettings::inst().shortcut(EditorSettings::S_New);
  auto sid = EditorSettings::inst().shortcut(EditorSettings::S_Save);

  Menu::Declarator decl;
  decl.item(Icon(), "Open project", oid.toString(), this, &MenuBar::onOpenProj);
  decl.item(Icon(), "New item",     nid.toString(), this, &MenuBar::onNewItem);
  decl.item(Icon(), "Save",         sid.toString(), this, &MenuBar::onSave);

  Menu m(std::move(decl));
  m.setMinimumWidth(200);
  m.exec(*file,Point(0,file->h()),true);
  }

void MenuBar::onEditMenu() {
  auto uid = EditorSettings::inst().shortcut(EditorSettings::S_Undo);
  auto rid = EditorSettings::inst().shortcut(EditorSettings::S_Redo);

  Menu::Declarator decl;
  decl.item(Icon(), "Undo", uid.toString(), this, &MenuBar::onUndo);
  decl.item(Icon(), "Redo", rid.toString(), this, &MenuBar::onRedo);

  Menu m(std::move(decl));
  m.setMinimumWidth(200);
  m.exec(*edit,Point(0,edit->h()),true);
  }

void MenuBar::onExtraMenu() {
  /*
  Menu::Declarator decl;
  decl.item(Icon(), "Settings",   "", &RootView::inst(), &RootView::onSettings);
  decl.item(Icon(), "Help",     "F1", &RootView::inst(), &RootView::onHelp);

  Menu m(std::move(decl));
  m.setMinimumWidth(200);
  m.exec(*more,Point(0,more->h()),true);
  */
  }

void MenuBar::onNewItem() {
  RootView::inst().onNewFile();
  }

void MenuBar::onOpenProj() {
  RootView::inst().onOpenProject();
  }

void MenuBar::onSave() {
  RootView::inst().onSave();
  }

void MenuBar::onUndo() {
  RootView::inst().onUndo();
  }

void MenuBar::onRedo() {
  RootView::inst().onRedo();
  }
