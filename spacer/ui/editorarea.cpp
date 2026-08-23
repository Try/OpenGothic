#include "editorarea.h"

#include <Tempest/Painter>
#include <Tempest/Button>
#include <Tempest/Label>
#include <Tempest/Log>

#include "ui/editors/baseeditor.h"
#include "ui/editors/worldeditor.h"
#include "ui/views/projecttree.h"
#include "ui/views/menubar.h"
#include "ui/dialogs/questionbox.h"
#include "ui/rootview.h"

#include "ui/resizablearea.h"
#include "ui/tabs.h"

#include "editorsettings.h"

#include "toolgroup.h"
#include "toolwindow.h"
#include "assets/assets.h"

using namespace Tempest;

struct EditorArea::EditorWrapper : public Tempest::Widget {
  EditorWrapper(BaseEditor* ed):edit(ed) {
    setLayout(Horizontal);

    //item.projectSettings(); // fetch settings
    //edit->preload(item);

    for(size_t i=0; i<ToolWindow::T_Count; ++i)
      tool[i].reset(edit->createToolpanel(ToolWindow::Tool(i)));
    }

  void paintEvent(Tempest::PaintEvent &e) override {
    if(widgetsCount()==0) {
      /*
      Painter p(e);
      const uint64_t period = 3000;
      const int sz = 32;
      p.setBrush(Resources::res().loadIndicator);
      p.translate(w()/2,h()/2);
      p.rotate(360.f*float(Application::tickCount()%period)/period);
      p.drawRect(-sz,-sz,2*sz,2*sz,
                 0,0,p.brush().w(),p.brush().h());
      */
      update();
      }
    }

  bool pokeItem() {
    if(loaded)
      return true;
    /*
    if(!item.isReady())
      return false;
    if(item.projectSettings()==nullptr)
      return false;
    Log::d("open: ",item.name());
    if(edit->load(item))
      Log::d("open: ",item.name()," - OK"); else
      Log::d("open: ",item.name()," - FAILED");
    */
    addWidget(edit.get());
    loaded = true;
    return true;
    }

  void undo() {
    if(edit!=nullptr)
      edit->undo();
    }

  void redo() {
    if(edit!=nullptr)
      edit->redo();
    }

  void save() {
    if(edit!=nullptr)
      edit->save();
    }

  void discardChanges() {
    // item.discardChanges();
    }

  void setEditorVisibility(bool v) {
    Widget::setVisible(v);
    for(auto& i:tool)
      if(i!=nullptr)
        i->setVisible(v);
    }

  // ProjectItem                 item;
  std::unique_ptr<BaseEditor> edit;
  std::unique_ptr<Widget>     tool[ToolWindow::T_Count];

  bool                        loaded=false;
  };

struct EditorArea::ToolArea : public ResizableArea {
  ToolArea(EditorArea& owner, BaseEditor::ToolType type) : ResizableArea(Vertical), owner(owner), type(type) {
    if(type==BaseEditor::ToolType::Left || type==BaseEditor::ToolType::Right) {
      setOrientation(Vertical);
      setSizePolicy(Fixed,Preferred);
      } else {
      setOrientation(Horizontal);
      setSizePolicy(Preferred,Fixed);
      }

    switch(type) {
      case BaseEditor::ToolType::Bottom:
        setSizeHint(Size(0,350));
        break;
      case BaseEditor::ToolType::Right:
        setSizeHint(Size(250,0));
        break;
      default:
        break;
      }

    resize(sizeHint());
    setMargins(Margin(0,0,0,0));
    setMinimumSize(100,100);
    }

  void   paintEvent(Tempest::PaintEvent &e) override {
    ResizableArea::paintEvent(e);
    }

  void dispatchPaintEvent(PaintEvent& e) override {
    paintEvent(e);
    paintNested(e);

    Tempest::Painter p(e);
    if(owner.aboutToDrop.area==this)
      drawDropHit(p);
    }

  void   drawDropHit(Painter& p, const Rect& r) {
    p.drawRect(r);

    p.translate(r.pos());
    p.drawLine(0,  0,r.w-1,0);
    p.drawLine(0,  0,    0,r.h-1);
    p.drawLine(0,  0,    0,r.h-1);
    p.drawLine(r.w-1,    0,r.w-1,r.h);
    p.translate(-r.pos());
    }

