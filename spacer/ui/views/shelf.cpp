#include "shelf.h"

#include <Tempest/ListView>
#include <Tempest/Painter>
#include <Tempest/ScrollBar>
#include <functional>

#include "project/projectmgr.h"
#include "ui/projectitemview.h"
#include "ui/rootview.h"
#include "resources.h"

#include "assets.h"

using namespace Tempest;

struct Shelf::Item : ProjectItemView {
  enum {
    Marg    = 2,
    DefSize = 64,
    TextSz  = 16,

    Height  = DefSize+Marg*2+TextSz
    };

  Item(const ProjectItem& it, bool prettyName):ProjectItemView(it), prettyName(prettyName) {
    if(prettyName)
      setText(it.displayName()); else
      setText(it.name());
    setDepth(it.depth());
    resize(DefSize+Marg*2,Height);
    }

  void paintEvent(Tempest::PaintEvent& e) {
    if(isDedicatedPaint())
      return;
    Painter p(e);
    paint(p);
    }

  bool isDedicatedPaint() {
    return state().moveOver && !isDrag();
    }

  void paint(Painter& p) {
    p.setFont(Assets::inst().fntSmall);

    auto sz = p.font().textSize(w()-Marg*2, text());

    if(state().moveOver && !isDrag()) {
      p.setBrush(Color(0.27f,0.27f,0.27f,1));
      p.drawRect(Rect(0,0,w(),h()+(sz.h-TextSz)));
      p.setBrush(Color(1,1,1,1));
      } else {
      style().draw(p,static_cast<Button*>(nullptr),Style::E_Background,
                   state(),Rect(0,0,w(),h()+(sz.h-TextSz)),Style::Extra(*this));
      }

    if(it.type()==ProjectItem::T_Project) {
      drawIcon(p, Assets::inst().ic.file_project);
      }
    else if(it.type()==ProjectItem::T_Dir) {
      drawIcon(p, Assets::inst().ic.folder_large);
      }
    else {
      drawIcon(p, Assets::inst().ic.file_large);
      }
    p.drawText(Marg, Marg+DefSize+TextSz, w()-Marg*2, h()+(sz.h-TextSz), text(), AlignHCenter);
    }

  void drawIcon(Painter& p, const Icon& ic) {
    auto  sp = ic.sprite(w(),h(),Icon::ST_Normal);
    p.setBrush(sp);
    p.drawRect(Marg+(DefSize-sp.w())/2,Marg+(DefSize-sp.h())/2,sp.w(),sp.h());
    }

  void drawIcon(Painter& p, const Texture2d& sp) {
    p.setBrush(sp);
    p.drawRect(Marg+(DefSize-sp.w())/2,Marg+(DefSize-sp.h())/2,sp.w(),sp.h());
    }

  void updateDisplay() {
    if(!prettyName)
      return;
    setText(it.displayName());
    }

  bool prettyName = false;
  };

struct Shelf::Central : Widget {
  public:
    struct Layout : Tempest::Layout {
      Layout(Central& owner):owner(owner){};
      void applyLayout() override {
        owner.doLayout();
        }
      Central& owner;
      };

    Central(Shelf& owner):owner(owner) {
      setLayout(new Layout(*this));
      setMargins(Margin(4,4,4,4));
      // ProjectMgr::inst().onProjectChange.bind(this,&Central::refreshProject);
      // ProjectMgr::inst().onFilesysChange.bind(this,&Central::refreshFileTree);
      // ProjectMgr::inst().onAssetReady   .bind(this,&Central::updateDisplay);
      // ProjectMgr::inst().onCompiled     .bind(this,&Central::updatePriview);
      setCategory(0);
      index();
      }

    void setCategory(size_t id) {
      switch(id) {
        case 0:
          filterFn = [](const ProjectItem&)    { return true; };
          break;
        default:
          filterFn = [](const ProjectItem&)    { return true; };
          break;
        }
      index();
      }

    void updatePriview() {
      update();
      updateDisplay();
      }

    void updateDisplay() {
      for(auto& i:items)
        i->updateDisplay();
      }

    void refreshProject() {
      owner.subdir.clear();
      refreshFileTree();
      }

