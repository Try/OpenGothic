#pragma once

#include <Tempest/Dialog>

#include "gamepadsettingsinput.h"

class GamepadSettingsMenu final : public Tempest::Dialog {
  public:
    explicit GamepadSettingsMenu(GamepadSettings settings, bool russian, float uiScale = 1.f);
    void onGamepad(KeyCodec::Action action);
    bool accepted() const { return input.accepted; }
    const GamepadSettings& settings() const { return input.draft; }

  protected:
    void keyDownEvent(Tempest::KeyEvent& event) override;
    void keyRepeatEvent(Tempest::KeyEvent& event) override;
    void keyUpEvent(Tempest::KeyEvent& event) override;
    void mouseDownEvent(Tempest::MouseEvent& event) override;
    void mouseWheelEvent(Tempest::MouseEvent& event) override;
    void metricsEvent(Tempest::DisplayMetricsEvent& event) override;
    void paintEvent(Tempest::PaintEvent& event) override;
    void paintShadow(Tempest::PaintEvent&) override {}

  private:
    struct Layout {
      int x, y, width, height, padding, rowHeight, visibleRows;
      float scale;
      };
    Layout layout() const;
    Tempest::Rect canvasRect() const;
    void reveal(const Layout& layout);
    std::string rowText(int row) const;
    GamepadSettingsInput input;
    bool russian = false;
    float uiScale = 1.f;
    int firstRow = 0;
  };
