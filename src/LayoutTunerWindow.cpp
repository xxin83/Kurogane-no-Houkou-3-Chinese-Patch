#include "LayoutTunerWindow.h"

#include <cwchar>

#include "LayoutTuner.h"

namespace
{
    enum
    {
        kForceCp932Id = 100,
        kParagraphId = 101,
        kFirstEditId = 200,
        kFirstEnabledId = 300,
        kApplyId = 400,
        kResetId = 401,
        kCloseId = 402,
        kStatusId = 403
    };

    const int kHeights[kLayoutTunerFontCount] = { 12, 16, 28, 38, 76 };
    HWND g_window = nullptr;
    HANDLE g_thread = nullptr;
    DWORD g_threadId = 0;
    volatile LONG g_running = 0;
    HWND g_edits[kLayoutTunerFontCount][3] = {};
    HWND g_enabled[kLayoutTunerFontCount] = {};
    HWND g_forceCp932 = nullptr;
    HWND g_paragraph = nullptr;
    HWND g_status = nullptr;

    void SetText(HWND control, const wchar_t* text)
    {
        if (control)
            SetWindowTextW(control, text);
    }

    void SetInteger(HWND control, int value)
    {
        wchar_t text[32] = {};
        _snwprintf_s(text, _countof(text), _TRUNCATE, L"%d", value);
        SetText(control, text);
    }

    int ReadInteger(HWND control)
    {
        wchar_t text[64] = {};
        if (!control || GetWindowTextW(control, text, static_cast<int>(_countof(text))) <= 0)
            return 0;
        wchar_t* end = nullptr;
        const long value = wcstol(text, &end, 10);
        if (end == text)
            return 0;
        if (value < 0)
            return 0;
        if (value > 4096)
            return 4096;
        return static_cast<int>(value);
    }

    void RefreshControls()
    {
        const LayoutTunerSnapshot snapshot = LayoutTunerGetSnapshot();
        if (g_forceCp932)
            SendMessageW(g_forceCp932, BM_SETCHECK,
                snapshot.forceCp932 ? BST_CHECKED : BST_UNCHECKED, 0);
        if (g_paragraph)
            SendMessageW(g_paragraph, BM_SETCHECK,
                snapshot.paragraphMeasure ? BST_CHECKED : BST_UNCHECKED, 0);
        for (int row = 0; row < kLayoutTunerFontCount; ++row)
        {
            SetInteger(g_edits[row][0], snapshot.fonts[row].lfWidth);
            SetInteger(g_edits[row][1], snapshot.fonts[row].averageWidth);
            SetInteger(g_edits[row][2], snapshot.fonts[row].maxWidth);
            if (g_enabled[row])
                SendMessageW(g_enabled[row], BM_SETCHECK,
                    snapshot.fonts[row].enabled ? BST_CHECKED : BST_UNCHECKED, 0);
        }
    }

    void ApplyControls()
    {
        for (int row = 0; row < kLayoutTunerFontCount; ++row)
        {
            const LayoutTunerFontProfile profile = {
                ReadInteger(g_edits[row][0]),
                ReadInteger(g_edits[row][1]),
                ReadInteger(g_edits[row][2]),
                g_enabled[row] && SendMessageW(g_enabled[row], BM_GETCHECK, 0, 0) == BST_CHECKED
            };
            LayoutTunerSetFont(row, profile);
        }
        LayoutTunerSetFlags(
            g_forceCp932 && SendMessageW(g_forceCp932, BM_GETCHECK, 0, 0) == BST_CHECKED,
            g_paragraph && SendMessageW(g_paragraph, BM_GETCHECK, 0, 0) == BST_CHECKED);
        SetText(g_status, L"已应用；lfWidth 仅对后续创建的字体生效。请重新进入界面观察。\0");
    }

    HWND AddControl(DWORD exStyle, const wchar_t* className, const wchar_t* text,
        DWORD style, int x, int y, int width, int height, HWND parent, int id)
    {
        return CreateWindowExW(exStyle, className, text, style,
            x, y, width, height, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
            GetModuleHandleW(nullptr), nullptr);
    }

