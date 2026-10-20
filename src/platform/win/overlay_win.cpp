// Windows: layered topmost window with a premultiplied PNG next to the cursor (GDI+)

#include "platform/platform.h"
#include "reaper_plugin_functions.h"
#include "resource.h"
#include "settings.h"

#include <objidl.h>
#include <gdiplus.h>

using namespace Gdiplus;

static HMODULE ThisModule()
{
    HMODULE mod = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&ThisModule, &mod);
    return mod;
}

static std::wstring Utf8ToWide(const std::string& utf8)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (n <= 1) return std::wstring();
    std::wstring out(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &out[0], n);
    return out;
}

// Draws the image into a premultiplied-alpha DIB section for UpdateLayeredWindow
static HBITMAP ToPremultipliedHBitmap(Bitmap* src, int* outW, int* outH)
{
    if (!src || src->GetLastStatus() != Ok)
        return nullptr;

    const int w = (int)src->GetWidth();
    const int h = (int)src->GetHeight();
    if (w <= 0 || h <= 0)
        return nullptr;

    Bitmap dst(w, h, PixelFormat32bppPARGB);
    if (dst.GetLastStatus() != Ok)
        return nullptr;
    {
        Graphics g(&dst);
        g.SetCompositingMode(CompositingModeSourceCopy);
        g.Clear(Color(0, 0, 0, 0));
        g.SetCompositingMode(CompositingModeSourceOver);
        g.DrawImage(src, 0, 0, w, h);
        g.Flush(FlushIntentionSync);
    }

    BitmapData bd = {};
    Rect rect(0, 0, w, h);
    if (dst.LockBits(&rect, ImageLockModeRead, PixelFormat32bppPARGB, &bd) != Ok)
        return nullptr;

    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (dib && bits)
    {
        const int dstStride = w * 4;
        for (int y = 0; y < h; ++y)
            memcpy((BYTE*)bits + y * dstStride, (BYTE*)bd.Scan0 + y * bd.Stride, dstStride);
    }
    dst.UnlockBits(&bd);

    if (!dib) return nullptr;
    if (outW) *outW = w;
    if (outH) *outH = h;
    return dib;
}

static HBITMAP LoadPngFile(const std::string& path, int* outW, int* outH)
{
    Bitmap src(Utf8ToWide(path).c_str());
    return ToPremultipliedHBitmap(&src, outW, outH);
}

// PNG from RCDATA embedded in this DLL
static HBITMAP LoadPngFromResource(int resId, int* outW, int* outH)
{
    HMODULE inst = ThisModule();
    HRSRC hrs = FindResourceA(inst, MAKEINTRESOURCEA(resId), (LPCSTR)RT_RCDATA);
    if (!hrs) return nullptr;
    HGLOBAL hg = LoadResource(inst, hrs);
    const void* data = hg ? LockResource(hg) : nullptr;
    DWORD size = SizeofResource(inst, hrs);
    if (!data || !size) return nullptr;

    HGLOBAL copy = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!copy) return nullptr;
    void* dst = GlobalLock(copy);
    if (!dst)
    {
        GlobalFree(copy);
        return nullptr;
    }
    memcpy(dst, data, size);
    GlobalUnlock(copy);

    IStream* stream = nullptr;
    if (CreateStreamOnHGlobal(copy, TRUE, &stream) != S_OK) // TRUE = free on release
    {
        GlobalFree(copy);
        return nullptr;
    }

    Bitmap* src = Bitmap::FromStream(stream);
    stream->Release();
    HBITMAP result = ToPremultipliedHBitmap(src, outW, outH);
    delete src;
    return result;
}

// Optional PNG in UserPlugins overrides the embedded default
static std::string FindOverridePng(const char* fileName)
{
    if (!GetResourcePath) return std::string();
    std::string path = std::string(GetResourcePath()) + "\\UserPlugins\\" + fileName;
    return platform::FileExists(path.c_str()) ? path : std::string();
}

struct OverlayImage
{
    HBITMAP bmp = nullptr; // plain GDI bitmap, outlives GDI+ shutdown
    int w = 32, h = 32;

