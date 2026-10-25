// A separate native translation unit keeps Apple's MacTypes names out of the
// fixture that includes the real GameMenu implementation.
#import <CoreFoundation/CoreFoundation.h>

extern "C" void* gamepadMenuLoopStartWatchdog(void (*callback)(void*), void* context) {
  // This timer runs in nested native loops even when a serial main GCD callback
  // has not returned. A dispatch timer could not bound the original regression.
  auto timer = CFRunLoopTimerCreateWithHandler(kCFAllocatorDefault,
    CFAbsoluteTimeGetCurrent()+5.0,0,0,0,^(CFRunLoopTimerRef) { callback(context); });
  if(timer!=nullptr)
    CFRunLoopAddTimer(CFRunLoopGetMain(),timer,kCFRunLoopCommonModes);
  return timer;
  }

extern "C" void gamepadMenuLoopStopWatchdog(void* timer) {
  if(timer==nullptr)
    return;
  auto native = static_cast<CFRunLoopTimerRef>(timer);
  CFRunLoopTimerInvalidate(native);
  CFRelease(native);
  }
