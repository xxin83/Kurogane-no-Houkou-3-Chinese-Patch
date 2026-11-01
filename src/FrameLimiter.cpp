#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <string.h>
#include "FrameLimiter.h"
#include "minhook/MinHook.h"

struct IDirect3D8;
struct IDirect3DDevice8;
using Create8Fn = IDirect3D8* (WINAPI*)(UINT);
using CreateDeviceFn = HRESULT (STDMETHODCALLTYPE*)(IDirect3D8*, UINT, UINT, HWND, DWORD, void*, IDirect3DDevice8**);
using EndSceneFn = HRESULT (STDMETHODCALLTYPE*)(IDirect3DDevice8*);

namespace
{
    Create8Fn g_create8 = nullptr;
    CreateDeviceFn g_createDevice = nullptr;
    EndSceneFn g_endScene = nullptr;
    UINT g_targetFps = 0;
    LARGE_INTEGER g_frequency = {};
    LONGLONG g_nextTick = 0;
    SRWLOCK g_lock = SRWLOCK_INIT;

    HRESULT STDMETHODCALLTYPE LimitEndScene(IDirect3DDevice8* device)
    {
        AcquireSRWLockExclusive(&g_lock);
        LARGE_INTEGER now = {};
        QueryPerformanceCounter(&now);
        const LONGLONG interval = g_frequency.QuadPart / g_targetFps;
        if (!g_nextTick || now.QuadPart > g_nextTick + interval * 4)
            g_nextTick = now.QuadPart;
        while (now.QuadPart < g_nextTick)
        {
            const LONGLONG ms = (g_nextTick - now.QuadPart) * 1000 / g_frequency.QuadPart;
            Sleep(ms > 1 ? static_cast<DWORD>(ms - 1) : 0);
            QueryPerformanceCounter(&now);
        }
        g_nextTick += interval;
        ReleaseSRWLockExclusive(&g_lock);
        return g_endScene(device);
    }

    HRESULT STDMETHODCALLTYPE HookCreateDevice(IDirect3D8* d3d, UINT adapter, UINT type,
        HWND window, DWORD flags, void* parameters, IDirect3DDevice8** device)
    {
        const HRESULT result = g_createDevice(d3d, adapter, type, window, flags, parameters, device);
        if (SUCCEEDED(result) && device && *device && !g_endScene)
        {
            void** vtable = *reinterpret_cast<void***>(*device);
            if (vtable && vtable[35] && MH_CreateHook(vtable[35],
                reinterpret_cast<LPVOID>(&LimitEndScene), reinterpret_cast<LPVOID*>(&g_endScene)) == MH_OK)
                MH_EnableHook(vtable[35]);
        }
        return result;
    }

    IDirect3D8* WINAPI HookCreate8(UINT sdk)
    {
        IDirect3D8* d3d = g_create8(sdk);
        if (d3d && !g_createDevice)
        {
            void** vtable = *reinterpret_cast<void***>(d3d);
            if (vtable && vtable[15] && MH_CreateHook(vtable[15],
                reinterpret_cast<LPVOID>(&HookCreateDevice), reinterpret_cast<LPVOID*>(&g_createDevice)) == MH_OK)
                MH_EnableHook(vtable[15]);
        }
        return d3d;
    }
}

bool InstallFrameLimiter(UINT targetFps)
{
    if (targetFps < 15 || targetFps > 240 || !QueryPerformanceFrequency(&g_frequency) || g_frequency.QuadPart < targetFps)
        return false;
    g_targetFps = targetFps;
    if (!GetModuleHandleW(L"d3d8.dll") && !LoadLibraryW(L"d3d8.dll"))
        return false;
    return MH_CreateHookApi(L"d3d8.dll", "Direct3DCreate8", reinterpret_cast<LPVOID>(&HookCreate8),
        reinterpret_cast<LPVOID*>(&g_create8)) == MH_OK;
}

bool InstallFrameLimiterFromGameIni()
{
    char path[MAX_PATH] = {};
    const DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* slash = len > 0 && len < MAX_PATH ? strrchr(path, '\\') : nullptr;
    if (!slash || static_cast<size_t>(slash - path) + sizeof("\\debug.ini") > MAX_PATH)
        return false;
    lstrcpyA(slash + 1, "debug.ini");
    const UINT frameRate = GetPrivateProfileIntA("Common", "FrameRate", 60, path);
    if (frameRate == 0)
        return true;
    return InstallFrameLimiter(frameRate >= 15 && frameRate <= 240 ? frameRate : 60);
}


