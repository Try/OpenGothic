#pragma once

#include <Tempest/Widget>

#include "ui/property/property.h"
#include "utility/variant.h"

class ParameterWidget : virtual public Tempest::Widget {
  public:
    static ParameterWidget* createEditor(const Property::Slot& s, const Variant& v, size_t id);

    virtual Variant argv() const;
    virtual void    setArgv(const Variant&);
    virtual void    setAnimationTiming(float fps, bool smooth);

    Tempest::Signal<void(size_t id,const Variant& v,bool commit)> onChanged;

  private:
    template<class T>
    struct Base;

    struct EditImage;
    struct EditTexture;
    struct EditMesh;
    struct EditString;
    struct EditBool;
    struct EditEnum;
    struct EditColorWidget;
    template<class Editor, class Value>
    struct EditAnimationList;
    struct EditInt1;
    struct EditFloat1;
    struct EditFloat2;
    struct EditFloat3;
    struct EditFloat4;

    ParameterWidget() = default;

    static ParameterWidget* implCreateEditor(const Property::Type& p, std::string_view name,
                                             const std::vector<std::string>& enumValues,
                                             const std::vector<Tempest::Vec3>& clId,
                                             const Tempest::Vec4& min, const Tempest::Vec4& max, size_t pos);

    template<class Edit, class T>
    static ParameterWidget* implVecEditor(const Property::Type& p, std::string_view name, const Tempest::Vec4& min, const Tempest::Vec4& max, size_t pos);

    static void             implSetSliderMinMax(EditFloat1* e, const Tempest::Vec4& min, const Tempest::Vec4& max);
    template<class Edit>
    static void             implSetSliderMinMax(Edit* e, const Tempest::Vec4& min, const Tempest::Vec4& max);
  };

