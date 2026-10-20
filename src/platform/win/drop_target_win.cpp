// Windows: OLE IDropTarget registered on the TCP/MCP window, replacing REAPER's own

#include "platform/platform.h"

#include <ole2.h>
#include <shellapi.h>

#ifndef MK_ALT
#define MK_ALT 0x20
#endif

static bool g_oleInited = false;

// OLE gives UTF-16; REAPER APIs and Cartridge pending_load expect UTF-8
static std::string WideToUtf8(const wchar_t* wide)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return std::string();
    std::string out(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, &out[0], n, nullptr, nullptr);
    return out;
}

// grfKeyState can miss keys pressed during the drag; also ask the keyboard state
static Modifiers ModifiersFromKeyState(DWORD grfKeyState)
{
    Modifiers m;
    m.shift = (grfKeyState & MK_SHIFT) != 0 || (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    m.ctrl = (grfKeyState & MK_CONTROL) != 0 || (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    m.alt = (grfKeyState & MK_ALT) != 0 || (GetKeyState(VK_MENU) & 0x8000) != 0;
    return m;
}

static std::vector<std::string> DroppedFiles(IDataObject* pDataObj)
{
    std::vector<std::string> files;
    if (!pDataObj) return files;

    FORMATETC fmt = { CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM stg = {};
    if (FAILED(pDataObj->GetData(&fmt, &stg)))
        return files;

    HDROP hDrop = (HDROP)GlobalLock(stg.hGlobal);
    if (hDrop)
    {
        UINT count = DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
        for (UINT i = 0; i < count; ++i)
        {
            UINT len = DragQueryFileW(hDrop, i, nullptr, 0);
            std::wstring path(len, L'\0');
            if (len && DragQueryFileW(hDrop, i, &path[0], len + 1))
            {
                std::string utf8 = WideToUtf8(path.c_str());
                if (!utf8.empty())
                    files.push_back(utf8);
            }
        }
        GlobalUnlock(stg.hGlobal);
    }
    ReleaseStgMedium(&stg);
    return files;
}

class OleDropTarget : public IDropTarget
{
public:
    explicit OleDropTarget(DragHandler* handler) : m_handler(handler) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv) override
    {
        if (!ppv) return E_POINTER;
        *ppv = nullptr;
        if (riid == IID_IUnknown || riid == IID_IDropTarget)
        {
            *ppv = static_cast<IDropTarget*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_ref); }

    ULONG STDMETHODCALLTYPE Release() override
    {
        LONG r = InterlockedDecrement(&m_ref);
        if (r == 0) delete this;
        return r;
    }

    HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override
    {
        if (pdwEffect) *pdwEffect = DROPEFFECT_COPY;
        m_handler->OnDragEnter((int)DroppedFiles(pDataObj).size(), pt.x, pt.y, ModifiersFromKeyState(grfKeyState));
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override
    {
        if (pdwEffect) *pdwEffect = DROPEFFECT_COPY;
        m_handler->OnDragOver(pt.x, pt.y, ModifiersFromKeyState(grfKeyState));
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE DragLeave() override
    {
        m_handler->OnDragLeave();
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override
    {
        if (pdwEffect) *pdwEffect = DROPEFFECT_COPY;
        m_handler->OnDrop(DroppedFiles(pDataObj), pt.x, pt.y, ModifiersFromKeyState(grfKeyState));
        return S_OK;
    }

private:
    LONG m_ref = 1;
    DragHandler* m_handler;
};

class OleDropTargetRegistration : public DropTarget
{
public:
    OleDropTargetRegistration(HWND hwnd, OleDropTarget* target) : m_hwnd(hwnd), m_target(target) {}

    ~OleDropTargetRegistration() override
    {
        if (IsWindow(m_hwnd))
            RevokeDragDrop(m_hwnd);
        m_target->Release();
    }

private:
    HWND m_hwnd;
    OleDropTarget* m_target;
};

std::unique_ptr<DropTarget> DropTarget::Attach(HWND hwnd, DragHandler* handler)
{
    if (!hwnd || !handler) return nullptr;
    if (!g_oleInited)
    {
        OleInitialize(nullptr);
        g_oleInited = true;
    }

    OleDropTarget* target = new OleDropTarget(handler);
    // If REAPER already owns the target, steal it
    HRESULT hr = RegisterDragDrop(hwnd, target);
    if (hr == DRAGDROP_E_ALREADYREGISTERED)
    {
        RevokeDragDrop(hwnd);
        hr = RegisterDragDrop(hwnd, target);
    }
    if (FAILED(hr))
    {
        target->Release();
        return nullptr;
    }
    return std::unique_ptr<DropTarget>(new OleDropTargetRegistration(hwnd, target));
}
