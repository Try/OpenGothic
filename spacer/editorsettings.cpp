#include "editorsettings.h"

#include <Tempest/Log>

#include "formats/jsonwriter.h"
#include "formats/jsonreader.h"
// #include "desktopservice.h"

using namespace Tempest;

static EditorSettings* instance = nullptr;

EditorSettings::EditorSettings() {
  instance = this;
  defaultIntefaceLayout();
  defaultShortcuts();
  load();
  }

EditorSettings::~EditorSettings() {
  instance = nullptr;
  }

EditorSettings& EditorSettings::inst() {
  return *instance;
  }

std::string_view EditorSettings::toStr(ToolWindow::Tool t) {
  switch(t) {
    case ToolWindow::T_Count:
      return "";
    case ToolWindow::T_ProjectTree:
      return "project";
    case ToolWindow::T_VobTree:
      return "level_vobs";
    case ToolWindow::T_VobProp:
      return "vob_prop";
    }
  return "";
  }

std::string_view EditorSettings::toStr(BaseEditor::ToolType t) {
  switch(t) {
    case BaseEditor::ToolType::Count:
    case BaseEditor::ToolType::Left:   return "left";
    case BaseEditor::ToolType::Right:  return "right";
    case BaseEditor::ToolType::Bottom: return "bottom";
    }
  return "right";
  }

BaseEditor::ToolType EditorSettings::toToolType(std::string_view str) {
  if(str=="left")
    return BaseEditor::ToolType::Left;
  if(str=="right")
    return BaseEditor::ToolType::Right;
  if(str=="bottom")
    return BaseEditor::ToolType::Bottom;
  return BaseEditor::ToolType::Right;
  }

void EditorSettings::pushRecent(const std::filesystem::path& path) {
  for(size_t i=0; i<recentFiles.size();) {
    if(recentFiles[i]==path) {
      recentFiles.erase(recentFiles.begin()+int(i));
      } else {
      ++i;
      }
    }
  if(recentFiles.size()>15)
    recentFiles.resize(15);
  recentFiles.insert(recentFiles.begin(),std::move(path));
  save();
  }

void EditorSettings::setViewPosition(ToolWindow::Tool t, BaseEditor::ToolType parent, size_t group, size_t ord) {
  tools[t].parent = parent;
  tools[t].group  = group;
  tools[t].order  = ord;
  }

EditorSettings::Tool EditorSettings::toolPosition(ToolWindow::Tool t) const {
  return tools[t];
  }

void EditorSettings::setToolWeight(const std::vector<float>& s, BaseEditor::ToolType parent) {
  switch(parent) {
    case BaseEditor::ToolType::Left:
      leftW = s;
      break;
    case BaseEditor::ToolType::Right:
      rightW = s;
      break;
    case BaseEditor::ToolType::Bottom:
      botW = s;
      break;
    case BaseEditor::ToolType::Count:
      break;
    }
  }

const std::vector<float>& EditorSettings::leftWeights() {
  return leftW;
  }

const std::vector<float>& EditorSettings::rightWeights() {
  return rightW;
  }

const std::vector<float>& EditorSettings::bottomWeights() {
  return botW;
  }

void EditorSettings::setRootWeight(const std::vector<float>& s, const std::vector<float>& mid) {
  rootW = s;
  midW  = mid;
  }

const std::vector<float>& EditorSettings::rootWeights() {
  return rootW;
  }

const std::vector<float>& EditorSettings::midWeights() {
  return midW;
  }

void EditorSettings::setShortcut(ShortcutId i, const Shortcut& s) {
  if(scuts[i].md==s.md && scuts[i].key==s.key)
    return;
  scuts[i] = s;
  save();
  onShortcuts();
  }

void EditorSettings::load() {
  auto path = std::filesystem::current_path();
  path /= "settings.json";

  if(!std::filesystem::exists(path))
    return;

  try {
    RFile fin(path.u16string());
    load(fin);
    }
  catch(std::exception& err) {
    Log::d("unable to load global settings file: ", err.what());
    }
  catch(...) {
    Log::d("unable to load global settings file");
    }
  }

