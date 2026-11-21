#pragma once

#include <Tempest/Widget>
#include <Tempest/Button>
#include <Tempest/Shortcut>

#include <filesystem>

// #include "ui/dialogs/newfiledialog.h"

class EditorArea;

class RootView : public Tempest::Widget {
  public:
    RootView();

    static RootView& inst();

    void onNewFile();
    void onSave();
    void onUndo();
    void onRedo();
    bool onOpenProject();
    bool onOpenProjectF(const std::filesystem::path& pro);
    bool onCloseApplication();

    void onOpenFile(const std::filesystem::path& p);

  private:
    void paintEvent(Tempest::PaintEvent &event) override;
    void initShortkuts();
    void setupShortkuts();

    Tempest::Shortcut      skNewFile;
    Tempest::Shortcut      skOpenFile;
    Tempest::Shortcut      scSave;
    Tempest::Shortcut      scUndo;
    Tempest::Shortcut      scRedo;

    EditorArea*            edit = nullptr;

    //NewFileDialog          newFileDlg;
  };