    void refreshFileTree() {
      while(owner.subdir.size()>0) {
        auto& it = owner.subdir.back();
        if(Resources::vdfsIndex().resolve(it.path())==nullptr) {
          owner.subdir.pop_back();
          continue;
          }
        break;
        }
      if(owner.subdir.size()==0)
        owner.subdir.push_back(ProjectMgr::inst().vdf(0));
      owner.path->invalidateView();
      index();
      }

    void doLayout() {
      if(lockLayout)
        return;
      lockLayout = true;
      Point p = {margins().left,margins().top};
      int   h = 0;
      const int width = this->w();

      for(auto i:items) {
        if(h>0 && p.x+i->w()+margins().right>width) {
          p.x  = margins().left;
          p.y += h + spacing();
          h = 0;
          }
        i->setPosition(p);
        p.x += i->w()+spacing();
        h = std::max(h,i->h());
        }
      resize(width,h+p.y);
      lockLayout = false;
      }

  private:
    void dispatchPaintEvent(PaintEvent& e) override {
      Widget::dispatchPaintEvent(e);
      for(auto& i:items)
        if(i->isDedicatedPaint()) {
          Painter p(e);
          p.setScissor(-x(),-y(),owner.w(),owner.h());
          p.translate(i->pos());
          i->paint(p);
          }
      }

    void clear() {
      for(auto i:items)
        delete i;
      items.clear();
      }

    ProjectItem currentFolder() const {
      if(owner.subdir.size()<=1)
        return ProjectMgr::inst().vdf(0);
      auto&  pro = ProjectMgr::inst();
      return owner.subdir.back();
      }

    size_t currentFolderDepth() const {
      if(owner.subdir.size()<=1)
        return 0; //root
      return owner.subdir.back().depth()+1;
      }

    void index() {
      lockLayout = true;
      clear();

      auto&  pro = ProjectMgr::inst();
      size_t id  = 0;
      if(proj<ProjectMgr::inst().vdfCount()) {
        const auto folder = currentFolder();
        for(size_t i=0; i<folder.itemsCount(); ++i) {
          auto itm = folder.item(i);
          if(owner.showByType) {
            if(filterFn(itm)) {
              auto& w = addWidget(new Item(itm,owner.showByType));
              w.onClick.bind(&owner,&Shelf::onItem);
              items.push_back(&w);
              }
            }
          else if(filterFn(itm)) {
            auto& w = addWidget(new Item(itm,owner.showByType));
            w.onClick.bind(&owner,&Shelf::onItem);
            items.push_back(&w);
            }
          }
        }

      lockLayout = false;
      doLayout();
      }

    void resizeEvent(Tempest::SizeEvent&) override {
      doLayout();
      }

    Shelf&              owner;
    std::vector<Item*>  items;
    size_t              proj       = 0;
    bool                lockLayout = false;

    std::function<bool(const ProjectItem& it)> filterFn;
  };

struct Shelf::Box : Widget {
  void paintEvent(Tempest::PaintEvent &e) override {
    Painter p(e);
    p.setBrush(Assets::inst().colors.workspaceD);
    p.drawRect(0,0,w(),h());
    }
  };

struct Shelf::Row : Tempest::Button {
  Row(Shelf& owner, std::string_view title, size_t pos) : owner(owner), pos(pos) {
    setFont(Assets::inst().fntSmall);
    setText(title);
    setButtonType(Button::T_FlatButton);
    }

  void paintEvent(Tempest::PaintEvent &e) override {
    Button::paintEvent(e);
    Tempest::Painter p(e);

    if(owner.categoryId==pos) {
      p.setPen(Assets::inst().colors.highlight);
      p.drawLine(0,0,w(),0);
      p.drawLine(0,h()-1,w(),h()-1);
      p.drawLine(0,1,0,h()-1);
      p.drawLine(w()-1,1,w()-1,h()-1);
      }
    }

  void emitClick() override {
    owner.setCategory(pos);
    }

  Shelf& owner;
  size_t pos = 0;
  };

struct Shelf::PathItem : Tempest::Button {
  PathItem(Shelf& owner, size_t pos):owner(owner), pos(pos) {
    setFont(Assets::inst().fntSmall);
    setButtonType(Button::T_FlatButton);
    setSizePolicy(Fixed,Fixed);
    }

  void emitClick() override {
    owner.setSubdir(pos);
    }

