#include "rootview.h"

#include "editorarea.h"
#include "editorsettings.h"
#include "assets.h"

#include <Tempest/Log>
#include <Tempest/Painter>

using namespace Tempest;

static RootView* instance = nullptr;

RootView::RootView() {
  instance = this;

  setLayout(Horizontal);
  setSpacing(0);

  edit = &addWidget(new EditorArea());

  initShortkuts();
  EditorSettings::inst().onShortcuts.bind(this,&RootView::setupShortkuts);
  /*
  ProjectMgr::inst().onAssetReady.bind(this,&RootView::update);
  if(!ProjectMgr::inst().hasVisibleProject()) {
    RootView::inst().onNewFile();
    } else {
    edit->openApp();
    }
  */
  edit->load(); // TEMP: start level view
  setFocus(true);
  }

RootView& RootView::inst() {
  return *instance;
  }

void RootView::paintEvent(PaintEvent& e) {
  // if(!ProjectMgr::inst().hasVisibleProject())
  //   return;
  Painter p(e);
  p.setBrush(Assets::inst().colors.workspace);
  p.drawRect(0,0,w(),h());
  }

void RootView::initShortkuts() {
  skNewFile = Shortcut(*this,Event::M_Command,   Event::K_N);
  skNewFile.onActivated.bind(this,&RootView::onNewFile);

  skOpenFile = Shortcut(*this,Event::M_Command,  Event::K_O);
  skOpenFile.onActivated.bind(this,&RootView::onOpenProject);

  scSave    = Shortcut(*this,Event::M_Command,   Event::K_S);
  scSave.onActivated.bind(this,&RootView::onSave);

  scUndo    = Shortcut(*this,Event::M_Command,   Event::K_Z);
  scUndo.onActivated.bind(this,&RootView::onUndo);

  scRedo    = Shortcut(*this,Event::M_Command,   Event::K_Y);
  scRedo.onActivated.bind(this,&RootView::onRedo);
  }

void RootView::setupShortkuts() {
  auto& newFile = EditorSettings::inst().shortcut(EditorSettings::S_New);
  skNewFile.setModifier(newFile.md);
  skNewFile.setKey     (newFile.key);

  auto& openFile = EditorSettings::inst().shortcut(EditorSettings::S_Open);
  skOpenFile.setModifier(openFile.md);
  skOpenFile.setKey     (openFile.key);

  auto& save = EditorSettings::inst().shortcut(EditorSettings::S_Save);
  scSave.setModifier(save.md);
  scSave.setKey     (save.key);

  auto& undo = EditorSettings::inst().shortcut(EditorSettings::S_Undo);
  scUndo.setModifier(undo.md);
  scUndo.setKey     (undo.key);

  auto& redo = EditorSettings::inst().shortcut(EditorSettings::S_Redo);
  scRedo.setModifier(redo.md);
  scRedo.setKey     (redo.key);
  }

void RootView::onNewFile() {
  //TODO
  }

void RootView::onSave() {
  edit->save();
  }

void RootView::onUndo() {
  edit->undo();
  setFocus(true);
  }

void RootView::onRedo() {
  edit->redo();
  setFocus(true);
  }

bool RootView::onOpenProject() {
  //auto proj = OpenFileDialog::getProjOpenFileName(nullptr);
  //return onOpenProjectF(proj);
  return true;
  }

bool RootView::onOpenProjectF(const std::filesystem::path& proj) {
  /*
  if(proj.empty() || !std::filesystem::exists(proj))
    return false;
  edit->closeAll();
  ProjectMgr::inst().unloadAll();
  if(!ProjectMgr::inst().addProject(proj))
    return false;
  edit->openApp();
  return true;
  */
  return true;
  }

bool RootView::onCloseApplication() {
  return edit->closeApp();
  }

void RootView::onOpenFile(const std::filesystem::path& p) {
  //auto it = ProjectMgr::inst().resolveIoPath(p.u8string());
  //edit->load(it);
  }