  void   drawDropHit(Painter& p) {
    auto cl = Assets::inst().colors.highlight;
    cl.set(cl.r(),cl.g(),cl.b(),0.3f);
    p.setBrush(cl);
    p.setPen(Assets::inst().colors.highlight);

    size_t count = 0;
    for(size_t i=0; i<widgetsCount(); ++i)
      if(widget(i).isVisible())
        count++;
    if(count==0) {
      drawDropHit(p,Rect(0,0,w(),h()));
      return;
      }

    size_t dropP = owner.aboutToDrop.pos;
    if(owner.aboutToDrop.merge && dropP<widgetsCount()) {
      auto r = widget(dropP).rect();
      r.x--;
      r.y--;
      r.w+=2;
      r.h+=2;
      drawDropHit(p,r);
      }
    else if(dropP<widgetsCount()) {
      if(orientation()==Horizontal) {
        int x = widget(dropP).x();
        p.drawRect(x-spacing(),0,spacing(),h());
        } else {
        int y = widget(dropP).y();
        p.drawRect(0,y-spacing(),w(),spacing());
        }
      }
    else if(dropP==widgetsCount()) {
      if(orientation()==Horizontal) {
        int x = w();
        p.drawRect(x-spacing(),0,spacing(),h());
        } else {
        int y = h();
        p.drawRect(0,y-spacing(),w(),spacing());
        }
      }
    }

  void   resizeEvent(Tempest::SizeEvent& e) override {
    ResizableArea::resizeEvent(e);

    auto sp = sizePolicy();
    if(sp.typeH==Fixed) {
      setSizeHint(w(),sizeHint().h);
      }
    if(sp.typeV==Fixed) {
      setSizeHint(sizeHint().w,h());
      }
    }

  size_t coordToDropPoint(const Point& p, bool& merge) {
    merge = false;
    if(orientation()==Horizontal) {
      if(p.x<DragPadding)
        return 0;
      for(size_t i=0; i<widgetsCount(); ++i) {
        auto r = widget(i).rect();
        if(p.x<=r.x+DragPadding)
          return i;
        if(r.x+DragPadding<p.x && p.x<r.x+r.w-DragPadding) {
          merge = true;
          return i;
          }
        }
      } else {
      if(p.y<DragPadding)
        return 0;
      for(size_t i=0; i<widgetsCount(); ++i) {
        if(!widget(i).isVisible())
          continue;
        auto r = widget(i).rect();
        if(p.y<=r.y+DragPadding)
          return i;
        if(r.y+DragPadding<p.y && p.y<r.y+r.h-DragPadding) {
          merge = true;
          return i;
          }
        }
      }
    return widgetsCount();
    }

  bool   dropWidget(ToolGroup* it, size_t at, bool merge) {
    if(at<widgetsCount() && merge) {
      if(auto dest = dynamic_cast<ToolGroup*>(&widget(at))) {
        dest->moveContent(*it);
        return true;
        }
      }
    addWidget(it,at);
    return false;
    }

  void invalidate() {
    for(size_t i=0; i<widgetsCount(); ++i)
      if(auto g = dynamic_cast<ToolGroup*>(&widget(i)))
        g->invalidate();
    bool v = owner.aboutToDrop.area==this;
    for(size_t i=0; i<widgetsCount(); ++i)
      v |= widget(i).isVisible();
    setVisible(v);
    }

  void saveUiLayout() {
    for(size_t i=0; i<widgetsCount(); ++i)
      if(auto g = dynamic_cast<ToolGroup*>(&widget(i)))
        g->saveUiLayout(type,i);
    auto w = this->weights();
    EditorSettings::inst().setToolWeight(w,type);
    }

  EditorArea&          owner;
  BaseEditor::ToolType type = BaseEditor::ToolType::Count;
  };

struct EditorArea::TopBar : public Widget {
  TopBar() {
    setLayout(Horizontal);
    setMinimumSize(0,27);
    setSizePolicy(Preferred,Fixed);
    setMargins(Margin(8,0,0,0));
    setSpacing(16);
    }
  void mouseDownEvent(Tempest::MouseEvent& e) {
    e.accept();
    }
  void mouseMoveEvent(Tempest::MouseEvent& e) {
    e.accept();
    }

  void paintEvent(PaintEvent& e) {
    Painter p(e);
    p.setBrush(Assets::inst().colors.workspaceD);
    p.drawRect(0,0,w(),h());
    }
  };

