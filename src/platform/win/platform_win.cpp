// Windows: paths and files (UTF-8 <-> UTF-16)

#include "platform/platform.h"

#include <cstdio>
#include <cstdlib>

static std::wstring Utf8ToWide(const char* utf8)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (n <= 1) return std::wstring();
    std::wstring out(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &out[0], n);
    return out;
}

static std::string WideToUtf8(const wchar_t* wide)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return std::string();
    std::string out(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, &out[0], n, nullptr, nullptr);
    return out;
}

namespace platform {

std::string CartridgeDataDir()
{
    const wchar_t* appdata = _wgetenv(L"APPDATA");
    if (appdata && appdata[0])
        return WideToUtf8(appdata) + "\\Cartridge";
    return "Cartridge";
}

bool FileExists(const char* path)
{
    return GetFileAttributesW(Utf8ToWide(path).c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool WriteTextFile(const char* path, const char* text)
{
    FILE* f = _wfopen(Utf8ToWide(path).c_str(), L"wb");
    if (!f) return false;
    bool ok = fputs(text, f) >= 0;
    return fclose(f) == 0 && ok;
}

} // namespace platform
