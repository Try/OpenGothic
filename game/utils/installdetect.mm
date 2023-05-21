#include "installdetect.h"

#if __has_feature(objc_arc)
#error "Objective C++ ARC is not supported"
#endif

#include <Tempest/TextCodec>

#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>

std::u16string InstallDetect::applicationSupportDirectory() {
  std::string ret;

  NSArray* paths = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
  if(paths!=nil && [paths count]>0) {
    NSString* app = [[paths firstObject] stringByAppendingPathComponent:@"OpenGothic"];
    if(app!=nil) {
      ret = [app cStringUsingEncoding:NSUTF8StringEncoding];
      [app release];
      }
    }

  if(paths!=nil)
    [paths release];

  return Tempest::TextCodec::toUtf16(ret);
  }