EditorArea::EditorArea() {
  setLayout(Horizontal);
  setSpacing(0);

  main       = &addWidget(new ResizableArea(Horizontal));
  areaL      = &main->addWidget(new ToolArea(*this,BaseEditor::ToolType::Left));
  auto& mid  = main ->addWidget(new Widget());
  areaR      = &main->addWidget(new ToolArea(*this,BaseEditor::ToolType::Right));

  auto& top  = mid.addWidget(new TopBar());
  top.addWidget(new MenuBar());
  tabs       = &top.addWidget(new Tabs(*this));
  areaM      = &mid .addWidget(new ResizableArea(Vertical));
  central    = &areaM->addWidget(new Widget());
  areaB      = &areaM->addWidget(new ToolArea(*this,BaseEditor::ToolType::Bottom));

  tabs->onClicked.bind(this,&EditorArea::showEditor);
  tabs->onClose  .bind(this,&EditorArea::closeEditor);

  areaR->setVisible(false);
  areaB->setVisible(false);

  mid.setSpacing(0);
  mid.setLayout(Vertical);

  central->setLayout(Horizontal);

  for(size_t i=0; i<ToolWindow::T_Count; ++i) {
    auto t = EditorSettings::inst().toolPosition(ToolWindow::Tool(i));
    Widget* dest = nullptr;
    switch(t.parent) {
      case BaseEditor::ToolType::Left:
        dest = areaL;
        break;
      case BaseEditor::ToolType::Right:
        dest = areaR;
        break;
      case BaseEditor::ToolType::Bottom:
      case BaseEditor::ToolType::Count:
        dest = areaB;
        break;
      }
    while(dest->widgetsCount()<=t.group)
      dest->addWidget(new ToolGroup());

    auto& g = reinterpret_cast<ToolGroup&>(dest->widget(t.group));
    window[i] = &g.add(new ToolWindow(ToolWindow::Tool(i)));
    // window[i]->setOrder(t.order);
    }

  main ->setWeights(EditorSettings::inst().rootWeights());
  areaM->setWeights(EditorSettings::inst().midWeights());
  areaL->setWeights(EditorSettings::inst().leftWeights());
  areaR->setWeights(EditorSettings::inst().rightWeights());
  areaB->setWeights(EditorSettings::inst().bottomWeights());

  projTree = &window[ToolWindow::T_ProjectTree]->addWidget(new ProjectTree());
  projTree->onFile.bind(this,&EditorArea::openFile);

  loadTimer.timeout.bind(this,&EditorArea::pokeLoading);
  // ProjectMgr::inst().onFilesysChange.bind(this,&EditorArea::onFilesysChange);
  onFilesysChange(); // adjust to current fs state

  main ->onResizeFinished.bind(this,&EditorArea::saveUiLayout);
  areaM->onResizeFinished.bind(this,&EditorArea::saveUiLayout);
  areaL->onResizeFinished.bind(this,&EditorArea::saveUiLayout);
  areaR->onResizeFinished.bind(this,&EditorArea::saveUiLayout);
  areaB->onResizeFinished.bind(this,&EditorArea::saveUiLayout);
  }

EditorArea::~EditorArea() {
  auto e = std::move(editor);
  for(auto i:e)
    delete i;
  }

void EditorArea::load() {
  implLoad<WorldEditor>();
  }

template<class T>
void EditorArea::implLoad() {
  /*
  for(size_t i=0;i<editor.size();++i) {
    if(editor[i]->item==p) {
      showEditor(i);
      return;
      }
    }
  */

  EditorWrapper* ed = &central->addWidget(new EditorWrapper(new T()));
  editor.emplace_back(ed);
  ed->edit->invalidateTab.bind(tabs,&Tabs::invalidate);

  for(size_t i=0;i<ToolWindow::T_Count;++i) {
    auto w = ed->tool[i].get();
    if(w==nullptr)
      continue;
    window[i]->addWidget(w);
    }

  updateOpenFileList();
  loadTimer.start(1);

  showEditor(editor.size()-1);
  }

void EditorArea::save() {
  if(auto e = currentEditor())
    e->save();
  }

void EditorArea::undo() {
  if(auto e = currentEditor())
    e->undo();
  }

void EditorArea::redo() {
  if(auto e = currentEditor())
    e->redo();
  }

bool EditorArea::closeApp() {
  int            count = 0;
  EditorWrapper* last  = nullptr;
  for(auto& i:editor) {
    if(i->edit->hasUnsavedChanges()) {
      count++;
      last = i;
      }
    }

  if(count==0)
    return false;

  char buf[256]={};
  if(false && count==1) {
    // std::snprintf(buf,sizeof(buf),"Save changes to \"%s\"?", last->item.name().c_str());
    } else {
    std::snprintf(buf,sizeof(buf),"Save changes to project?");
    }

  switch(QuestionBox::ask(buf,QuestionBox::R_No|QuestionBox::R_Yes|QuestionBox::R_Cancel)) {
    case QuestionBox::R_Cancel:
      return true;
    case QuestionBox::R_Yes:
      for(auto& i:editor) {
        i->save();
        }
      break;
    default:
      break;
    }
  return false;
  }

void EditorArea::openApp() {
  /*
  auto& p = ProjectMgr::inst();
  for(size_t i=0; i<p.projsCount(); ++i) {
    if(!p.proj(i).isVisible())
      continue;
    if(auto s = p.proj(i).settings()) {
      auto files = s->openFiles();
      for(auto& i:files) {
        auto f = ProjectMgr::inst().resolveIoPath(i);
        load(f);
        }
      }
    return;
    }
    */
  }

