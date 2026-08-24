#pragma once

#include <Tempest/ListView>
#include <Tempest/Widget>

#include "property.h"

class PropertyList : public Tempest::Widget {
  public:
    enum Type : uint8_t {
      T_Label,
      T_Typed,
      };

    struct Prop final {
      Prop(std::string_view title);
      Prop(std::string_view title, const Property::Type& t);

      std::string_view      title = "";
      Type                  type  = T_Label;
      const Property::Type* ptype = nullptr;
      };

    PropertyList(const std::initializer_list<Prop>& prop);
    PropertyList(const std::vector<Prop>& prop);

    Tempest::Signal<void()> onChanged;

  private:
    struct Delegate;
    Tempest::ListView list;
  };

