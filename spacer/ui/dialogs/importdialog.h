#pragma once

#include <Tempest/Button>
#include <Tempest/Dialog>
#include <Tempest/Label>
#include <Tempest/LineEdit>
#include <Tempest/ScrollWidget>
#include <Tempest/Signal>
#include <Tempest/TextModel>
#include <filesystem>

class ImportDialog : public Tempest::Dialog {
  public:
    ImportDialog();

    std::filesystem::path selectProject();

  private:
    class ImportPreview;

    void browse();
    void previewProject();
    void invalidatePreview(const Tempest::TextModel&);
    void acceptProject();

    Tempest::LineEdit*     path = nullptr;
    Tempest::Label*        status = nullptr;
    Tempest::Label*        contents = nullptr;
    Tempest::ScrollWidget* scroll = nullptr;
    Tempest::Button*       open = nullptr;

    std::filesystem::path previewedPath;
    bool                  accepted = false;
  };
