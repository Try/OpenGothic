#pragma once

#include <Tempest/LineEdit>
#include <Tempest/Widget>

class StringWidget  : public Tempest::Widget {
public:
  StringWidget();
  StringWidget(std::string_view name);

  void setValue(std::string_view v);
  auto value() const -> std::string_view;

  Tempest::Signal<void(std::string_view)> onValueChanged;
  Tempest::Signal<void(std::string_view,bool commit)> onValueModifyed;

private:
  Tempest::Label*    lbl = nullptr;
  Tempest::LineEdit* txt = nullptr;

  void               onModifyed(const Tempest::TextModel& txt);
};
