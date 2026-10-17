#include "mainwindow.h"

#if defined(__IOS__)

#import <UIKit/UIKit.h>

#include <algorithm>
#include <cmath>

float MainWindow::uiScale() const {
  auto window = reinterpret_cast<UIWindow*>(hwnd());
  const auto area = clientRect();
  // Keep the default 800x600 UI inside the safe area on smaller screens.
  return std::max(1.f, std::min({float(window.contentScaleFactor), float(area.w)/800.f, float(area.h)/600.f}));
  }

void MainWindow::updateSafeArea() {
  auto window = reinterpret_cast<UIWindow*>(hwnd());
  const auto insets = window.rootViewController.view.safeAreaInsets;
  const auto scale  = window.contentScaleFactor;
  setMargins(Tempest::Margin(int(std::ceil(insets.left*scale)), int(std::ceil(insets.right*scale)),
                            int(std::ceil(insets.top*scale)),  int(std::ceil(insets.bottom*scale))));
  }

#endif
