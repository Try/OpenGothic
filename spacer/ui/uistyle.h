#pragma once

#include <Tempest/Style>

class UiStyle : public Tempest::Style {
  public:
    UiStyle();

    // common
    void draw(Tempest::Painter& p, Tempest::Panel* w, Element e,
              const Tempest::WidgetState& st, const Tempest::Rect& r, const Extra& extra) const override;
    void draw(Tempest::Painter& p, Tempest::Dialog* w, Element e,
              const Tempest::WidgetState& st, const Tempest::Rect& r, const Extra& extra) const override;

    void draw(Tempest::Painter& p, Tempest::Button* w, Element e,
              const Tempest::WidgetState& st, const Tempest::Rect& r, const Extra& extra) const override;
    void draw(Tempest::Painter& p, Tempest::AbstractTextInput* w, Element e,
              const Tempest::WidgetState& st, const Tempest::Rect& r, const Extra& extra) const override;
    void draw(Tempest::Painter& p, Tempest::ComboBox* w, Element e,
              const Tempest::WidgetState& st, const Tempest::Rect& r, const Extra& extra) const override;
    void draw(Tempest::Painter& p, Tempest::CheckBox* w, Element e,
              const Tempest::WidgetState& st, const Tempest::Rect& r, const Extra& extra) const override;


    // dialog shadow
    void draw(Tempest::Painter& p, Tempest::Dialog* w, Tempest::UiOverlay* o, Element e,
              const Tempest::WidgetState& st, const Tempest::Rect& r,
              const Extra& extra, const Tempest::Rect& overlay) const override;

    // complex
    void draw(Tempest::Painter& p, Tempest::ScrollBar* w, Element e,
              const Tempest::WidgetState& st, const Tempest::Rect& r, const Extra& extra) const override;

    const UiMetrics& metrics() const override;
    Element          visibleElements() const override;

  private:
    static void draw9Path(Tempest::Painter& p, const Tempest::Rect& rect);

    UiMetrics met;
  };

