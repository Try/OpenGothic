#pragma once

#include <Tempest/Widget>
#include <Tempest/Label>
#include <Tempest/LineEdit>

class NumberEdit;

class IntWidget : public Tempest::Widget {
  public:
    IntWidget();
    IntWidget(std::string_view name);

    void setValue(int v);
    int  value() const;

    Tempest::Signal<void(int)> onValueChanged;
    Tempest::Signal<void(int v,bool commit)> onValueModifyed;

  private:
    Tempest::Label* lbl = nullptr;
    NumberEdit*     txt = nullptr;
    int             val = 0;

    void            onModifyed(double v, bool commit);
  };