  Shelf& owner;
  size_t pos = 0;
  };

struct Shelf::Delegate : Tempest::ListDelegate {
  std::string_view txt[4] = {
    "Vdf",
    "Meshes",
    "Textures",
    "Scripts",
    };

  Delegate(Shelf& owner):owner(owner){}
  size_t size() const override {
    return std::extent<decltype(txt)>::value;
    }
  Widget* createView(size_t i) override {
    return new Row(owner, txt[i], i);
    }
  Shelf& owner;
  };

struct Shelf::PathDelegate : Tempest::ListDelegate {
  PathDelegate(Shelf& owner):owner(owner){}
  size_t size() const override {
    return owner.subdir.size();
    }
  Widget* createView(size_t i) override {
    auto&  itm =  owner.subdir[i];

    auto b = new PathItem(owner,i);
    b->setText(itm.name());
    return b;
    }
  Shelf& owner;
  };

Shelf::Shelf() {
  subdir.push_back(ProjectMgr::inst().vdf(0));

  category    = &addWidget(new ListView());
  auto& right =  addWidget(new Widget());

  auto& top = right.addWidget(new Widget());
  auto  fnt = Assets::inst().fntSmall;
  fnt.setBold(true);

  addItm = &top.addWidget(new Button());
  addItm->setFont(fnt);
  addItm->setText("Add new");
  addItm->setSizePolicy(Fixed,Fixed);
  addItm->onClick.bind(&RootView::inst(), &RootView::onNewFile);

  path = &top.addWidget(new ListView(Horizontal));
  path->setDelegate(new PathDelegate(*this));

  top.setMinimumSize(0,27);
  top.setSizePolicy(Preferred,Fixed);
  top.setLayout(Horizontal);

  box      = &right.addWidget(new Box());
  cen      = &box->addWidget(new Central(*this));
  scroll   = &box->addWidget(new ScrollBar());

  category->setDelegate(new Delegate(*this));
  scroll->onValueChanged.bind(this,&Shelf::onScroll);

  setMargins(Margin(4,4,0,4));
  box->setLayout(Horizontal);
  right.setLayout(Vertical);
  setLayout(Horizontal);

  setCategory(0);
  }

void Shelf::onItem(Widget*, const ProjectItem& itm) {
  if(itm.type()==ProjectItem::T_Dir) {
    subdir.push_back(itm);
    cen->refreshFileTree();
    path->invalidateView();
    } else {
    onItemSelected(itm);
    }
  }

void Shelf::resizeEvent(SizeEvent&) {
  if(w()<h()) {
    category->setMinimumSize(0,34);
    category->setSizePolicy(Preferred,Fixed);
    category->setLayout(Horizontal);
    setLayout(Vertical);
    } else {
    category->setMinimumSize(100,0);
    category->setSizePolicy(Fixed,Preferred);
    category->setLayout(Vertical);
    setLayout(Horizontal);
    }
  scroll->setVisible(true);
  cen->doLayout();
  if(cen->h()<box->h()) {
    scroll->setVisible(false);
    cen->doLayout();
    scroll->setValue(0);
    scroll->setRange(0,0);
    return;
    }
  scroll->setRange(0,cen->h()-Item::Height);
  }

void Shelf::mouseDownEvent(MouseEvent& e) {
  e.accept();
  }

void Shelf::mouseUpEvent(MouseEvent& e) {
  if(e.button==Event::ButtonBack) {
    if(subdir.size()>1)
      setSubdir(subdir.size()-2);
    }
  }

void Shelf::mouseWheelEvent(MouseEvent& e) {
  scroll->setValue(scroll->value()-e.delta);
  }

void Shelf::onScroll(int v) {
  cen->setPosition(0,-v);
  }

void Shelf::setCategory(size_t id) {
  if(categoryId==id)
    return;
  categoryId   = id;
  showByType = (categoryId!=0);

  addItm ->setVisible(id==0);

  subdir.resize(1);
  cen->setCategory(id);
  cen->refreshFileTree();
  path->invalidateView();
  }

void Shelf::setSubdir(size_t id) {
  if(subdir.size()==id+1)
    return;
  subdir.resize(id+1);
  cen->refreshFileTree();
  path->invalidateView();
  }

size_t Shelf::findRootItem() const {
  return 0;
  }
