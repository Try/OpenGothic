#pragma once

#include <Tempest/Widget>

class EnumWidget : public Tempest::Widget {
  public:
    EnumWidget(std::string_view hint);

    Tempest::Signal<void(size_t)> onItemSelected;
    Tempest::Signal<void(size_t)> onSelectionChanged;

    void   setItems(const std::vector<std::string>& it);
    void   setCurrentIndex(size_t id);
    size_t currentIndex() const;

  private:
    Tempest::ComboBox* val      = nullptr;
    size_t             eltCount = 0;
  };