    OverlayImage() = default;
    OverlayImage(const OverlayImage&) = delete;
    OverlayImage& operator=(const OverlayImage&) = delete;

    void Load(const char* fileName, int resId)
    {
        const char* source = "embedded";
        std::string path = FindOverridePng(fileName);
        if (!path.empty())
        {
            bmp = LoadPngFile(path, &w, &h);
            if (bmp) source = "file";
        }
        if (!bmp)
            bmp = LoadPngFromResource(resId, &w, &h);
        DebugLog("[Drop To Sampler] overlay %s=%s (%s %dx%d)\n", fileName, bmp ? "OK" : "FAIL", source, w, h);
    }

    ~OverlayImage()
    {
        if (bmp) DeleteObject(bmp);
    }
};

static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

class GdiPlusOverlay : public DragOverlay
{
public:
    GdiPlusOverlay()
    {
        GdiplusStartupInput in;
        if (GdiplusStartup(&m_gdiplusToken, &in, nullptr) != Ok)
            return;
        m_insert.Load("sampler_insert.png", IDR_PNG_INSERT);
        m_replace.Load("sampler_replace.png", IDR_PNG_REPLACE);
    }

    ~GdiPlusOverlay() override
    {
        if (m_hwnd && IsWindow(m_hwnd))
            DestroyWindow(m_hwnd);
        if (m_gdiplusToken)
            GdiplusShutdown(m_gdiplusToken);
    }

    void ShowAtCursor(DropVisualKind kind) override
    {
        EnsureWindow();
        if (!m_hwnd)
            return;

        const OverlayImage& img = (kind == DropVisual_Replace && m_replace.bmp) ? m_replace : m_insert;
        if (!img.bmp)
        {
            // Hide previous frame (layered window can keep last image if only SW_HIDE)
            ShowWindow(m_hwnd, SW_HIDE);
            return;
        }

        POINT pt;
        GetCursorPos(&pt);
        pt.x += 16;
        pt.y += 16;

        SetWindowPos(m_hwnd, HWND_TOPMOST, pt.x, pt.y, img.w, img.h,
            SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOCOPYBITS);
        Paint(img, pt);
    }

    void Hide() override
    {
        if (m_hwnd && IsWindow(m_hwnd))
            ShowWindow(m_hwnd, SW_HIDE);
    }

private:
    void EnsureWindow()
    {
        if (m_hwnd && IsWindow(m_hwnd))
            return;
        HINSTANCE inst = ThisModule();
        static bool registered = false;
        if (!registered)
        {
            WNDCLASSA wc = {};
            wc.lpfnWndProc = OverlayWndProc;
            wc.hInstance = inst;
            wc.lpszClassName = "DropToSamplerOverlay";
            RegisterClassA(&wc);
            registered = true;
        }
        m_hwnd = CreateWindowExA(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            "DropToSamplerOverlay", "", WS_POPUP,
            0, 0, 64, 64, nullptr, nullptr, inst, nullptr);
    }

    void Paint(const OverlayImage& img, POINT ptDst)
    {
        HDC screen = GetDC(nullptr);
        HDC mem = CreateCompatibleDC(screen);
        HGDIOBJ old = SelectObject(mem, img.bmp);

        SIZE size = { img.w, img.h };
        POINT ptSrc = { 0, 0 };
        BLENDFUNCTION blend = {};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;

        if (!UpdateLayeredWindow(m_hwnd, screen, &ptDst, &size, mem, &ptSrc, 0, &blend, ULW_ALPHA))
            DebugLog("[Drop To Sampler] UpdateLayeredWindow failed err=%lu\n", GetLastError());

        SelectObject(mem, old);
        DeleteDC(mem);
        ReleaseDC(nullptr, screen);
    }

    ULONG_PTR m_gdiplusToken = 0;
    HWND m_hwnd = nullptr;
    OverlayImage m_insert, m_replace;
};

std::unique_ptr<DragOverlay> DragOverlay::Create()
{
    return std::unique_ptr<DragOverlay>(new GdiPlusOverlay());
}
