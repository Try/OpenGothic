#pragma once

#include <Tempest/File>
#include <vector>
#include <filesystem>

#include "ui/editors/baseeditor.h"

class EditorSettings final {
  public:
    EditorSettings();
    ~EditorSettings();

    static EditorSettings& inst();

    enum ShortcutId : uint8_t {
      S_Undo,
      S_Redo,
      S_New,
      S_Open,
      S_Save,
      S_Count,
      };

    struct Tool {
      BaseEditor::ToolType parent = BaseEditor::ToolType::Right;
      size_t               group  = 0;
      size_t               order  = 0;
      };

    struct Shortcut {
      Tempest::Event::Modifier md  = Tempest::Event::M_NoModifier;
      Tempest::Event::KeyType  key = Tempest::Event::K_NoKey;

      std::string              toString() const;
      };

    void save();
    void defaultIntefaceLayout();
    void defaultShortcuts();

    Tempest::Signal<void()> onShortcuts;
    Tempest::Signal<void()> onRendering;

    const std::vector<std::filesystem::path>& recent() const { return recentFiles; }
    void pushRecent(const std::filesystem::path& path);

    void setViewPosition(ToolWindow::Tool t, BaseEditor::ToolType parent, size_t group, size_t ord);
    Tool toolPosition(ToolWindow::Tool t) const;

    void setToolWeight(const std::vector<float>& s, BaseEditor::ToolType parent);
    auto leftWeights()   -> const std::vector<float>&;
    auto rightWeights()  -> const std::vector<float>&;
    auto bottomWeights() -> const std::vector<float>&;

    void setRootWeight(const std::vector<float>& s, const std::vector<float>& mid);
    auto rootWeights() -> const std::vector<float>&;
    auto midWeights() -> const std::vector<float>&;

    const Shortcut& shortcut(ShortcutId i) const { return scuts[i]; }
    void setShortcut(ShortcutId i, const Shortcut& s);

  private:
    void load();

    void load(Tempest::RFile& fin);
    void save(Tempest::WFile& fout) const;

    static std::string_view     toStr(ToolWindow::Tool t);
    static std::string_view     toStr(BaseEditor::ToolType t);
    static BaseEditor::ToolType toToolType(std::string_view str);

    std::vector<std::filesystem::path> recentFiles;
    Tool                               tools[ToolWindow::T_Count];
    std::vector<float>                 rootW, midW, leftW, rightW, botW;

    Shortcut                           scuts[S_Count];
  };

