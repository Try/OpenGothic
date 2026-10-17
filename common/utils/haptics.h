#pragma once

/// Vibration of the device. Does nothing on platforms without it.
namespace Haptics {
  /// duration in milliseconds, strength from 1 to 255
  void vibrate(int ms, int strength);
  }
