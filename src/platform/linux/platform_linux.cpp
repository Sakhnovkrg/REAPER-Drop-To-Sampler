// Linux: paths (file helpers are in platform/posix)

#include "platform/platform.h"

#include <cstdlib>

namespace platform {

std::string CartridgeDataDir()
{
    // JUCE's userApplicationDataDirectory on Linux is ~/.config (XDG_CONFIG_HOME is not consulted)
    const char* home = getenv("HOME");
    if (home && home[0])
        return std::string(home) + "/.config/Cartridge";
    return "/tmp/Cartridge";
}

} // namespace platform