void EditorSettings::save() {
  auto path = std::filesystem::current_path();
  path /= "settings.json";

  try {
    WFile fin(path.u16string());
    save(fin);
    }
  catch(std::exception& err) {
    Log::d("unable to save global settings file: ", err.what());
    }
  catch(...) {
    Log::d("unable to save global settings file");
    }
  }

void EditorSettings::defaultIntefaceLayout() {
  tools[ToolWindow::T_ProjectTree].parent = BaseEditor::Bottom;
  tools[ToolWindow::T_VobTree    ].parent = BaseEditor::Right;
  tools[ToolWindow::T_VobProp    ].parent = BaseEditor::Right;
  tools[ToolWindow::T_VobProp    ].group  = 1;

  rootW  = {1.f,5.f,1.f};
  midW   = {3.f,1.f};
  leftW  = {1.f};
  rightW = {0.5f,1.f,1.f};
  botW   = {4.f,1.f};
  }

void EditorSettings::defaultShortcuts() {
  scuts[S_Undo] = {Event::M_Command, Event::K_Z};
  scuts[S_Redo] = {Event::M_Command, Event::K_Y};
  scuts[S_New ] = {Event::M_Command, Event::K_N};
  scuts[S_Open] = {Event::M_Command, Event::K_O};
  scuts[S_Save] = {Event::M_Command, Event::K_S};
  };

void EditorSettings::load(RFile& fin) {
  JsonReader json(fin);
  if(auto r = json["recent"]) {
    for(size_t i=0; i<r.size(); ++i) {
      auto file = r[i].toPath();
      if(std::filesystem::exists(file))
        recentFiles.emplace_back(std::move(file));
      }
    }
  if(auto ui = json["interface"]) {
    for(size_t i=0; i<ToolWindow::T_Count; ++i) {
      if(auto t = ui[toStr(ToolWindow::Tool(i))]) {
        tools[i].parent = toToolType(t["parent"].toString());
        tools[i].group  = t["group"].toUint();
        tools[i].order  = t["order"].toUint();

        if(tools[i].group>=ToolWindow::T_Count)
          tools[i].group = 0;
        }
      }

    if(auto weights = ui["weights"]) {
      weights["root"  ].toArray(rootW);
      weights["mid"   ].toArray(midW);
      weights["left"  ].toArray(leftW);
      weights["right" ].toArray(rightW);
      weights["bottom"].toArray(botW);
      }
    }
  }

void EditorSettings::save(WFile& fout) const {
  JsonWriter json(fout);
  if(auto r = json.array("recent")) {
    for(auto& i:recentFiles)
      ;//r.val("",i.u8string());
    }
  if(auto ui = json.object("interface")) {
    for(size_t i=0; i<ToolWindow::T_Count; ++i) {
      if(auto t = ui.object(toStr(ToolWindow::Tool(i)))) {
        t.val("parent", toStr(tools[i].parent));
        t.val("group",  tools[i].group);
        t.val("order",  tools[i].order);
        }
      }
    if(auto weights = ui.object("weights")) {
      weights.val("root",  rootW);
      weights.val("mid",   midW);
      weights.val("left",  leftW);
      weights.val("right", rightW);
      weights.val("bottom",botW);
      }
    }
  }

std::string EditorSettings::Shortcut::toString() const {
  std::string s;
  if(md & Event::M_Ctrl) {
    s += "Ctrl";
    }
  if(md & Event::M_Command) {
#if defined(__OSX__)
    if(s.size()>0)
      s += " + ";
    s += "Cmd";
#else
    if((md & Event::M_Ctrl)==0) {
      if(s.size()>0)
        s += " + ";
      s += "Ctrl";
      }
#endif
    }
  if(md & Event::M_Alt) {
    if(s.size()>0)
      s += " + ";
    s += "Alt";
    }
  if(md & Event::M_Shift) {
    if(s.size()>0)
      s += " + ";
    s += "Shift";
    }

  if(key!=Event::K_NoKey) {
    if(s.size()>0)
      s += " + ";
    if(Event::K_0<=key && key<=Event::K_9)
      s += ('0'+(key-Event::K_0));
    if(Event::K_A<=key && key<=Event::K_Z)
      s += ('A'+(key-Event::K_A));
    }

  return s;
  }
