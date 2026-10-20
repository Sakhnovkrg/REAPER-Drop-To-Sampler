// macOS / Linux: files (paths are UTF-8 natively)

#include "platform/platform.h"

#include <cstdio>
#include <unistd.h>

namespace platform {

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
