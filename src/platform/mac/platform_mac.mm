// macOS: paths (file helpers are in platform/posix)

#import <Foundation/Foundation.h>

#include "platform/platform.h"

namespace platform {

std::string CartridgeDataDir()
{
    // ~/Library/Application Support/Cartridge
    NSArray* dirs = NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory, NSUserDomainMask, YES);
    NSString* base = [dirs count] ? [dirs objectAtIndex:0] : NSTemporaryDirectory();
    return std::string([base UTF8String]) + "/Cartridge";
}

} // namespace platform