    void CreateControls(HWND window)
    {
        AddControl(0, L"STATIC", L"字体高度", WS_CHILD | WS_VISIBLE,
            12, 12, 80, 22, window, 0);
        AddControl(0, L"STATIC", L"lfWidth", WS_CHILD | WS_VISIBLE,
            100, 12, 90, 22, window, 0);
        AddControl(0, L"STATIC", L"avgWidth", WS_CHILD | WS_VISIBLE,
            200, 12, 90, 22, window, 0);
        AddControl(0, L"STATIC", L"maxWidth", WS_CHILD | WS_VISIBLE,
            300, 12, 90, 22, window, 0);
        AddControl(0, L"STATIC", L"启用", WS_CHILD | WS_VISIBLE,
            400, 12, 50, 22, window, 0);

        for (int row = 0; row < kLayoutTunerFontCount; ++row)
        {
            const int y = 40 + row * 32;
            wchar_t heightText[32] = {};
            _snwprintf_s(heightText, _countof(heightText), _TRUNCATE, L"%dpx", kHeights[row]);
            AddControl(0, L"STATIC", heightText, WS_CHILD | WS_VISIBLE,
                12, y + 4, 80, 22, window, 0);
            for (int column = 0; column < 3; ++column)
            {
                g_edits[row][column] = AddControl(WS_EX_CLIENTEDGE, L"EDIT", L"0",
                    WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_NUMBER,
                    100 + column * 100, y, 82, 24, window,
                    kFirstEditId + row * 3 + column);
            }
            g_enabled[row] = AddControl(0, L"BUTTON", L"",
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                405, y, 24, 24, window, kFirstEnabledId + row);
        }

        g_forceCp932 = AddControl(0, L"BUTTON", L"强制 CP932",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            12, 210, 130, 24, window, kForceCp932Id);
        g_paragraph = AddControl(0, L"BUTTON", L"段落测量修正",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            155, 210, 150, 24, window, kParagraphId);
        AddControl(0, L"BUTTON", L"Apply",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            320, 210, 90, 26, window, kApplyId);
        AddControl(0, L"BUTTON", L"Reset",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            420, 210, 90, 26, window, kResetId);
        AddControl(0, L"BUTTON", L"Close",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            520, 210, 90, 26, window, kCloseId);
        g_status = AddControl(0, L"STATIC", L"",
            WS_CHILD | WS_VISIBLE,
            12, 248, 600, 42, window, kStatusId);
        RefreshControls();
        SetText(g_status, L"lfWidth 仅对后续创建的字体生效；修改后请重新进入界面观察。\0");
    }

    LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
            CreateControls(window);
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case kApplyId:
                ApplyControls();
                return 0;
            case kResetId:
                LayoutTunerResetXpDefaults();
                RefreshControls();
        SetText(g_status, L"已恢复默认调参值。\0");
                return 0;
            case kCloseId:
                DestroyWindow(window);
                return 0;
            default:
                break;
            }
            break;
        case WM_CLOSE:
            DestroyWindow(window);
            return 0;
        case WM_DESTROY:
            InterlockedExchangePointer(reinterpret_cast<PVOID*>(&g_window), nullptr);
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }
        return DefWindowProcW(window, message, wParam, lParam);
    }

    DWORD WINAPI WindowThread(LPVOID parameter)
    {
        const HINSTANCE instance = static_cast<HINSTANCE>(parameter);
        const wchar_t className[] = L"WSC3LayoutTunerWindow";
        WNDCLASSW windowClass = {};
        windowClass.hInstance = instance;
        windowClass.lpfnWndProc = WindowProc;
        windowClass.lpszClassName = className;
        windowClass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        RegisterClassW(&windowClass);

        HWND window = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
            className,
            L"WSC3 排版调参测试",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            CW_USEDEFAULT, CW_USEDEFAULT, 640, 340,
            nullptr, nullptr, instance, nullptr);
        InterlockedExchangePointer(reinterpret_cast<PVOID*>(&g_window), window);
        if (!window)
        {
            InterlockedExchange(&g_running, 0);
            return 0;
        }
        ShowWindow(window, SW_SHOW);
        UpdateWindow(window);

        MSG message = {};
        while (GetMessageW(&message, nullptr, 0, 0) > 0)
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        InterlockedExchange(&g_running, 0);
        return 0;
    }
}

bool LayoutTunerWindowStart(HINSTANCE instance)
{
    LayoutTunerInitialize();
    if (InterlockedCompareExchange(&g_running, 1, 0) != 0)
        return true;
    if (g_thread)
    {
        CloseHandle(g_thread);
        g_thread = nullptr;
    }
    g_thread = CreateThread(nullptr, 0, WindowThread, instance, 0, &g_threadId);
    if (!g_thread)
    {
        InterlockedExchange(&g_running, 0);
        return false;
    }
    return true;
}

void LayoutTunerWindowStop()
{
    HWND window = reinterpret_cast<HWND>(InterlockedCompareExchangePointer(
        reinterpret_cast<PVOID*>(&g_window), nullptr, nullptr));
    if (window)
        PostMessageW(window, WM_CLOSE, 0, 0);
    if (g_thread)
    {
        const DWORD waitResult = WaitForSingleObject(g_thread, 2000);
        if (waitResult == WAIT_OBJECT_0)
        {
            CloseHandle(g_thread);
            g_thread = nullptr;
            InterlockedExchange(&g_running, 0);
        }
    }
}

bool LayoutTunerWindowIsRunning()
{
    return InterlockedCompareExchange(&g_running, 0, 0) != 0;
}
