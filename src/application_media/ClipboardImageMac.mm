#include "application_media/ClipboardImageMac.hpp"
#import <AppKit/AppKit.h>

namespace batbox::application_media::indigenous {
namespace {
constexpr NSUInteger maxPngBytes=32*1024*1024;

std::optional<std::string> copyData(NSData* data){
  if(!data||[data length]==0||[data length]>maxPngBytes)return {};
  return std::string(static_cast<const char*>([data bytes]),
    static_cast<std::size_t>([data length]));
}
}

std::optional<std::string> readMacClipboardPng(){
  @autoreleasepool {
    @try {
    NSPasteboard* pasteboard=[NSPasteboard generalPasteboard];
    if(!pasteboard)return {};
    if(auto png=copyData([pasteboard dataForType:NSPasteboardTypePNG]))
      return png;
    NSData* tiff=[pasteboard dataForType:NSPasteboardTypeTIFF];
    if(!tiff||[tiff length]==0||[tiff length]>maxPngBytes)return {};
    NSBitmapImageRep* bitmap=[[NSBitmapImageRep alloc] initWithData:tiff];
    if(!bitmap)return {};
    NSData* png=[bitmap representationUsingType:NSBitmapImageFileTypePNG
      properties:@{}];
    auto copied=copyData(png);
    [bitmap release];
    return copied;
    } @catch(NSException* error) {
      (void)error;
      return {};
    }
  }
}

std::optional<std::string> readMacClipboardFilePath(){
  @autoreleasepool {
    @try {
    NSPasteboard* pasteboard=[NSPasteboard generalPasteboard];
    if(!pasteboard)return {};
    NSArray* urls=[pasteboard readObjectsForClasses:@[[NSURL class]]
      options:@{NSPasteboardURLReadingFileURLsOnlyKey:@YES}];
    NSURL* url=[urls firstObject];
    if(!url){
      NSString* text=[pasteboard stringForType:NSPasteboardTypeFileURL];
      if(text)url=[NSURL URLWithString:text];
    }
    if(!url||![url isFileURL])return {};
    NSString* path=[url path];
    if(!path)return {};
    const char* encoded=[path UTF8String];
    if(!encoded||!*encoded)return {};
    return std::string(encoded);
    } @catch(NSException* error) {
      (void)error;
      return {};
    }
  }
}
}
