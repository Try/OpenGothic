#pragma once

#include <Tempest/Widget>

/// Cursor of the UI that doesn't need the mouse: hidden, unless the mouse emulates a touch screen on desktop
#if defined(OPENGOTHIC_TOUCH_UI) && !defined(__ANDROID__)
constexpr Tempest::CursorShape HiddenCursor = Tempest::CursorShape::Arrow;
#else
constexpr Tempest::CursorShape HiddenCursor = Tempest::CursorShape::Hidden;
#endif
