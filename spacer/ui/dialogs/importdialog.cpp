#include "importdialog.h"

#include <Tempest/Painter>
#include <Tempest/TextCodec>

#include "assets.h"
#include "gothic.h"
#include "openfiledialog.h"
#include "ui/uihelper.h"
#include "utils/fileext.h"
#include "utils/fileutil.h"
#include "utils/srcparser.h"

using namespace Tempest;

class ImportDialog::ImportPreview : public ScrollWidget {
  protected:
    void paintEvent(PaintEvent& event) override {
      Painter painter(event);
      painter.setBrush(Assets::inst().colors.workspaceD);
      painter.drawRect(0, 0, w(), h());
      }
  };

ImportDialog::ImportDialog() {
  constexpr int padding = 16;
  constexpr int spacing = 10;
  constexpr int rowHeight = 32;

  setLayout(Vertical);
  setMargins(padding);
  setSpacing(spacing);
  resize(800, 600);

  auto& title = addWidget(new Label());
  auto font = title.font();
  font.setPixelSize(20);
  font.setBold(true);
  title.setFont(font);
  title.setText("Import Gothic Project");

  auto& source = addWidget(new Widget());
  source.setLayout(Horizontal);
  source.setSizePolicy(Preferred, Fixed);
  source.setSpacing(spacing);

  auto& sourceLabel = source.addWidget(new Label());
  sourceLabel.setText("Project INI");
  sourceLabel.setSizePolicy(Fixed, Fixed);
  sourceLabel.setMinimumSize(sourceLabel.sizeHint().w, rowHeight);
  path = &source.addWidget(new LineEdit());
  path->setSizePolicy(Expanding, Fixed);
  path->setMinimumSize(0, rowHeight);
  path->onTextChanged.bind(this, &ImportDialog::invalidatePreview);
  path->onEnter.bind(this, &ImportDialog::previewProject);

  auto& browseButton = source.addWidget(new Button());
  browseButton.setText("Browse...");
  browseButton.setSizePolicy(Fixed, Fixed);
  browseButton.onClick.bind(this, &ImportDialog::browse);

  source.setMinimumSize(UiHelper::wrapContent(source, Horizontal));

  auto& previewLabel = addWidget(new Label());
  previewLabel.setText("Import preview");

  scroll = &addWidget(new ImportPreview());
  scroll->setSizePolicy(Preferred, Expanding);
  scroll->setMargins(spacing);
  scroll->setVscrollViewMode(ScrollWidget::AsNeed);
  scroll->setHscrollViewMode(ScrollWidget::AsNeed);
  scroll->scrollAfterEndV(false);

  auto& actions = addWidget(new Widget());
  actions.setLayout(Horizontal);
  actions.setSizePolicy(Preferred, Fixed);
  actions.setSpacing(spacing);

  status = &actions.addWidget(new Label());
  status->setSizePolicy(Expanding, Fixed);
  status->setText("Select a Gothic project INI to preview its contents.");

  auto& cancel = actions.addWidget(new Button());
  cancel.setText("Cancel");
  cancel.setButtonType(Button::T_FlatButton);
  cancel.setSizePolicy(Fixed, Fixed);
  cancel.onClick.bind(static_cast<Dialog*>(this), &Dialog::close);

  open = &actions.addWidget(new Button());
  open->setText("Open");
  open->setSizePolicy(Fixed, Fixed);
  open->setEnabled(false);
  open->onClick.bind(this, &ImportDialog::acceptProject);
  actions.setMinimumSize(0, rowHeight);
  }

std::filesystem::path ImportDialog::selectProject() {
  accepted = false;
  exec();
  return accepted ? previewedPath : std::filesystem::path();
  }

void ImportDialog::browse() {
  auto selected = OpenFileDialog::getProjOpenFileName("");
  if(selected.empty())
    return;
  path->setText(TextCodec::toUtf8(selected.u16string()));
  previewProject();
  }

void ImportDialog::invalidatePreview(const TextModel&) {
  previewedPath.clear();
  open->setEnabled(false);
  scroll->centralWidget().removeAllWidgets();
  status->setText("Preview the selected INI before opening.");
  }

void ImportDialog::previewProject() {
  invalidatePreview(path->text());
  try {
    auto selected = std::filesystem::path(TextCodec::toUtf16(path->text().c_str()));
    if(selected.empty() || !FileExt::hasExt(selected.filename().u16string(), u"ini") ||
       !std::filesystem::is_regular_file(selected)) {
      status->setText("Select an existing .ini file.");
      return;
      }

    selected = std::filesystem::absolute(selected);
    const auto gameDir = selected.parent_path().parent_path();
    const auto dataDir = FileUtil::nestedPath(gameDir.u16string(), {u"Data"}, Dir::FT_Dir);
    const auto workDir = FileUtil::nestedPath(gameDir.u16string(), {u"_work"}, Dir::FT_Dir);
    GameModDescriptor desc(selected.u16string(), dataDir, workDir);

    bool manifestReadFailed = false;
    {
      ScrollWidget::LayoutLock lock(*scroll);
      auto& list = scroll->centralWidget();
      constexpr int indent = 12;
      auto addRow = [&list](std::string_view text, int leftMargin, bool heading = false) {
        auto& label = list.addWidget(new Label());
        const int headingPadding = heading ? 6 : 0;
        label.setMargins(Margin(leftMargin, headingPadding, 0, headingPadding));
        if(heading) {
          auto font = label.font();
          font.setBold(true);
          label.setFont(font);
          }
        label.setText(text);
        };

      addRow("Discovered VDF and MOD archives", 0, true);
      if(desc.vdf.empty())
        addRow("None found", indent);
      for(const auto& archive : desc.vdf)
        addRow(TextCodec::toUtf8(std::filesystem::path(archive).filename().u16string()), indent);

      addRow("Script manifests and entries", 0, true);
      if(desc.src.empty())
        addRow("No .src files found", indent);
      for(const auto& manifest : desc.src) {
        auto relative = std::filesystem::path(manifest).lexically_relative(gameDir);
        addRow(TextCodec::toUtf8(relative.empty() ? manifest : relative.u16string()), indent);
        try {
          SrcParser scripts(manifest);
          for(const auto& entry : scripts.files)
            addRow(entry, 2 * indent);
          }
        catch(...) {
          addRow("Could not read this manifest.", 2 * indent);
          manifestReadFailed = true;
          }
        }
    }
    scroll->scrollH(0);
    scroll->scrollV(0);
    open->setEnabled(!manifestReadFailed);
    if(manifestReadFailed) {
      status->setText("Could not read one or more script manifests.");
      return;
      }
    status->setText("Review the archives and scripts, then click Open.");
    previewedPath = std::move(selected);
    }
  catch(const std::exception&) {
    status->setText("Could not read the INI or scan its project files.");
    }
  }

void ImportDialog::acceptProject() {
  if(previewedPath.empty())
    return;
  const auto selected = previewedPath;
  accepted = true;
  close();
  }
