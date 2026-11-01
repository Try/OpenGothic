#pragma once

#include <Tempest/Widget>
#include <Tempest/Label>
#include <Tempest/Checkbox>

class NumberEdit;

class CheckWidget : public Tempest::Widget {
  public:
    CheckWidget(std::string_view name = "");

    void setValue(bool v);
    bool value() const;

    Tempest::Signal<void(bool)> onValueChanged;
    Tempest::Signal<void(bool v,bool commit)> onValueModifyed;

  private:
    Tempest::Label*    lbl = nullptr;
    Tempest::CheckBox* val = nullptr;

    void onModifyed(double v, bool commit);
  };