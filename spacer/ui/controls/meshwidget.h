#pragma once

#include <Tempest/Widget>
#include <Tempest/Signal>

#include <zenkit/vobs/VirtualObject.hh>

#include <memory>

#include "ui/dragdrop.h"

class MeshWidget : public Tempest::Widget,
                   public DropReciver {
  public:
    MeshWidget();
    MeshWidget(std::string_view title);

    Tempest::Signal<void(const zenkit::Visual&,bool)> onValueModifyed;
    Tempest::Signal<void(const zenkit::Visual&)>      onValueChanged;

    void setValue(std::shared_ptr<zenkit::Visual> file);
    auto value() const -> const zenkit::Visual&;

  protected:
    void moveDropOver(DropOverEvent& ev) override;
    void dropDone    (DropOverEvent& ev) override;

  private:
    void onModifyed();

    struct Combo;

    zenkit::Visual  vis = {};
    Tempest::Label* lbl = nullptr;
    Tempest::Label* txt = nullptr;
};