void EditorArea::closeAll() {
  auto e = std::move(editor);
  editor.clear();
  for(auto i:e)
    delete i;
  }

std::string_view EditorArea::editorTitle(size_t i) const {
  return editor[i]->edit->title();
  }

bool EditorArea::hasUnsavedChanges(size_t i) const {
  return editor[i]->edit->hasUnsavedChanges();
  }

void EditorArea::pokeLoading() {
  bool ready = true;
  for(auto& i:editor)
    ready &= i->pokeItem();
  updateOpenFileList();
  if(ready)
    loadTimer.stop();
  }

void EditorArea::showEditor(size_t id) {
  for(size_t i=0;i<editor.size();++i) {
    auto& ed = editor[i];
    ed->setEditorVisibility(id==i);
    }
  tabs->setSelection(id);
  invalidateTools();
  }

void EditorArea::closeEditor(size_t i) {
  auto ed = editor[i];

  if(ed->edit->hasUnsavedChanges()) {
    char buf[256]={};
    // std::snprintf(buf,sizeof(buf),"Save changes to \"%s\"?", ed->item.name().c_str());
    std::snprintf(buf,sizeof(buf),"Save changes to the file?");
    switch(QuestionBox::ask(buf,QuestionBox::R_No|QuestionBox::R_Yes|QuestionBox::R_Cancel)) {
      case QuestionBox::R_Cancel:
        return;
      case QuestionBox::R_Yes:
        ed->save();
        break;
      case QuestionBox::R_No:
      default:
        ed->discardChanges();
        break;
      }
    }

  delete ed;
  editor.erase(editor.begin()+i);

  updateOpenFileList();
  size_t currentTab = tabs->selection();
  if(i<currentTab)
    currentTab--;

  if(currentTab<editor.size())
    showEditor(currentTab);
  else if(0<editor.size())
    showEditor(editor.size()-1);
  }

void EditorArea::openFile(size_t id) {
  // auto& it = ProjectMgr::inst().item(id);
  // load(it);
  }

void EditorArea::onFilesysChange() {
  // const bool hasPro = ProjectMgr::inst().hasVisibleProject();
  // projTree->setVisible(hasPro);
  invalidateTools();
  }

void EditorArea::invalidateTools() {
  tabs ->invalidate();
  areaL->invalidate();
  areaR->invalidate();
  areaB->invalidate();
  }

void EditorArea::saveUiLayout() {
  areaL->saveUiLayout();
  areaR->saveUiLayout();
  areaB->saveUiLayout();
  EditorSettings::inst().setRootWeight(main->weights(),areaM->weights());
  EditorSettings::inst().save();
  }

void EditorArea::updateOpenFileList() {
  tabs->invalidate();
  }

EditorArea::EditorWrapper* EditorArea::currentEditor() {
  for(auto& i:editor)
    if(i->isVisible())
      return i;
  return nullptr;
  }

void EditorArea::moveDropOver(DropOverEvent& ev) {
  auto it = dynamic_cast<ToolGroup*>(&ev.drop());
  if(it==nullptr) {
    ev.ignore();
    return;
    }
  ev.accept();

  ToolArea* areas[] = {areaL,areaR,areaB};

  auto prevArea = aboutToDrop.area;
  aboutToDrop.area = nullptr;
  if(ev.pos().x<DragPadding) {
    aboutToDrop.area = areaL;
    }
  if(ev.pos().x>w()-DragPadding) {
    aboutToDrop.area = areaR;
    }
  if(ev.pos().y>h()-DragPadding) {
    aboutToDrop.area = areaB;
    }

  for(auto& i:areas) {
    if(i->rect().contains(ev.pos()))
      aboutToDrop.area = i;
    }

  if(aboutToDrop.area!=nullptr) {
    auto origin = aboutToDrop.area->mapToRoot(Point(0,0)) - mapToRoot(Point(0,0));
    aboutToDrop.merge = false;
    aboutToDrop.pos   = aboutToDrop.area->coordToDropPoint(ev.pos()-origin,aboutToDrop.merge);
    }
  if(aboutToDrop.area!=prevArea)
    invalidateTools();
  update();
  }

void EditorArea::moveDropLeave(DropOverEvent&) {
  aboutToDrop.pos  = size_t(-1);
  aboutToDrop.area = nullptr;
  update();
  }

void EditorArea::dropDone(DropOverEvent& ev) {
  if(aboutToDrop.area==nullptr)
    return;
  if(auto it = dynamic_cast<ToolGroup*>(&ev.drop())) {
    ev.accept();
    aboutToDrop.area->dropWidget(it,aboutToDrop.pos,aboutToDrop.merge);
    aboutToDrop.area  = nullptr;
    aboutToDrop.pos   = size_t(-1);
    aboutToDrop.merge = false;
    invalidateTools();
    saveUiLayout();
    }
  }
