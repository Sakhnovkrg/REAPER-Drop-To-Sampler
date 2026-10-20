// macOS: paths and files (paths are UTF-8 natively)

#import <Foundation/Foundation.h>

#include "platform/platform.h"

#include <cstdio>
#include <unistd.h>

namespace platform {

std::string CartridgeDataDir()
{
    // ~/Library/Application Support/Cartridge
    NSArray* dirs = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
    NSString* base = [dirs count] ? [dirs objectAtIndex:0] : NSTemporaryDirectory();
    return std::string([base UTF8String]) + "/Cartridge";
}

bool FileExists(const char* path)
{
    return path && access(path, F_OK) == 0;
}

bool WriteTextFile(const char* path, const char* text)
{
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fputs(text, f) >= 0;
    return fclose(f) == 0 && ok;
}

} // namespace platform
