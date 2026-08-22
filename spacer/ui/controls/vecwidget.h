#pragma once

#include <Tempest/Widget>

class NumberEdit;
class FloatWidget;

class VecWidget : public Tempest::Widget {
  public:
    VecWidget(std::string_view name, uint8_t cnt);

    void setValue(const Tempest::Vec2& v);
    void setValue(const Tempest::Vec3& v);
    void setValue(const Tempest::Vec4& v);

    void setComponentCount(uint8_t cnt);
    void setNames(std::initializer_list<std::string_view> n);

    void setSliderMinMax(float min, float max);
    void setSliderMinMax(const Tempest::Vec4& min, const Tempest::Vec4& max);

    void enableFloats(bool integer);
    void setCompact(bool comp);

    Tempest::Vec4 value() const;

    Tempest::Signal<void(Tempest::Vec4 v,bool commit)> onValueModifyed;
    Tempest::Signal<void()> onResize;

  private:
    struct Header;

    void onToogleMinimize();
    void onModifyed(float v, bool commit);
    void invalidatePriview();

    Header*       hdr     = nullptr;
    FloatWidget*  edit[4] = {};

    Tempest::Vec4 val;
    uint8_t       compCnt = 1;
  };

