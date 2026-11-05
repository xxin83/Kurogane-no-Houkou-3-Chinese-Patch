#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>
#include <intrin.h>
#include <algorithm>
#include <cwctype>
#include <map>
#include <string>
#include <vector>
#include <stdint.h>
#include "UiDiag.h"
#include "LayoutTuner.h"
#include "LayoutTunerWindow.h"
#include "XpCp932Width.h"
#include "FrameLimiter.h"
#include "Utils.h"
#include "minhook/MinHook.h"

// Minimal D3D8 declarations. The current SDK installation does not ship
// the legacy d3d8.h header, but these fields are stable for diagnostics.
struct IDirect3D8;
struct IDirect3DDevice8;
using D3DDEVTYPE = unsigned int;
using D3DFORMAT = unsigned int;
using D3DMULTISAMPLE_TYPE = unsigned int;
using D3DSWAPEFFECT = unsigned int;
struct D3DPRESENT_PARAMETERS
{
    UINT BackBufferWidth;
    UINT BackBufferHeight;
    UINT BackBufferFormat;
    UINT BackBufferCount;
    UINT MultiSampleType;
    UINT SwapEffect;
    HWND hDeviceWindow;
    BOOL Windowed;
    BOOL EnableAutoDepthStencil;
    UINT AutoDepthStencilFormat;
    DWORD Flags;
    UINT FullScreen_RefreshRateInHz;
    UINT FullScreen_PresentationInterval;
};

namespace
{
    bool g_logText = false;
    bool g_logMetrics = false;
    bool g_targetTextWidthOnly = false;
    bool g_doubleTargetWidth = false;
    bool g_narrowTargetFont = false;
    int g_targetFontScalePercent = 100;
    bool g_reflowTargetText = false;
    bool g_patchOnly = false;
    bool g_patchTrace = false;
    bool g_captureShipData = false;
    bool g_forceSystemCp932 = false;
    volatile LONG g_shipLookupHookInstalled = 0;
    UINT g_patchTextCodePage = 936;
    bool g_debugWindowEnabled = false;
    HWND g_debugWindow = nullptr;
    DWORD g_debugThreadId = 0;
    volatile LONG g_debugWindowStartState = 0;
    HANDLE g_debugWindowInstanceMutex = nullptr;
    volatile LONG g_debugSourceCodePage = 932;
    CRITICAL_SECTION g_debugLock;
    bool g_debugLockInitialized = false;
    struct TranslationEntry
    {
        uint32_t key = 0;
        std::wstring text;
        // Optional source template.  A template may contain {0}, {1}, ...
        // for dynamic fields that must be copied back unchanged.
        std::wstring source;
        // Only enabled for templates whose captured values should also be
        // looked up as independent CP932 translations.
        bool translateCaptures = false;
    };
    std::vector<TranslationEntry> g_translations;
    struct DecodeAlias
    {
        uint32_t convertedKey = 0;
        std::wstring originalText;
    };
    std::vector<DecodeAlias> g_decodeAliases;
    std::string g_translationPath;
    std::string g_cachedParagraph;
    int g_cachedParagraphCount = 0;
    bool g_reflowDrawn = false;
    __declspec(thread) bool g_inHook = false;

    using GetSystemMetricsFn = int (WINAPI*)(int);
    using GetACPFn = UINT (WINAPI*)();
    using GetOEMCPFn = UINT (WINAPI*)();
    using GetDeviceCapsFn = int (WINAPI*)(HDC, int);
    using GetClientRectFn = BOOL (WINAPI*)(HWND, LPRECT);
    using TextOutAFn = BOOL (WINAPI*)(HDC, int, int, LPCSTR, int);
    using TextOutWFn = BOOL (WINAPI*)(HDC, int, int, LPCWSTR, int);
    using GetTextExtentPoint32AFn = BOOL (WINAPI*)(HDC, LPCSTR, int, LPSIZE);
    using GetTextExtentPoint32WFn = BOOL (WINAPI*)(HDC, LPCWSTR, int, LPSIZE);
    using GetTextMetricsAFn = BOOL (WINAPI*)(HDC, LPTEXTMETRICA);
    using DrawTextAFn = int (WINAPI*)(HDC, LPCSTR, int, LPRECT, UINT);
    using DrawTextWFn = int (WINAPI*)(HDC, LPCWSTR, int, LPRECT, UINT);
    using ShipLookupFn = int (__thiscall*)(void*, int, int);
    using MultiByteToWideCharFn = int (WINAPI*)(UINT, DWORD, LPCCH, int, LPWSTR, int);
    using CreateFontAFn = HFONT (WINAPI*)(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, LPCSTR);
    using CreateFontIndirectAFn = HFONT (WINAPI*)(const LOGFONTA*);
    using BitBltFn = BOOL (WINAPI*)(HDC, int, int, int, int, HDC, int, int, DWORD);
    using StretchBltFn = BOOL (WINAPI*)(HDC, int, int, int, int, HDC, int, int, int, int, DWORD);
    using SetMapModeFn = int (WINAPI*)(HDC, int);
    using SetViewportOrgExFn = BOOL (WINAPI*)(HDC, int, int, LPPOINT);
    using SetWindowExtExFn = BOOL (WINAPI*)(HDC, int, int, LPSIZE);
    using SetViewportExtExFn = BOOL (WINAPI*)(HDC, int, int, LPSIZE);
    using Direct3DCreate8Fn = IDirect3D8* (WINAPI*)(UINT);
    using D3DCreateDeviceFn = HRESULT (STDMETHODCALLTYPE*)(IDirect3D8*, UINT, D3DDEVTYPE,
        HWND, DWORD, D3DPRESENT_PARAMETERS*, IDirect3DDevice8**);

    GetSystemMetricsFn g_getSystemMetrics = nullptr;
    GetACPFn g_getACP = nullptr;
    GetOEMCPFn g_getOEMCP = nullptr;
    GetDeviceCapsFn g_getDeviceCaps = nullptr;
    GetClientRectFn g_getClientRect = nullptr;
    TextOutAFn g_textOutA = nullptr;
    TextOutWFn g_textOutW = nullptr;
    GetTextExtentPoint32AFn g_getTextExtentA = nullptr;
    GetTextExtentPoint32WFn g_getTextExtentW = nullptr;
    GetTextMetricsAFn g_getTextMetricsA = nullptr;
    DrawTextAFn g_drawTextA = nullptr;
    DrawTextWFn g_drawTextW = nullptr;
    ShipLookupFn g_shipLookupOriginal = nullptr;
    MultiByteToWideCharFn g_multiByteToWideChar = nullptr;
    CreateFontAFn g_createFontA = nullptr;
    CreateFontIndirectAFn g_createFontIndirectA = nullptr;
    BitBltFn g_bitBlt = nullptr;
    StretchBltFn g_stretchBlt = nullptr;
    SetMapModeFn g_setMapMode = nullptr;
    SetViewportOrgExFn g_setViewportOrgEx = nullptr;
    SetWindowExtExFn g_setWindowExtEx = nullptr;
    SetViewportExtExFn g_setViewportExtEx = nullptr;
    Direct3DCreate8Fn g_direct3DCreate8 = nullptr;
    D3DCreateDeviceFn g_d3dCreateDevice = nullptr;
    bool g_d3dCreateDeviceHooked = false;
    bool g_d3d8FrameTrace = false;
    bool g_forceXpTextMetrics = false;
    bool g_layoutTunerEnabled = false;
    bool g_layoutTunerOnly = false;
    volatile LONG g_xpDrawEvents = 0;
    wchar_t g_patchParagraph[8192] = {};
    int g_patchParagraphCount = 0;
    wchar_t g_patchOriginalParagraph[8192] = {};
    int g_patchOriginalParagraphCount = 0;
    uint32_t g_patchParagraphKey = 0;
    std::string g_patchParagraphSourceBytes;
    HDC g_patchParagraphDc = nullptr;
    LONG g_patchParagraphLeft = 0;
    size_t g_patchParagraphNextByte = 0;
    ULONGLONG g_patchParagraphDeadline = 0;
    bool g_patchParagraphPending = false;
    bool g_patchParagraphDrawn = false;
    LONG g_patchLayoutVerticalOffset = 0;
    LONG g_patchLayoutLastOriginalFirstTop = 0;
    LONG g_patchLayoutLastOriginalLastTop = 0;
    LONG g_patchLayoutLastRenderedHeight = 0;
    LONG g_patchLayoutLastLineHeight = 15;
    bool g_patchLayoutHasLast = false;

    struct DebugSnapshot
    {
        wchar_t text[2048] = {};
        wchar_t source[4096] = {};
        LONG left = 0;
        LONG top = 0;
        LONG right = 0;
        LONG bottom = 0;
        int count = 0;
        UINT format = 0;
        int state = 0;
        uint32_t key = 0;
    };

    // Source provenance is deliberately kept separate from the translation
    // cache.  It is a diagnostic index built from resource reads observed
    // after the hooks are installed, not a second translation database.
    struct SourceHandleContext
    {
        std::wstring path;
        ULONGLONG cursor = 0;
        bool cursorKnown = true;
    };

    struct SourceMappingContext
    {
        std::wstring path;
        ULONGLONG fileSize = 0;
    };

    struct SourceMappedViewContext
    {
        HANDLE mapping = nullptr;
        std::wstring path;
        ULONGLONG offset = 0;
        SIZE_T length = 0;
    };

    struct SourceReadSnapshot
    {
        std::wstring path;
        ULONGLONG offset = 0;
        bool offsetKnown = false;
        std::string bytes;
        ULONGLONG touch = 0;
    };

    struct SourceCandidate
    {
        std::wstring path;
        ULONGLONG offset = 0;
        bool offsetKnown = false;
    };

    struct SourceProvenanceSpan
    {
        int byteStart = 0;
        int byteLength = 0;
        bool unknown = false;
        bool ambiguous = false;
        std::vector<SourceCandidate> candidates;
    };

    std::map<HANDLE, SourceHandleContext> g_sourceHandles;
    std::map<HANDLE, SourceMappingContext> g_sourceMappings;
    std::map<LPCVOID, SourceMappedViewContext> g_sourceViews;
    std::vector<SourceReadSnapshot> g_sourceSnapshots;
    SRWLOCK g_sourceLock = SRWLOCK_INIT;
    volatile LONG g_sourceTrackingEnabled = 0;
    ULONGLONG g_sourceTouch = 0;
    size_t g_sourceCacheBytes = 0;
    constexpr size_t kSourcePerFileLimit = 512 * 1024;
    constexpr size_t kSourceGlobalLimit = 8 * 1024 * 1024;

    bool IsSourceTrackingEnabledInternal()
    {
        return InterlockedCompareExchange(&g_sourceTrackingEnabled, 0, 0) != 0;
    }

    std::wstring LowercasePath(std::wstring value)
    {
        for (wchar_t& ch : value)
        {
            if (ch == L'/')
                ch = L'\\';
            ch = static_cast<wchar_t>(towlower(ch));
        }
        return value;
    }

    std::wstring NormalizeSourcePath(const std::wstring& input)
    {
        if (input.empty())
            return std::wstring();
        if (input == L"NUL" || input == L"CON" || input.rfind(L"\\\\.\\", 0) == 0 ||
            input.rfind(L"\\Device\\", 0) == 0 || input.rfind(L"\\\\?\\GLOBALROOT", 0) == 0)
            return std::wstring();

        std::wstring path = input;
        if (path.rfind(L"\\\\?\\", 0) == 0)
            path.erase(0, 4);
        wchar_t modulePath[32768] = {};
        DWORD moduleLength = GetModuleFileNameW(nullptr, modulePath, ARRAYSIZE(modulePath));
        std::wstring root;
        if (moduleLength > 0 && moduleLength < ARRAYSIZE(modulePath))
        {
            root.assign(modulePath, moduleLength);
            const size_t slash = root.find_last_of(L"\\/");
            if (slash != std::wstring::npos)
                root.resize(slash + 1);
        }

        const bool relative = !(path.size() >= 2 && path[1] == L':') &&
            path.rfind(L"\\\\", 0) != 0;
        if (relative && !root.empty())
            path = root + path;
        wchar_t fullPath[32768] = {};
        DWORD length = GetFullPathNameW(path.c_str(), ARRAYSIZE(fullPath), fullPath, nullptr);
        if (length > 0 && length < ARRAYSIZE(fullPath))
            path.assign(fullPath, length);

        const std::wstring lowerPath = LowercasePath(path);
        const std::wstring lowerRoot = LowercasePath(root);
        std::wstring display = path;
        if (!lowerRoot.empty() && lowerPath.rfind(lowerRoot, 0) == 0)
            display = path.substr(root.size());
        while (!display.empty() && (display.front() == L'\\' || display.front() == L'/'))
            display.erase(display.begin());
        for (wchar_t& ch : display)
            if (ch == L'\\')
                ch = L'/';
        return display;
    }

    void RemoveSourceSnapshotLocked(size_t index)
    {
        if (index >= g_sourceSnapshots.size())
            return;
        g_sourceCacheBytes -= g_sourceSnapshots[index].bytes.size();
        g_sourceSnapshots.erase(g_sourceSnapshots.begin() + index);
    }

    void EnforceSourceCacheLimitsLocked(const std::wstring& path)
    {
        size_t pathBytes = 0;
        for (const SourceReadSnapshot& snapshot : g_sourceSnapshots)
            if (snapshot.path == path)
                pathBytes += snapshot.bytes.size();
        while (pathBytes > kSourcePerFileLimit)
        {
            size_t oldest = g_sourceSnapshots.size();
            for (size_t i = 0; i < g_sourceSnapshots.size(); ++i)
            {
                if (g_sourceSnapshots[i].path == path &&
                    (oldest == g_sourceSnapshots.size() ||
                        g_sourceSnapshots[i].touch < g_sourceSnapshots[oldest].touch))
                    oldest = i;
            }
            if (oldest == g_sourceSnapshots.size())
                break;
            pathBytes -= g_sourceSnapshots[oldest].bytes.size();
            RemoveSourceSnapshotLocked(oldest);
        }
        while (g_sourceCacheBytes > kSourceGlobalLimit && !g_sourceSnapshots.empty())
        {
            size_t oldest = 0;
            for (size_t i = 1; i < g_sourceSnapshots.size(); ++i)
                if (g_sourceSnapshots[i].touch < g_sourceSnapshots[oldest].touch)
                    oldest = i;
            RemoveSourceSnapshotLocked(oldest);
        }
    }

    void MergeSourceSnapshotLocked(const std::wstring& path, ULONGLONG offset,
        bool offsetKnown, const char* bytes, size_t length)
    {
        if (path.empty() || !bytes || length == 0)
            return;
        if (length > kSourcePerFileLimit)
            length = kSourcePerFileLimit;

        SourceReadSnapshot incoming;
        incoming.path = path;
        incoming.offset = offset;
        incoming.offsetKnown = offsetKnown;
        incoming.bytes.assign(bytes, length);
        incoming.touch = ++g_sourceTouch;

        if (offsetKnown)
        {
            for (size_t i = 0; i < g_sourceSnapshots.size(); )
            {
                SourceReadSnapshot& existing = g_sourceSnapshots[i];
                if (existing.path != path || !existing.offsetKnown)
                {
                    ++i;
                    continue;
                }
                const ULONGLONG incomingEnd = incoming.offset + incoming.bytes.size();
                const ULONGLONG existingEnd = existing.offset + existing.bytes.size();
                if (incomingEnd < existing.offset || existingEnd < incoming.offset)
                {
                    ++i;
                    continue;
                }

                const ULONGLONG start = (std::min)(incoming.offset, existing.offset);
                const ULONGLONG end = (std::max)(incomingEnd, existingEnd);
                std::string merged(static_cast<size_t>(end - start), '\0');
                memcpy(&merged[static_cast<size_t>(existing.offset - start)],
                    existing.bytes.data(), existing.bytes.size());
                memcpy(&merged[static_cast<size_t>(incoming.offset - start)],
                    incoming.bytes.data(), incoming.bytes.size());
                incoming.offset = start;
                incoming.bytes.swap(merged);
                RemoveSourceSnapshotLocked(i);
                continue;
            }
        }

        g_sourceCacheBytes += incoming.bytes.size();
        g_sourceSnapshots.push_back(std::move(incoming));
        EnforceSourceCacheLimitsLocked(path);
    }

    std::vector<SourceCandidate> FindSourceCandidatesLocked(const char* bytes, int length)
    {
        std::vector<SourceCandidate> result;
        if (!bytes || length <= 0)
            return result;
        for (const SourceReadSnapshot& snapshot : g_sourceSnapshots)
        {
            const char* begin = snapshot.bytes.data();
            const char* end = begin + snapshot.bytes.size();
            const char* found = std::search(begin, end, bytes, bytes + length);
            while (found != end)
            {
                SourceCandidate candidate;
                candidate.path = snapshot.path;
                candidate.offsetKnown = snapshot.offsetKnown;
                candidate.offset = snapshot.offsetKnown
                    ? snapshot.offset + static_cast<ULONGLONG>(found - begin) : 0;
                bool duplicate = false;
                for (const SourceCandidate& prior : result)
                {
                    if (prior.path == candidate.path &&
                        prior.offsetKnown == candidate.offsetKnown &&
                        (!prior.offsetKnown || prior.offset == candidate.offset))
                    {
                        duplicate = true;
                        break;
                    }
                }
                if (!duplicate)
                    result.push_back(candidate);
                found = std::search(found + 1, end, bytes, bytes + length);
            }
        }
        return result;
    }

    std::wstring FormatSourceCandidate(const SourceCandidate& candidate)
    {
        wchar_t line[768] = {};
        if (candidate.offsetKnown)
        {
            swprintf_s(line, ARRAYSIZE(line), L"%s @0x%llX",
                candidate.path.c_str(), candidate.offset);
        }
        else
        {
            swprintf_s(line, ARRAYSIZE(line), L"%s @offset unknown",
                candidate.path.c_str());
        }
        return line;
    }

    std::wstring FormatSourceProvenance(const std::vector<SourceProvenanceSpan>& spans)
    {
        if (spans.empty())
            return L"Source: source unknown";
        bool hasUnknown = false;
        bool hasAmbiguous = false;
        for (const SourceProvenanceSpan& span : spans)
        {
            hasUnknown = hasUnknown || span.unknown;
            hasAmbiguous = hasAmbiguous || span.ambiguous;
        }
        std::wstring result = spans.size() == 1 && !hasUnknown && !hasAmbiguous
            ? L"Source: " : L"Source spans:\r\n";
        for (size_t i = 0; i < spans.size(); ++i)
        {
            const SourceProvenanceSpan& span = spans[i];
            if (i > 0 || spans.size() > 1)
                result += L"[" + std::to_wstring(span.byteStart) + L".." +
                    std::to_wstring(span.byteStart + span.byteLength - 1) + L"] ";
            if (span.unknown || span.candidates.empty())
            {
                result += L"source unknown";
            }
            else if (span.ambiguous)
            {
                result += L"source ambiguous: ";
                for (size_t c = 0; c < span.candidates.size(); ++c)
                {
                    if (c > 0)
                        result += L" | ";
                    result += FormatSourceCandidate(span.candidates[c]);
                }
            }
            else
            {
                result += FormatSourceCandidate(span.candidates.front());
            }
            if (spans.size() > 1)
                result += L"\r\n";
        }
        return result;
    }

    std::wstring ResolveSourceProvenance(const char* bytes, int length)
    {
        if (!bytes || length <= 0)
            return L"Source: source unknown";

        std::vector<SourceProvenanceSpan> spans;
        AcquireSRWLockShared(&g_sourceLock);
        std::vector<SourceCandidate> full = FindSourceCandidatesLocked(bytes, length);
        if (!full.empty())
        {
            SourceProvenanceSpan span;
            span.byteLength = length;
            span.ambiguous = full.size() > 1;
            span.candidates = std::move(full);
            spans.push_back(std::move(span));
        }
        else
        {
            // Longest-match-first keeps a sentence composed from multiple
            // reads useful while avoiding a flood of one-character matches.
            int position = 0;
            int unknownStart = -1;
            while (position < length)
            {
                int longestLength = 0;
                std::vector<SourceCandidate> longest;
                for (int candidateLength = length - position;
                    candidateLength >= 3; --candidateLength)
                {
                    std::vector<SourceCandidate> candidates =
                        FindSourceCandidatesLocked(bytes + position, candidateLength);
                    if (!candidates.empty())
                    {
                        longestLength = candidateLength;
                        longest = std::move(candidates);
                        break;
                    }
                }
                if (longestLength == 0)
                {
                    if (unknownStart < 0)
                        unknownStart = position;
                    ++position;
                    continue;
                }
                if (unknownStart >= 0)
                {
                    SourceProvenanceSpan unknown;
                    unknown.byteStart = unknownStart;
                    unknown.byteLength = position - unknownStart;
                    unknown.unknown = true;
                    spans.push_back(std::move(unknown));
                    unknownStart = -1;
                }
                SourceProvenanceSpan span;
                span.byteStart = position;
                span.byteLength = longestLength;
                span.ambiguous = longest.size() > 1;
                span.candidates = std::move(longest);
                spans.push_back(std::move(span));
                position += longestLength;
            }
            if (unknownStart >= 0)
            {
                SourceProvenanceSpan unknown;
                unknown.byteStart = unknownStart;
                unknown.byteLength = length - unknownStart;
                unknown.unknown = true;
                spans.push_back(std::move(unknown));
            }
        }
        ReleaseSRWLockShared(&g_sourceLock);
        return FormatSourceProvenance(spans);
    }

    constexpr int kDebugHistoryMaxCapacity = 500;
    DebugSnapshot g_debugHistory[kDebugHistoryMaxCapacity] = {};
    int g_debugHistoryCapacity = 200;
    int g_debugHistoryCount = 0;
    int g_debugHistoryNext = 0;
    int g_debugSelectedSlot = -1;
    bool g_debugFollowLatest = true;
    bool g_debugHideTranslated = false;
    DebugSnapshot g_debugSnapshot;
    constexpr UINT kDebugUpdate = WM_APP + 101;
    constexpr int kDebugHistoryList = 1002;
    constexpr int kDebugHideTranslatedButton = 1013;
    constexpr int kDebugTranslationEdit = 1005;
    constexpr int kDebugApplyTranslationButton = 1006;
    constexpr int kDebugManualSaveButton = 1009;
    constexpr int kDebugManualSourceEdit = 1010;
    constexpr int kDebugCopyCurrentButton = 1011;
    constexpr int kDebugTranslateCapturesCheckbox = 1014;
    constexpr int kDebugHistoryCapacityCombo = 1015;
    constexpr int kDebugSourceInfo = 1016;
    constexpr int kDebugSourceTrackingButton = 1017;
    constexpr int kDebugSourceClearButton = 1018;

    std::string CopyWideText(LPCWSTR text, int count);
    std::string CopyAnsiHex(LPCSTR text, int count);
    bool WideToCodePageBytes(UINT codePage, const std::wstring& text,
        std::string& output);
    void CaptureShipTextA(LPCSTR text, int count, LPRECT rect, UINT format,
        uintptr_t caller, LPCWSTR original, int originalCount,
        LPCWSTR rendered, int renderedCount, uint32_t key);

#ifdef WSC3_MINIMAL_RELEASE
#define WSC3_PUBLISH_DEBUG_SNAPSHOT(...) ((void)0)
#else
#define WSC3_PUBLISH_DEBUG_SNAPSHOT(...) PublishDebugSnapshot(__VA_ARGS__)
#endif

#ifdef WSC3_MINIMAL_RELEASE
#define WSC3_PATCH_TRACE(...) ((void)0)
#else
#define WSC3_PATCH_TRACE(...) do { if (g_patchTrace) log(__VA_ARGS__); } while (0)
#endif

    int HistorySlotAtDisplayIndexLocked(int index)
    {
        if (index < 0 || index >= g_debugHistoryCount)
            return -1;
        int slot = g_debugHistoryNext - 1 - index;
        while (slot < 0)
            slot += g_debugHistoryCapacity;
        return slot % g_debugHistoryCapacity;
    }

    void SetDebugHistoryCapacityLocked(int capacity)
    {
        if (capacity < 100 || capacity > kDebugHistoryMaxCapacity ||
            (capacity % 100) != 0 || capacity == g_debugHistoryCapacity)
            return;

        const int keep = g_debugHistoryCount < capacity
            ? g_debugHistoryCount : capacity;
        std::vector<DebugSnapshot> preserved;
        preserved.reserve(keep);
        for (int displayIndex = keep - 1; displayIndex >= 0; --displayIndex)
        {
            const int slot = HistorySlotAtDisplayIndexLocked(displayIndex);
            if (slot >= 0)
                preserved.push_back(g_debugHistory[slot]);
        }

        g_debugHistoryCapacity = capacity;
        for (int index = 0; index < static_cast<int>(preserved.size()); ++index)
            g_debugHistory[index] = preserved[index];
        g_debugHistoryCount = static_cast<int>(preserved.size());
        g_debugHistoryNext = g_debugHistoryCount % g_debugHistoryCapacity;
        g_debugSelectedSlot = g_debugHistoryCount > 0
            ? g_debugHistoryCount - 1 : -1;
        g_debugFollowLatest = true;
        if (g_debugHistoryCount > 0)
            g_debugSnapshot = g_debugHistory[g_debugHistoryCount - 1];
    }

    uint32_t HashWideText(LPCWSTR text, int count)
    {
        uint32_t hash = 2166136261u;
        if (!text)
            return hash;
        if (count < 0)
            count = lstrlenW(text);
        for (int i = 0; i < count; ++i)
        {
            const uint16_t ch = static_cast<uint16_t>(text[i]);
            hash ^= static_cast<uint8_t>(ch & 0xff);
            hash *= 16777619u;
            hash ^= static_cast<uint8_t>(ch >> 8);
            hash *= 16777619u;
        }
        return hash;
    }

    uint32_t HashRawText(LPCSTR text, int count)
    {
        uint32_t hash = 2166136261u;
        if (!text)
            return hash;
        if (count < 0)
            count = static_cast<int>(strlen(text));
        for (int i = 0; i < count; ++i)
        {
            hash ^= static_cast<unsigned char>(text[i]);
            hash *= 16777619u;
        }
        return hash;
    }

    std::wstring FindTranslation(uint32_t key)
    {
        for (const auto& entry : g_translations)
            if (entry.key == key)
                return entry.text;
        return std::wstring();
    }

    bool ParseTemplatePlaceholder(const std::wstring& pattern, size_t open,
        size_t& close, int& index)
    {
        if (open >= pattern.size() || pattern[open] != L'{')
            return false;
        size_t pos = open + 1;
        if (pos >= pattern.size() || pattern[pos] < L'0' || pattern[pos] > L'9')
            return false;
        int value = 0;
        while (pos < pattern.size() && pattern[pos] >= L'0' && pattern[pos] <= L'9')
        {
            value = value * 10 + (pattern[pos] - L'0');
            if (value > 31)
                return false;
            ++pos;
        }
        if (pos >= pattern.size() || pattern[pos] != L'}')
            return false;
        close = pos;
        index = value;
        return true;
    }

    bool HasTemplatePlaceholder(const std::wstring& source)
    {
        size_t open = source.find(L'{');
        while (open != std::wstring::npos)
        {
            size_t close = 0;
            int index = 0;
            if (ParseTemplatePlaceholder(source, open, close, index))
                return true;
            open = source.find(L'{', open + 1);
        }
        return false;
    }

    bool MatchSourceTemplate(const std::wstring& pattern,
        const std::wstring& value, std::vector<std::wstring>& captures)
    {
        size_t patternPos = 0;
        size_t valuePos = 0;
        while (patternPos < pattern.size())
        {
            const size_t open = pattern.find(L'{', patternPos);
            if (open == std::wstring::npos)
            {
                const std::wstring literal = pattern.substr(patternPos);
                if (value.compare(valuePos, literal.size(), literal) != 0 ||
                    valuePos + literal.size() != value.size())
                    return false;
                return true;
            }

            const std::wstring literal = pattern.substr(patternPos, open - patternPos);
            if (value.compare(valuePos, literal.size(), literal) != 0)
                return false;
            valuePos += literal.size();

            size_t close = 0;
            int index = 0;
            if (!ParseTemplatePlaceholder(pattern, open, close, index))
                return false;
            const size_t nextOpen = pattern.find(L'{', close + 1);
            const size_t nextEnd = nextOpen == std::wstring::npos
                ? pattern.size() : nextOpen;
            const std::wstring nextLiteral = pattern.substr(close + 1,
                nextEnd - close - 1);
            size_t captureEnd = value.size();
            if (!nextLiteral.empty())
            {
                captureEnd = value.find(nextLiteral, valuePos);
                if (captureEnd == std::wstring::npos)
                    return false;
            }
            if (index >= static_cast<int>(captures.size()))
                captures.resize(index + 1);
            const std::wstring captured = value.substr(valuePos,
                captureEnd - valuePos);
            if (!captures[index].empty() && captures[index] != captured)
                return false;
            captures[index] = captured;
            valuePos = captureEnd;
            patternPos = close + 1;
        }
        return valuePos == value.size();
    }

    std::wstring TranslateCapturedValue(const std::wstring& value)
    {
        if (value.empty())
            return value;
        std::string raw;
        if (!WideToCodePageBytes(932, value, raw) || raw.empty())
            return value;
        const uint32_t key = HashRawText(raw.data(), static_cast<int>(raw.size()));
        const std::wstring translated = FindTranslation(key);
        return translated.empty() ? value : translated;
    }

    std::wstring ApplyTranslationTemplate(const std::wstring& translation,
        const std::vector<std::wstring>& captures, bool translateCaptures)
    {
        std::wstring result;
        size_t pos = 0;
        while (pos < translation.size())
        {
            const size_t open = translation.find(L'{', pos);
            if (open == std::wstring::npos)
            {
                result += translation.substr(pos);
                break;
            }
            result += translation.substr(pos, open - pos);
            size_t close = 0;
            int index = 0;
            if (!ParseTemplatePlaceholder(translation, open, close, index) ||
                index >= static_cast<int>(captures.size()))
            {
                return std::wstring();
            }
            result += translateCaptures
                ? TranslateCapturedValue(captures[index])
                : captures[index];
            pos = close + 1;
        }
        return result;
    }

    std::wstring FindTemplateTranslation(const std::wstring& source)
    {
        std::wstring best;
        size_t bestLiteralLength = 0;
        for (const auto& entry : g_translations)
        {
            if (entry.source.find(L'{') == std::wstring::npos)
                continue;
            std::vector<std::wstring> captures;
            if (!MatchSourceTemplate(entry.source, source, captures))
                continue;
            const size_t literalLength = entry.source.size();
            const std::wstring replacement = ApplyTranslationTemplate(
                entry.text, captures, entry.translateCaptures);
            if (!replacement.empty() && literalLength >= bestLiteralLength)
            {
                best = replacement;
                bestLiteralLength = literalLength;
            }
        }
        return best;
    }

    bool HasCaptureTranslationTemplate(const std::wstring& source)
    {
        for (const auto& entry : g_translations)
        {
            if (!entry.translateCaptures ||
                entry.source.find(L'{') == std::wstring::npos)
                continue;
            std::vector<std::wstring> captures;
            if (MatchSourceTemplate(entry.source, source, captures))
                return true;
        }
        return false;
    }

    void UpsertTranslation(uint32_t key, const std::wstring& text,
        const std::wstring& source = std::wstring(),
        bool translateCaptures = false)
    {
        const std::wstring storedSource = HasTemplatePlaceholder(source)
            ? source : std::wstring();
        for (auto& entry : g_translations)
        {
            if (entry.key == key &&
                (storedSource.empty() || entry.source.empty() || entry.source == storedSource))
            {
                entry.text = text;
                entry.source = storedSource;
                entry.translateCaptures = !storedSource.empty() && translateCaptures;
                return;
            }
        }
        TranslationEntry entry;
        entry.key = key;
        entry.text = text;
        entry.source = storedSource;
        entry.translateCaptures = !storedSource.empty() && translateCaptures;
        g_translations.push_back(entry);
    }

    const std::wstring* FindDecodeAlias(uint32_t convertedKey)
    {
        for (const auto& alias : g_decodeAliases)
            if (alias.convertedKey == convertedKey)
                return &alias.originalText;
        return nullptr;
    }

    void UpsertDecodeAlias(uint32_t convertedKey, const std::wstring& originalText)
    {
        if (!convertedKey || originalText.empty())
            return;
        for (auto& alias : g_decodeAliases)
        {
            if (alias.convertedKey == convertedKey)
            {
                alias.originalText = originalText;
                return;
            }
        }
        DecodeAlias alias;
        alias.convertedKey = convertedKey;
        alias.originalText = originalText;
        g_decodeAliases.push_back(alias);
    }

    std::string WideToUtf8(const std::wstring& text)
    {
        if (text.empty())
            return std::string();
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(),
            static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0)
            return std::string();
        std::string result(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(),
            static_cast<int>(text.size()), &result[0], size, nullptr, nullptr);
        return result;
    }

    std::wstring Utf8ToWide(const std::string& text)
    {
        if (text.empty())
            return std::wstring();
        int size = MultiByteToWideChar(CP_UTF8, 0, text.data(),
            static_cast<int>(text.size()), nullptr, 0);
        if (size <= 0)
            return std::wstring();
        std::wstring result(size, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(),
            static_cast<int>(text.size()), &result[0], size);
        return result;
    }

    std::string JsonEscapeUtf8(const std::wstring& text)
    {
        const std::string utf8 = WideToUtf8(text);
        std::string result;
        for (unsigned char ch : utf8)
        {
            if (ch == '\\') result += "\\\\";
            else if (ch == '"') result += "\\\"";
            else if (ch == '\r') result += "\\r";
            else if (ch == '\n') result += "\\n";
            else if (ch == '\t') result += "\\t";
            else result.push_back(static_cast<char>(ch));
        }
        return result;
    }

    bool WideToCodePageBytes(UINT codePage, const std::wstring& text, std::string& output)
    {
        int size = WideCharToMultiByte(codePage, 0, text.c_str(),
            static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0)
            return false;
        output.resize(size);
        return WideCharToMultiByte(codePage, 0, text.c_str(),
            static_cast<int>(text.size()), &output[0], size, nullptr, nullptr) > 0;
    }

    std::string JsonUnescapeUtf8(const std::string& text)
    {
        std::string result;
        bool escaped = false;
        for (size_t i = 0; i < text.size(); ++i)
        {
            char ch = text[i];
            if (!escaped)
            {
                if (ch == '\\') escaped = true;
                else result.push_back(ch);
                continue;
            }
            escaped = false;
            if (ch == 'n') result.push_back('\n');
            else if (ch == 'r') result.push_back('\r');
            else if (ch == 't') result.push_back('\t');
            else result.push_back(ch);
        }
        return result;
    }

    size_t FindJsonObjectEnd(const std::string& data, size_t start)
    {
        bool inString = false;
        bool escaped = false;
        for (size_t i = start; i < data.size(); ++i)
        {
            const char ch = data[i];
            if (inString)
            {
                if (escaped)
                    escaped = false;
                else if (ch == '\\')
                    escaped = true;
                else if (ch == '"')
                    inString = false;
                continue;
            }
            if (ch == '"')
                inString = true;
            else if (ch == '}')
                return i;
        }
        return std::string::npos;
    }

    void ResolveTranslationPath()
    {
        char modulePath[MAX_PATH] = {};
        DWORD length = GetModuleFileNameA(nullptr, modulePath, ARRAYSIZE(modulePath));
        if (length == 0 || length >= ARRAYSIZE(modulePath))
        {
            g_translationPath = "chinese\\translations.json";
            CreateDirectoryA("chinese", nullptr);
            return;
        }
        char* slash = strrchr(modulePath, '\\');
        if (slash)
            slash[1] = '\0';
        const std::string directory = std::string(modulePath) + "chinese";
        CreateDirectoryA(directory.c_str(), nullptr);
        g_translationPath = directory + "\\translations.json";
    }

    void LoadTranslations()
    {
        if (g_translationPath.empty())
            ResolveTranslationPath();
        FILE* file = nullptr;
        if (fopen_s(&file, g_translationPath.c_str(), "rb") != 0 || !file)
            return;
        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);
        std::string data;
        if (size > 0)
        {
            data.resize(static_cast<size_t>(size));
            fread(&data[0], 1, data.size(), file);
        }
        fclose(file);

        size_t pos = 0;
        while ((pos = data.find("\"hash\":\"", pos)) != std::string::npos)
        {
            pos += 8;
            size_t hashEnd = data.find('"', pos);
            size_t textKey = data.find("\"text\":\"", hashEnd);
            if (hashEnd == std::string::npos || textKey == std::string::npos)
                break;
            unsigned int key = 0;
            sscanf_s(data.substr(pos, hashEnd - pos).c_str(), "%x", &key);
            size_t textStart = textKey + 8;
            size_t textEnd = textStart;
            bool escaped = false;
            for (; textEnd < data.size(); ++textEnd)
            {
                if (!escaped && data[textEnd] == '"')
                    break;
                if (!escaped && data[textEnd] == '\\') escaped = true;
                else escaped = false;
            }
            if (textEnd >= data.size())
                break;
            const std::wstring translation = Utf8ToWide(
                JsonUnescapeUtf8(data.substr(textStart, textEnd - textStart)));
            std::wstring source;
            const size_t objectEnd = FindJsonObjectEnd(data, textEnd + 1);
            const size_t sourceKey = data.find("\"source\":\"", textEnd + 1);
            bool translateCaptures = false;
            if (sourceKey != std::string::npos && objectEnd != std::string::npos &&
                sourceKey < objectEnd)
            {
                const size_t sourceStart = sourceKey + 10;
                size_t sourceEnd = sourceStart;
                bool sourceEscaped = false;
                for (; sourceEnd < data.size(); ++sourceEnd)
                {
                    if (!sourceEscaped && data[sourceEnd] == '"')
                        break;
                    if (!sourceEscaped && data[sourceEnd] == '\\')
                        sourceEscaped = true;
                    else
                        sourceEscaped = false;
                }
                if (sourceEnd < data.size())
                {
                    source = Utf8ToWide(JsonUnescapeUtf8(
                        data.substr(sourceStart, sourceEnd - sourceStart)));
                    if (!HasTemplatePlaceholder(source))
                        source.clear();
                }
            }
            const std::string captureKey = "\"translate_captures\":";
            const size_t captureKeyPos = data.find(captureKey, textEnd + 1);
            if (captureKeyPos != std::string::npos &&
                objectEnd != std::string::npos && captureKeyPos < objectEnd)
            {
                size_t valuePos = captureKeyPos + captureKey.size();
                while (valuePos < objectEnd &&
                    (data[valuePos] == ' ' || data[valuePos] == '\t'))
                    ++valuePos;
                translateCaptures = data.compare(valuePos, 4, "true") == 0;
            }
            UpsertTranslation(static_cast<uint32_t>(key), translation, source,
                translateCaptures);
            pos = textEnd + 1;
        }
    }

    bool SaveTranslations()
    {
        if (g_translationPath.empty())
            ResolveTranslationPath();
        FILE* file = nullptr;
        if (fopen_s(&file, g_translationPath.c_str(), "wb") != 0 || !file)
            return false;
        fputs("{\n  \"translations\": [\n", file);
        for (size_t i = 0; i < g_translations.size(); ++i)
        {
            char line[64] = {};
            sprintf_s(line, "    {\"hash\":\"%08X\",\"text\":\"",
                g_translations[i].key);
            fputs(line, file);
            const std::string escaped = JsonEscapeUtf8(g_translations[i].text);
            fwrite(escaped.data(), 1, escaped.size(), file);
            fputs("\",\"source\":\"", file);
            const std::wstring storedSource = HasTemplatePlaceholder(
                g_translations[i].source) ? g_translations[i].source : std::wstring();
            const std::string escapedSource = JsonEscapeUtf8(storedSource);
            fwrite(escapedSource.data(), 1, escapedSource.size(), file);
            fputs("\"", file);
            if (g_translations[i].translateCaptures && !storedSource.empty())
                fputs(",\"translate_captures\":true", file);
            fputs(i + 1 == g_translations.size() ? "}\n" : "},\n", file);
        }
        fputs("  ]\n}\n", file);
        fclose(file);
        return true;
    }

    bool IsSupportedReflowWidth(LONG width)
    {
        return width == 254 || width == 260 || width == 328 || width == 362;
    }

    bool IsLegacyReflowWidth(LONG width)
    {
        return width == 254 || width == 260 || width == 328 || width == 362;
    }

    constexpr int kMinHistoricalParagraphSourceBytes = 30;

    constexpr uintptr_t kParagraphMeasureCaller = 0x0049B324;
    constexpr uintptr_t kParagraphDrawCaller = 0x0049B9F0;

    bool TryApplyLayoutTunerMeasure(HDC dc, LPCSTR text, int count, LPRECT rect,
        UINT format, uintptr_t callerAddress);

#ifndef WSC3_MINIMAL_RELEASE
    constexpr uintptr_t kShipLookupRva = 0x000B03F0;
    constexpr LONG kMaxShipTraceLines = 256;

    struct ShipTraceSignature
    {
        uintptr_t caller = 0;
        int arg1 = 0;
        int index = 0;
        DWORD root = 0;
        DWORD slot = 0;
        DWORD value = 0;
    };

    ShipTraceSignature g_shipTraceSeen[kMaxShipTraceLines] = {};
    volatile LONG g_shipTraceSeenCount = 0;

    bool SameShipTraceSignature(const ShipTraceSignature& left,
        const ShipTraceSignature& right)
    {
        return left.caller == right.caller &&
            left.arg1 == right.arg1 &&
            left.index == right.index &&
            left.root == right.root &&
            left.slot == right.slot &&
            left.value == right.value;
    }

    bool ReserveNewShipTrace(const ShipTraceSignature& signature)
    {
        LONG count = InterlockedCompareExchange(&g_shipTraceSeenCount, 0, 0);
        if (count >= kMaxShipTraceLines)
            return false;

        for (LONG i = 0; i < count; ++i)
        {
            if (SameShipTraceSignature(g_shipTraceSeen[i], signature))
                return false;
        }

        const LONG slot = InterlockedIncrement(&g_shipTraceSeenCount) - 1;
        if (slot < 0 || slot >= kMaxShipTraceLines)
            return false;
        g_shipTraceSeen[slot] = signature;
        return true;
    }

    bool ReadDwordSafe(uintptr_t address, DWORD* value)
    {
        if (!value || address == 0)
            return false;
        __try
        {
            *value = *reinterpret_cast<const DWORD*>(address);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            *value = 0;
            return false;
        }
    }

    int __fastcall HookShipLookup(void* self, void*, int arg1, int index)
    {
        DWORD root = 0;
        DWORD previousIndex = 0;
        DWORD slot = 0;
        DWORD value = 0;
        const uintptr_t selfAddress = reinterpret_cast<uintptr_t>(self);
        ReadDwordSafe(selfAddress + 0x08, &root);
        ReadDwordSafe(selfAddress + 0x10, &previousIndex);
        if (root != 0 && index >= 0 && index <= 0x100000)
        {
            slot = static_cast<DWORD>(root +
                static_cast<uintptr_t>(index) * 0x81u + 0x200u);
            ReadDwordSafe(slot, &value);
        }
        const uintptr_t caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
        const ShipTraceSignature signature = {
            caller, arg1, index, root, slot, value
        };
        if (ReserveNewShipTrace(signature))
        {
            g_inHook = true;
            log("[SHIPDATA] caller=0x%08lX self=0x%08lX arg1=%d index=%d self10=0x%08lX root=0x%08lX slot=0x%08lX value=0x%08lX\n",
                static_cast<unsigned long>(caller),
                static_cast<unsigned long>(selfAddress), arg1, index,
                static_cast<unsigned long>(previousIndex),
                static_cast<unsigned long>(root), static_cast<unsigned long>(slot),
                static_cast<unsigned long>(value));
            g_inHook = false;
        }
        return g_shipLookupOriginal
            ? g_shipLookupOriginal(self, arg1, index) : 0;
    }

    void InstallShipLookupHook()
    {
        if (!g_captureShipData ||
            InterlockedCompareExchange(&g_shipLookupHookInstalled, 0, 0) != 0)
            return;

        if (InterlockedCompareExchange(&g_shipLookupHookInstalled, 1, 0) != 0)
            return;

        char modulePath[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, modulePath, ARRAYSIZE(modulePath));
        const char* fileName = strrchr(modulePath, '\\');
        fileName = fileName ? fileName + 1 : modulePath;
        if (_stricmp(fileName, "wsc3.exe") != 0)
        {
            InterlockedExchange(&g_shipLookupHookInstalled, 3);
            return;
        }

        BYTE* target = reinterpret_cast<BYTE*>(
            reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)) + kShipLookupRva);
        const BYTE expected[] = {
            0x8B, 0x44, 0x24, 0x08, 0x8B, 0x51, 0x10, 0x3B, 0xC2, 0x74, 0x4A
        };
        if (!target || memcmp(target, expected, sizeof(expected)) != 0)
        {
            log("[SHIPDATA] hook skipped: unexpected bytes at RVA 0x%08lX\n",
                static_cast<unsigned long>(kShipLookupRva));
            InterlockedExchange(&g_shipLookupHookInstalled, 3);
            return;
        }

        MH_STATUS createStatus = MH_CreateHook(target,
            reinterpret_cast<LPVOID>(&HookShipLookup),
            reinterpret_cast<LPVOID*>(&g_shipLookupOriginal));
        MH_STATUS enableStatus = createStatus == MH_OK
            ? MH_EnableHook(target) : createStatus;
        if (enableStatus != MH_OK)
        {
            log("[SHIPDATA] hook failed: create=%d enable=%d\n",
                static_cast<int>(createStatus), static_cast<int>(enableStatus));
            InterlockedExchange(&g_shipLookupHookInstalled, 3);
            return;
        }

        InterlockedExchange(&g_shipLookupHookInstalled, 2);
        log("[SHIPDATA] hook enabled: RVA=0x%08lX\n",
            static_cast<unsigned long>(kShipLookupRva));
    }
#endif

    bool IsHistoricalParagraphMeasure(uintptr_t caller)
    {
        return caller == 0 || caller == kParagraphMeasureCaller;
    }

    bool IsHistoricalParagraphDraw(uintptr_t caller)
    {
        return caller == 0 || caller == kParagraphDrawCaller;
    }

    LONG MeasurePatchParagraphHeight(HDC dc, LPCWSTR text, int count,
        LONG width, UINT format)
    {
        if (!g_drawTextW || !dc || !text || count <= 0 || width <= 0)
            return 0;

        RECT measured = { 0, 0, width, 0 };
        const UINT measureFormat = format | DT_CALCRECT | DT_WORDBREAK;
        g_drawTextW(dc, text, count, &measured, measureFormat);
        LONG height = measured.bottom - measured.top;
        if (height <= 0)
        {
            TEXTMETRICW metrics = {};
            if (GetTextMetricsW(dc, &metrics) && metrics.tmHeight > 0)
                height = metrics.tmHeight;
        }
        return height;
    }

    bool FindPatchParagraphLine(LPCSTR line, int count, size_t& after)
    {
        if (!line || count < 4 || g_patchParagraphSourceBytes.empty())
            return false;
        size_t next = g_patchParagraphNextByte;
        while (next < g_patchParagraphSourceBytes.size() &&
            (g_patchParagraphSourceBytes[next] == ' ' ||
                g_patchParagraphSourceBytes[next] == '\r' ||
                g_patchParagraphSourceBytes[next] == '\n'))
            ++next;
        if (g_patchParagraphSourceBytes.compare(next,
            static_cast<size_t>(count), line, static_cast<size_t>(count)) != 0)
            return false;
        after = next + static_cast<size_t>(count);
        return true;
    }

    bool ReplayHistoricalPatchParagraph(HDC dc, LPCSTR line, int count,
        LPRECT rect, UINT format, uintptr_t caller, int& result)
    {
        if (!g_drawTextW || !line || !rect || !(format & DT_WORDBREAK) ||
            !IsSupportedReflowWidth(rect->right - rect->left) ||
            (!g_patchParagraphPending && !g_patchParagraphDrawn) ||
            !IsHistoricalParagraphDraw(caller))
            return false;
        if (GetTickCount64() > g_patchParagraphDeadline ||
            dc != g_patchParagraphDc || rect->left != g_patchParagraphLeft)
        {
            g_patchParagraphPending = false;
            g_patchParagraphDrawn = false;
            g_patchParagraphSourceBytes.clear();
            return false;
        }
        size_t after = 0;
        if (!FindPatchParagraphLine(line, count, after))
            return false;
        // A dialog can contain several cached paragraphs.  Later paragraphs
        // legitimately start at non-zero top coordinates (for example 90,
        // 180, 270), so the matching first line—not top == 0—must start the
        // historical replay.
        if (g_patchParagraphPending)
        {
            if (g_patchLayoutHasLast && rect->top <= g_patchLayoutLastOriginalFirstTop)
            {
                // A new repaint/scroll pass starts from an earlier source
                // line.  Do not carry the previous pass's vertical offset.
                g_patchLayoutVerticalOffset = 0;
                g_patchLayoutHasLast = false;
            }

            if (g_patchLayoutHasLast)
            {
                const LONG lineHeight = g_patchLayoutLastLineHeight > 0
                    ? g_patchLayoutLastLineHeight : 15;
                LONG originalTrailingGap = rect->top -
                    g_patchLayoutLastOriginalLastTop - lineHeight;
                if (originalTrailingGap < 0)
                    originalTrailingGap = 0;
                const LONG originalParagraphSpan = rect->top -
                    g_patchLayoutLastOriginalFirstTop;
                const LONG desiredParagraphSpan =
                    g_patchLayoutLastRenderedHeight + originalTrailingGap;
                g_patchLayoutVerticalOffset += desiredParagraphSpan -
                    originalParagraphSpan;
                WSC3_PATCH_TRACE("[PATCHTRACE] layout previousHeight=%ld originalSpan=%ld "
                        "trailingGap=%ld delta=%ld offset=%ld\n",
                        g_patchLayoutLastRenderedHeight, originalParagraphSpan,
                        originalTrailingGap,
                        desiredParagraphSpan - originalParagraphSpan,
                        g_patchLayoutVerticalOffset);
            }

            RECT expanded = *rect;
            g_patchParagraphNextByte = after;
            const LONG renderedHeight = MeasurePatchParagraphHeight(dc,
                g_patchParagraph, g_patchParagraphCount,
                rect->right - rect->left, format);
            expanded.top = rect->top + g_patchLayoutVerticalOffset;
            expanded.bottom = expanded.top + 512;
            g_patchParagraphPending = false;
            g_patchParagraphDrawn = true;
            g_patchLayoutLastOriginalFirstTop = rect->top;
            g_patchLayoutLastOriginalLastTop = rect->top;
            g_patchLayoutLastRenderedHeight = renderedHeight > 0
                ? renderedHeight : g_patchLayoutLastLineHeight;
            g_patchLayoutHasLast = true;
            WSC3_PATCH_TRACE("[PATCHTRACE] replay top=%ld drawTop=%ld width=%ld "
                    "height=%ld count=%d\n", rect->top, expanded.top,
                    rect->right - rect->left, g_patchLayoutLastRenderedHeight,
                    g_patchParagraphCount);
            result = g_drawTextW(dc, g_patchParagraph,
                g_patchParagraphCount, &expanded, format);
            return true;
        }
        if (g_patchParagraphDrawn)
        {
            if (rect->top > g_patchLayoutLastOriginalLastTop)
            {
                const LONG step = rect->top - g_patchLayoutLastOriginalLastTop;
                if (step > 0 && step < 128)
                    g_patchLayoutLastLineHeight = step;
            }
            g_patchLayoutLastOriginalLastTop = rect->top;
            g_patchParagraphNextByte = after;
            result = 0;
            return true;
        }
        return false;
    }

    void UpdateDebugControls(HWND hwnd)
    {
        if (!hwnd || !g_debugLockInitialized)
            return;
        DebugSnapshot snapshot;
        std::vector<DebugSnapshot> history;
        int firstVisibleSlot = -1;
        int selectedDisplayIndex = -1;
        int historyCapacity = 200;
        EnterCriticalSection(&g_debugLock);
        snapshot = g_debugSnapshot;
        historyCapacity = g_debugHistoryCapacity;
        for (int i = 0; i < g_debugHistoryCount; ++i)
        {
            const int slot = HistorySlotAtDisplayIndexLocked(i);
            if (slot < 0)
                continue;
            const DebugSnapshot& candidate = g_debugHistory[slot];
            const bool translated = !FindTranslation(candidate.key).empty() ||
                !FindTemplateTranslation(candidate.text).empty();
            if (g_debugHideTranslated && translated)
                continue;
            if (firstVisibleSlot < 0)
                firstVisibleSlot = slot;
            if (slot == g_debugSelectedSlot)
                selectedDisplayIndex = static_cast<int>(history.size());
            history.push_back(candidate);
        }
        if (g_debugHideTranslated && selectedDisplayIndex < 0 &&
            firstVisibleSlot >= 0 && !history.empty())
        {
            g_debugSelectedSlot = firstVisibleSlot;
            g_debugSnapshot = history[0];
            snapshot = g_debugSnapshot;
            selectedDisplayIndex = 0;
        }
        LeaveCriticalSection(&g_debugLock);

        HWND historyList = GetDlgItem(hwnd, kDebugHistoryList);
        HWND text = GetDlgItem(hwnd, 1003);
        HWND translation = GetDlgItem(hwnd, kDebugTranslationEdit);
        HWND captureToggle = GetDlgItem(hwnd, kDebugTranslateCapturesCheckbox);
        HWND capacityCombo = GetDlgItem(hwnd, kDebugHistoryCapacityCombo);
        HWND info = GetDlgItem(hwnd, 1004);
        HWND sourceInfo = GetDlgItem(hwnd, kDebugSourceInfo);
        HWND sourceTracking = GetDlgItem(hwnd, kDebugSourceTrackingButton);
        if (historyList)
        {
            SendMessageW(historyList, LB_RESETCONTENT, 0, 0);
            for (size_t i = 0; i < history.size(); ++i)
            {
                wchar_t preview[96] = {};
                lstrcpynW(preview, history[i].text, ARRAYSIZE(preview));
                wchar_t item[160] = {};
                swprintf_s(item, ARRAYSIZE(item), L"%d  [%08X]  %s",
                    static_cast<int>(i) + 1, history[i].key, preview);
                SendMessageW(historyList, LB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(item));
            }
            if (selectedDisplayIndex >= 0)
                SendMessageW(historyList, LB_SETCURSEL,
                    selectedDisplayIndex, 0);
        }
        if (text)
            SetWindowTextW(text, snapshot.text);
        if (translation)
        {
            std::wstring value = FindTranslation(snapshot.key);
            if (value.empty())
                value = FindTemplateTranslation(snapshot.text);
            SetWindowTextW(translation, value.c_str());
        }
        if (captureToggle)
        {
            SendMessageW(captureToggle, BM_SETCHECK,
                HasCaptureTranslationTemplate(snapshot.text)
                    ? BST_CHECKED : BST_UNCHECKED, 0);
        }
        if (capacityCombo)
        {
            const int selection = historyCapacity / 100 - 1;
            SendMessageW(capacityCombo, CB_SETCURSEL, selection, 0);
        }
        if (info)
        {
            wchar_t line[512] = {};
            swprintf_s(line, ARRAYSIZE(line),
                L"count=%d  format=0x%08X  rect=%ld,%ld - %ld,%ld  state=%d  cp=%ld",
                snapshot.count, snapshot.format, snapshot.left, snapshot.top,
                snapshot.right, snapshot.bottom, snapshot.state, 932L);
            SetWindowTextW(info, line);
        }
        if (sourceInfo)
            SetWindowTextW(sourceInfo, snapshot.source);
        if (sourceTracking)
        {
            SendMessageW(sourceTracking, BM_SETCHECK,
                IsSourceTrackingEnabledInternal() ? BST_CHECKED : BST_UNCHECKED, 0);
        }
    }

    LRESULT CALLBACK DebugWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
        {
            CreateWindowW(L"STATIC", L"\u6700\u8FD1\u6587\u5B57\u8BB0\u5F55", WS_CHILD | WS_VISIBLE,
                10, 10, 260, 20, hwnd, nullptr, nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"\u9690\u85CF\u5DF2\u7FFB\u8BD1", WS_CHILD | WS_VISIBLE |
                BS_AUTOCHECKBOX,
                300, 8, 180, 24, hwnd,
                reinterpret_cast<HMENU>(kDebugHideTranslatedButton), nullptr, nullptr);
            CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                LBS_NOTIFY | WS_VSCROLL,
                10, 35, 560, 100, hwnd,
                reinterpret_cast<HMENU>(kDebugHistoryList), nullptr, nullptr);
            CreateWindowW(L"STATIC", L"\u9009\u4E2D\u6587\u5B57", WS_CHILD | WS_VISIBLE,
                10, 140, 200, 20, hwnd, nullptr, nullptr, nullptr);
            CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                10, 165, 560, 85, hwnd, reinterpret_cast<HMENU>(1003), nullptr, nullptr);
            CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
                10, 255, 560, 25, hwnd, reinterpret_cast<HMENU>(1004), nullptr, nullptr);
            CreateWindowW(L"EDIT", L"Source: source unknown", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | ES_READONLY | WS_VSCROLL,
                10, 280, 560, 80, hwnd,
                reinterpret_cast<HMENU>(kDebugSourceInfo), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"\u6765\u6E90\u8FFD\u8E2A", WS_CHILD | WS_VISIBLE |
                BS_AUTOCHECKBOX,
                10, 365, 120, 24, hwnd,
                reinterpret_cast<HMENU>(kDebugSourceTrackingButton), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"\u6E05\u7A7A\u6765\u6E90\u7F13\u5B58", WS_CHILD | WS_VISIBLE |
                BS_PUSHBUTTON,
                135, 365, 130, 24, hwnd,
                reinterpret_cast<HMENU>(kDebugSourceClearButton), nullptr, nullptr);
            CreateWindowW(L"STATIC", L"\u624B\u52A8\u539F\u6587:", WS_CHILD | WS_VISIBLE,
                10, 400, 70, 22, hwnd, nullptr, nullptr, nullptr);
            CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | WS_VSCROLL,
                85, 395, 485, 70, hwnd,
                reinterpret_cast<HMENU>(kDebugManualSourceEdit), nullptr, nullptr);
            CreateWindowW(L"STATIC", L"\u8BD1\u6587:", WS_CHILD | WS_VISIBLE,
                10, 475, 70, 22, hwnd, nullptr, nullptr, nullptr);
            CreateWindowW(L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_BORDER |
                ES_MULTILINE | WS_VSCROLL,
                85, 470, 485, 70, hwnd,
                reinterpret_cast<HMENU>(kDebugTranslationEdit), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"\u901A\u914D\u7B26\u4E8C\u6B21\u6BD4\u5BF9", WS_CHILD | WS_VISIBLE |
                BS_AUTOCHECKBOX,
                85, 545, 160, 24, hwnd,
                reinterpret_cast<HMENU>(kDebugTranslateCapturesCheckbox), nullptr, nullptr);
            CreateWindowW(L"STATIC", L"\u7F13\u5B58\u6761\u6570:", WS_CHILD | WS_VISIBLE,
                255, 549, 65, 20, hwnd, nullptr, nullptr, nullptr);
            HWND capacityCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE |
                WS_BORDER | CBS_DROPDOWNLIST,
                325, 545, 80, 140, hwnd,
                reinterpret_cast<HMENU>(kDebugHistoryCapacityCombo), nullptr, nullptr);
            if (capacityCombo)
            {
                SendMessageW(capacityCombo, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(L"100"));
                SendMessageW(capacityCombo, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(L"200"));
                SendMessageW(capacityCombo, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(L"300"));
                SendMessageW(capacityCombo, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(L"400"));
                SendMessageW(capacityCombo, CB_ADDSTRING, 0,
                    reinterpret_cast<LPARAM>(L"500"));
                SendMessageW(capacityCombo, CB_SETCURSEL, 1, 0);
            }
            CreateWindowW(L"BUTTON", L"\u4FDD\u5B58\u7FFB\u8BD1", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                85, 580, 120, 24, hwnd,
                reinterpret_cast<HMENU>(kDebugApplyTranslationButton), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"\u590D\u5236\u5F53\u524D\u539F\u6587", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                210, 580, 120, 24, hwnd,
                reinterpret_cast<HMENU>(kDebugCopyCurrentButton), nullptr, nullptr);
            CreateWindowW(L"BUTTON", L"\u4FDD\u5B58\u624B\u52A8\u8BB0\u5F55", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                335, 580, 125, 24, hwnd,
                reinterpret_cast<HMENU>(kDebugManualSaveButton), nullptr, nullptr);
            return 0;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == kDebugHistoryList &&
                HIWORD(wParam) == LBN_SELCHANGE)
            {
                HWND historyList = GetDlgItem(hwnd, kDebugHistoryList);
                LRESULT selected = SendMessageW(historyList, LB_GETCURSEL, 0, 0);
                if (selected != LB_ERR && g_debugLockInitialized)
                {
                    EnterCriticalSection(&g_debugLock);
                    const int slot = HistorySlotAtDisplayIndexLocked(
                        static_cast<int>(selected));
                    if (slot >= 0)
                    {
                        g_debugSelectedSlot = slot;
                        g_debugFollowLatest = false;
                        g_debugSnapshot = g_debugHistory[slot];
                    }
                    LeaveCriticalSection(&g_debugLock);
                    UpdateDebugControls(hwnd);
                }
                return 0;
            }
            if (LOWORD(wParam) == kDebugHideTranslatedButton &&
                HIWORD(wParam) == BN_CLICKED)
            {
                HWND toggle = GetDlgItem(hwnd, kDebugHideTranslatedButton);
                g_debugHideTranslated = SendMessageW(toggle, BM_GETCHECK, 0, 0) ==
                    BST_CHECKED;
                UpdateDebugControls(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == kDebugSourceTrackingButton &&
                HIWORD(wParam) == BN_CLICKED)
            {
                HWND toggle = GetDlgItem(hwnd, kDebugSourceTrackingButton);
                SetSourceTrackingEnabled(toggle &&
                    SendMessageW(toggle, BM_GETCHECK, 0, 0) == BST_CHECKED);
                UpdateDebugControls(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == kDebugSourceClearButton &&
                HIWORD(wParam) == BN_CLICKED)
            {
                ClearSourceTrackingCache();
                UpdateDebugControls(hwnd);
                return 0;
            }
            if (LOWORD(wParam) == kDebugHistoryCapacityCombo &&
                HIWORD(wParam) == CBN_SELCHANGE)
            {
                HWND capacityCombo = GetDlgItem(hwnd, kDebugHistoryCapacityCombo);
                const LRESULT selected = SendMessageW(capacityCombo,
                    CB_GETCURSEL, 0, 0);
                if (selected >= 0 && selected <= 4 && g_debugLockInitialized)
                {
                    EnterCriticalSection(&g_debugLock);
                    SetDebugHistoryCapacityLocked(
                        100 + static_cast<int>(selected) * 100);
                    LeaveCriticalSection(&g_debugLock);
                    UpdateDebugControls(hwnd);
                }
                return 0;
            }
            if (LOWORD(wParam) == kDebugCopyCurrentButton)
            {
                wchar_t current[2048] = {};
                if (g_debugLockInitialized)
                {
                    EnterCriticalSection(&g_debugLock);
                    memcpy(current, g_debugSnapshot.text, sizeof(current));
                    LeaveCriticalSection(&g_debugLock);
                }
                SetWindowTextW(GetDlgItem(hwnd, kDebugManualSourceEdit), current);
                return 0;
            }
            if (LOWORD(wParam) == kDebugManualSaveButton)
            {
                wchar_t source[4096] = {};
                wchar_t translation[4096] = {};
                GetDlgItemTextW(hwnd, kDebugManualSourceEdit, source, ARRAYSIZE(source));
                GetDlgItemTextW(hwnd, kDebugTranslationEdit, translation, ARRAYSIZE(translation));
                const UINT codePage = 932;
                HWND captureToggle = GetDlgItem(hwnd,
                    kDebugTranslateCapturesCheckbox);
                const bool translateCaptures = captureToggle &&
                    SendMessageW(captureToggle, BM_GETCHECK, 0, 0) == BST_CHECKED;
                std::string raw;
                if (source[0])
                {
                    const std::wstring sourceText(source);
                    uint32_t key = 0;
                    if (HasTemplatePlaceholder(sourceText))
                    {
                        // Template records use the visible placeholder form,
                        // e.g. "{0}\u3092\u30ed\u30fc\u30c9\u3057\u307e\u3059\u304b\uFF1F".
                        key = HashWideText(sourceText.c_str(),
                            static_cast<int>(sourceText.size()));
                    }
                    else if (WideToCodePageBytes(codePage, sourceText, raw))
                    {
                        key = HashRawText(raw.data(), static_cast<int>(raw.size()));
                    }
                    if (g_debugLockInitialized)
                    {
                        EnterCriticalSection(&g_debugLock);
                        if (key != 0)
                        {
                            UpsertTranslation(key, translation, sourceText,
                                translateCaptures);
                            SaveTranslations();
                        }
                        LeaveCriticalSection(&g_debugLock);
                    }
                }
                return 0;
            }
            if (LOWORD(wParam) == kDebugApplyTranslationButton)
            {
                wchar_t value[4096] = {};
                GetDlgItemTextW(hwnd, kDebugTranslationEdit, value, ARRAYSIZE(value));
                if (g_debugLockInitialized)
                {
                    EnterCriticalSection(&g_debugLock);
                    if (g_debugSnapshot.key != 0)
                    {
                        UpsertTranslation(g_debugSnapshot.key, value,
                            g_debugSnapshot.text);
                        SaveTranslations();
                    }
                    LeaveCriticalSection(&g_debugLock);
                }
                UpdateDebugControls(hwnd);
                return 0;
            }
            return 0;
        case kDebugUpdate:
            UpdateDebugControls(hwnd);
            return 0;
        case WM_DESTROY:
            InterlockedExchangePointer(reinterpret_cast<PVOID*>(&g_debugWindow), nullptr);
            if (g_debugWindowInstanceMutex)
            {
                CloseHandle(g_debugWindowInstanceMutex);
                g_debugWindowInstanceMutex = nullptr;
            }
            InterlockedExchange(&g_debugWindowStartState, 0);
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    DWORD WINAPI DebugWindowThread(LPVOID)
    {
        g_debugThreadId = GetCurrentThreadId();
        HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSW wc = {};
        wc.lpfnWndProc = DebugWindowProc;
        wc.hInstance = instance;
        wc.lpszClassName = L"Wsc3UiDebugWindow";
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        RegisterClassW(&wc);
        HWND window = CreateWindowExW(WS_EX_TOPMOST, wc.lpszClassName,
            L"WSC3 UI Debug", WS_OVERLAPPEDWINDOW,
            80, 80, 600, 680, nullptr, nullptr, instance, nullptr);
        InterlockedExchangePointer(reinterpret_cast<PVOID*>(&g_debugWindow), window);
        if (window)
        {
            ShowWindow(window, SW_SHOW);
            UpdateWindow(window);
        }
        else
        {
            if (g_debugWindowInstanceMutex)
            {
                CloseHandle(g_debugWindowInstanceMutex);
                g_debugWindowInstanceMutex = nullptr;
            }
            InterlockedExchange(&g_debugWindowStartState, 0);
            return 0;
        }
        MSG message;
        while (GetMessageW(&message, nullptr, 0, 0) > 0)
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (g_debugWindowInstanceMutex)
        {
            CloseHandle(g_debugWindowInstanceMutex);
            g_debugWindowInstanceMutex = nullptr;
        }
        InterlockedExchange(&g_debugWindowStartState, 0);
        return 0;
    }

    void StartDebugWindow()
    {
        if (InterlockedCompareExchange(&g_debugWindowStartState, 1, 0) != 0)
        {
            HWND existing = reinterpret_cast<HWND>(InterlockedCompareExchangePointer(
                reinterpret_cast<PVOID*>(&g_debugWindow), nullptr, nullptr));
            if (existing)
            {
                ShowWindow(existing, SW_RESTORE);
                SetForegroundWindow(existing);
            }
            return;
        }
        if (!g_debugLockInitialized)
        {
            InitializeCriticalSection(&g_debugLock);
            g_debugLockInitialized = true;
        }

        wchar_t mutexName[96] = {};
        swprintf_s(mutexName, ARRAYSIZE(mutexName),
            L"Local\\WSC3UiDebugWindow_%lu", GetCurrentProcessId());
        HANDLE instanceMutex = CreateMutexW(nullptr, FALSE, mutexName);
        if (!instanceMutex)
        {
            InterlockedExchange(&g_debugWindowStartState, 0);
            return;
        }
        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            CloseHandle(instanceMutex);
            InterlockedExchange(&g_debugWindowStartState, 0);
            HWND existing = FindWindowW(L"Wsc3UiDebugWindow", nullptr);
            if (existing)
            {
                ShowWindow(existing, SW_RESTORE);
                SetForegroundWindow(existing);
            }
            return;
        }
        g_debugWindowInstanceMutex = instanceMutex;
        HANDLE thread = CreateThread(nullptr, 0, DebugWindowThread, nullptr, 0, nullptr);
        if (!thread)
        {
            CloseHandle(g_debugWindowInstanceMutex);
            g_debugWindowInstanceMutex = nullptr;
            InterlockedExchange(&g_debugWindowStartState, 0);
            return;
        }
        CloseHandle(thread);
    }

    void PublishDebugSnapshot(LPCWSTR text, int count, LPRECT rect, UINT format,
        int state, uint32_t key = 0, LPCCH sourceBytes = nullptr,
        int sourceByteCount = 0)
    {
        if (!g_debugWindowEnabled || !g_debugLockInitialized)
            return;

        DebugSnapshot incoming;
        int copy = count < ARRAYSIZE(incoming.text) - 1 ? count : ARRAYSIZE(incoming.text) - 1;
        if (copy < 0)
            copy = text ? lstrlenW(text) : 0;
        if (copy >= ARRAYSIZE(incoming.text))
            copy = ARRAYSIZE(incoming.text) - 1;
        if (text && copy > 0)
            memcpy(incoming.text, text, copy * sizeof(wchar_t));
        incoming.text[copy] = L'\0';
        std::wstring source = ResolveSourceProvenance(sourceBytes,
            sourceByteCount);
        if (source.find(L"source unknown") != std::wstring::npos)
        {
            const SourceTrackingStats stats = GetSourceTrackingStats();
            wchar_t status[256] = {};
            swprintf_s(status, ARRAYSIZE(status),
                L"\r\ntracking: enabled=%d handles=%lu mappings=%lu views=%lu snapshots=%lu bytes=%llu",
                stats.enabled ? 1 : 0, stats.handles, stats.mappings, stats.views, stats.snapshots,
                stats.cachedBytes);
            source += status;
        }
        const size_t sourceCopy = (std::min)(source.size(),
            ARRAYSIZE(incoming.source) - 1);
        if (sourceCopy > 0)
            memcpy(incoming.source, source.data(), sourceCopy * sizeof(wchar_t));
        incoming.source[sourceCopy] = L'\0';
        incoming.count = count;
        incoming.format = format;
        incoming.left = rect ? rect->left : 0;
        incoming.top = rect ? rect->top : 0;
        incoming.right = rect ? rect->right : 0;
        incoming.bottom = rect ? rect->bottom : 0;
        incoming.state = state;
        incoming.key = key != 0 ? key : HashWideText(text, count);

        bool historyChanged = false;
        EnterCriticalSection(&g_debugLock);
        int duplicateSlot = -1;
        for (int i = 0; i < g_debugHistoryCount; ++i)
        {
            const int slot = HistorySlotAtDisplayIndexLocked(i);
            if (slot >= 0 && wcscmp(g_debugHistory[slot].text, incoming.text) == 0)
            {
                duplicateSlot = slot;
                break;
            }
        }
        if (duplicateSlot >= 0)
        {
            // The same visible text is kept only once in the history.  Update
            // its metadata, but do not post another UI refresh for the repeat.
            g_debugHistory[duplicateSlot] = incoming;
            if (g_debugSelectedSlot == duplicateSlot || g_debugFollowLatest)
                g_debugSnapshot = incoming;
        }
        else
        {
            const int slot = g_debugHistoryNext;
            g_debugHistory[slot] = incoming;
            g_debugHistoryNext = (g_debugHistoryNext + 1) % g_debugHistoryCapacity;
            if (g_debugHistoryCount < g_debugHistoryCapacity)
                ++g_debugHistoryCount;
            historyChanged = true;

            if (g_debugFollowLatest || g_debugSelectedSlot < 0 ||
                g_debugSelectedSlot == slot)
            {
                g_debugSelectedSlot = slot;
                g_debugFollowLatest = true;
                g_debugSnapshot = incoming;
            }
        }
        LeaveCriticalSection(&g_debugLock);
        if (historyChanged && g_debugWindow)
            PostMessageW(g_debugWindow, kDebugUpdate, 0, 0);
    }

    bool HasHighByte(LPCCH text, int count)
    {
        if (!text)
            return false;
        if (count < 0)
            count = static_cast<int>(strlen(text));
        for (int i = 0; i < count; ++i)
            if (static_cast<unsigned char>(text[i]) >= 0x80)
                return true;
        return false;
    }

    int WINAPI HookMultiByteToWideChar(UINT codePage, DWORD flags, LPCCH text, int count,
        LPWSTR output, int outputCount)
    {
        if (!g_multiByteToWideChar)
            return 0;
        const bool tunerCp932 = g_layoutTunerEnabled &&
            LayoutTunerGetSnapshot().forceCp932;
        if ((g_forceSystemCp932 || tunerCp932) && codePage == CP_ACP)
            codePage = 932;
        if (g_patchOnly && g_patchTextCodePage == 936 &&
            (codePage == CP_ACP || codePage == 932) &&
            HasHighByte(text, count))
        {
            if (codePage == 932 && text && output && outputCount > 1)
            {
                wchar_t original[4096] = {};
                int originalCount = g_multiByteToWideChar(932, flags, text, count,
                    original, ARRAYSIZE(original));
                int convertedCount = g_multiByteToWideChar(936, flags, text, count,
                    output, outputCount);
                int originalLength = originalCount;
                int convertedLength = convertedCount;
                if (count < 0)
                {
                    originalLength = lstrlenW(original);
                    convertedLength = lstrlenW(output);
                }
                if (originalLength > 0 && convertedLength > 0)
                    UpsertDecodeAlias(HashWideText(output, convertedLength),
                        std::wstring(original, originalLength));
                return convertedCount;
            }
            codePage = 936;
        }
        return g_multiByteToWideChar(codePage, flags, text, count, output, outputCount);
    }

    int ConvertTextCodePage(UINT codePage, LPCSTR text, int count,
        LPWSTR output, int outputCount)
    {
        if (!text || !output || outputCount <= 1)
            return 0;
        return MultiByteToWideChar(codePage, 0, text, count, output, outputCount);
    }

    int ConvertPatchText(LPCSTR text, int count, LPWSTR output, int outputCount)
    {
        return ConvertTextCodePage(g_patchTextCodePage, text, count, output, outputCount);
    }

    void CachePatchParagraph(LPCSTR sourceBytes, int sourceCount,
        LPCWSTR original, int originalCount, LPCWSTR replacement,
        int replacementCount, HDC dc, LPRECT rect, UINT format,
        LONG requestedWidth)
    {
        if (!(format & DT_CALCRECT) || !rect || !original || !replacement ||
            originalCount <= 0 || replacementCount <= 0)
            return;

        const LONG measuredWidth = rect->right - rect->left;
        const bool zeroWidthParagraph = requestedWidth <= 0 &&
            sourceCount >= kMinHistoricalParagraphSourceBytes;
        if (measuredWidth < 300 && !zeroWidthParagraph)
            return;

        const int paragraphCopy = (std::min)(replacementCount,
            static_cast<int>(ARRAYSIZE(g_patchParagraph)) - 1);
        const int originalCopy = (std::min)(originalCount,
            static_cast<int>(ARRAYSIZE(g_patchOriginalParagraph)) - 1);
        memcpy(g_patchParagraph, replacement, paragraphCopy * sizeof(wchar_t));
        g_patchParagraph[paragraphCopy] = L'\0';
        g_patchParagraphCount = paragraphCopy;
        memcpy(g_patchOriginalParagraph, original, originalCopy * sizeof(wchar_t));
        g_patchOriginalParagraph[originalCopy] = L'\0';
        g_patchOriginalParagraphCount = originalCopy;
        g_patchParagraphSourceBytes.clear();
        if (sourceBytes && sourceCount > 0)
            g_patchParagraphSourceBytes.assign(sourceBytes,
                static_cast<size_t>(sourceCount));
        g_patchParagraphDc = dc;
        g_patchParagraphLeft = rect->left;
        g_patchParagraphNextByte = 0;
        g_patchParagraphDeadline = GetTickCount64() + 500;
        g_patchParagraphPending = true;
        g_patchParagraphDrawn = false;
    }

    int WINAPI HookDrawTextPatch(HDC dc, LPCSTR text, int count, LPRECT rect,
        UINT format)
    {
#ifndef WSC3_MINIMAL_RELEASE
        InstallShipLookupHook();
#endif
        if (!rect || ((rect->right - rect->left != 972) &&
            (!(format & DT_CALCRECT) || count < 30 || count % 60 == 0)))
            WSC3_PATCH_TRACE("[PATCHTRACE][A] count=%d format=0x%08X width=%ld top=%ld\n", count, format,
                rect ? rect->right - rect->left : -1L, rect ? rect->top : -1L);
        const uintptr_t callerAddress = reinterpret_cast<uintptr_t>(_ReturnAddress());
        const int actualCount = count < 0
            ? (text ? static_cast<int>(strlen(text)) : 0) : count;
        wchar_t converted[8192] = {};
        int convertedCount = ConvertPatchText(text, count, converted, ARRAYSIZE(converted));
        if (convertedCount <= 0 || !g_drawTextW)
            return g_drawTextA ? g_drawTextA(dc, text, count, rect, format) : 0;
        const uint32_t convertedAliasKey = HashWideText(converted, convertedCount);
        convertedCount = lstrlenW(converted);
        wchar_t originalText[8192] = {};
        UINT debugCodePage = static_cast<UINT>(InterlockedCompareExchange(
            &g_debugSourceCodePage, 0, 0));
        int originalCount = ConvertTextCodePage(debugCodePage, text, count,
            originalText, ARRAYSIZE(originalText));
        if (originalCount <= 0)
        {
            memcpy(originalText, converted, convertedCount * sizeof(wchar_t));
            originalText[convertedCount] = L'\0';
            originalCount = convertedCount;
        }
        originalCount = lstrlenW(originalText);
        if (debugCodePage == 932)
        {
            const std::wstring* alias = FindDecodeAlias(convertedAliasKey);
            if (alias && !alias->empty())
            {
                int aliasCount = static_cast<int>(alias->size());
                if (aliasCount >= ARRAYSIZE(originalText))
                    aliasCount = ARRAYSIZE(originalText) - 1;
                memcpy(originalText, alias->data(), aliasCount * sizeof(wchar_t));
                originalText[aliasCount] = L'\0';
                originalCount = aliasCount;
            }
        }
        // Keep the translation identity independent of the selected display
        // code page.  The raw game bytes identify the source text reliably.
        const uint32_t originalKey = HashRawText(text, count);
        std::wstring replacement = FindTranslation(originalKey);
        if (replacement.empty())
            replacement = FindTemplateTranslation(originalText);
        if (!replacement.empty())
        {
            int copy = static_cast<int>(replacement.size());
            if (copy >= ARRAYSIZE(converted))
                copy = ARRAYSIZE(converted) - 1;
            memcpy(converted, replacement.data(), copy * sizeof(wchar_t));
            converted[copy] = L'\0';
            convertedCount = copy;
        }
        // The full Debug build can keep the normal translation path while
        // applying the live layout profile to the game's original A-side
        // paragraph measurement call.
        TryApplyLayoutTunerMeasure(dc, text, count, rect, format, callerAddress);

#ifndef WSC3_MINIMAL_RELEASE
        CaptureShipTextA(text, actualCount, rect, format, callerAddress,
            originalText, originalCount, converted, convertedCount, originalKey);
#endif

        int replayResult = 0;
        if (ReplayHistoricalPatchParagraph(dc, text, actualCount, rect, format,
            callerAddress, replayResult))
            return replayResult;

        if ((format & DT_WORDBREAK) && rect &&
            IsLegacyReflowWidth(rect->right - rect->left) &&
            (g_patchParagraphPending || g_patchParagraphDrawn))
        {
            if (rect->top == 0)
            {
                if (g_patchParagraphPending)
                {
                    WSC3_PATCH_TRACE("[PATCHTRACE][A] reflow cached=%d\n", g_patchParagraphCount);
                    RECT expanded = *rect;
                    expanded.bottom = expanded.top + 512;
                    g_patchParagraphPending = false;
                    g_patchParagraphDrawn = true;
                    WSC3_PATCH_TRACE("[PATCHTRACE][A] reflow_rect l=%ld t=%ld r=%ld b=%ld text=\"%s\"\n",
                            expanded.left, expanded.top, expanded.right, expanded.bottom,
                            CopyWideText(g_patchParagraph, g_patchParagraphCount).c_str());
                    WSC3_PUBLISH_DEBUG_SNAPSHOT(g_patchOriginalParagraph, g_patchOriginalParagraphCount,
                        &expanded, format,
                        3, g_patchParagraphKey, g_patchParagraphSourceBytes.data(),
                        static_cast<int>(g_patchParagraphSourceBytes.size()));
                    return g_drawTextW(dc, g_patchParagraph, g_patchParagraphCount, &expanded, format);
                }
                g_patchParagraphPending = false;
                g_patchParagraphDrawn = false;
            }
            else if (g_patchParagraphDrawn)
                return 0;
        }

        // The game passes several target paragraphs in CP936 byte counts
        // below 90 (for example 70-84 bytes).  Use the measured intrinsic
        // width as the second guard so short labels are not treated as
        // paragraphs, while those shorter target paragraphs can still be
        // reflowed later when they are drawn into the 254px text box.
        if ((format & DT_CALCRECT) && rect)
        {
            const LONG requestedWidth = rect->right - rect->left;
            int result = g_drawTextW(dc, converted, convertedCount, rect, format);
            const LONG measuredWidth = rect->right - rect->left;
            // Some WSC3 text boxes call the full paragraph with a zero-width
            // rectangle first, then issue separate 254px line draws.  In that
            // path the intrinsic width is not a reliable cache discriminator,
            // but the full text/count is still the paragraph we need.
            // count is the original multibyte byte count.  After CP936
            // conversion, a paragraph such as the BGM entry may contain
            // fewer than 30 wchar_t characters even though the original
            // full-text call has 45 bytes.  Use the source count here so
            // these short Chinese paragraphs are not mistaken for labels.
            const bool zeroWidthParagraph = requestedWidth <= 0 &&
                actualCount >= kMinHistoricalParagraphSourceBytes;
            if (measuredWidth < 300 && !zeroWidthParagraph)
                return result;
            CachePatchParagraph(text, actualCount, originalText, originalCount,
                converted, convertedCount, dc, rect, format, requestedWidth);
            g_patchParagraphKey = originalKey;
            WSC3_PUBLISH_DEBUG_SNAPSHOT(g_patchOriginalParagraph, g_patchOriginalParagraphCount,
                rect, format,
                1, originalKey, g_patchParagraphSourceBytes.data(),
                static_cast<int>(g_patchParagraphSourceBytes.size()));
            if (actualCount < 30 || actualCount % 60 == 0)
                WSC3_PATCH_TRACE("[PATCHTRACE][A] cache=%d requested=%ld measured=%ld text=\"%s\"\n",
                    g_patchParagraphCount,
                    requestedWidth, rect->right - rect->left,
                    CopyWideText(g_patchParagraph, g_patchParagraphCount).c_str());
            return result;
        }

        WSC3_PUBLISH_DEBUG_SNAPSHOT(originalText, originalCount, rect, format,
            (format & DT_WORDBREAK) ? 2 : 0, originalKey, text, actualCount);
        return g_drawTextW(dc, converted, convertedCount, rect, format);
    }

    int WINAPI HookDrawTextWPatch(HDC dc, LPCWSTR text, int count, LPRECT rect, UINT format)
    {
#ifndef WSC3_MINIMAL_RELEASE
        InstallShipLookupHook();
#endif
        WSC3_PATCH_TRACE("[PATCHTRACE][W] count=%d format=0x%08X width=%ld top=%ld\n", count, format,
                rect ? rect->right - rect->left : -1L, rect ? rect->top : -1L);
        if (!g_drawTextW)
            return 0;

        const uintptr_t callerAddress = reinterpret_cast<uintptr_t>(_ReturnAddress());
        const int actualCount = count < 0
            ? (text ? lstrlenW(text) : 0) : count;
        std::string sourceBytes;
        std::wstring originalText;
        uint32_t originalKey = 0;
        if (text && actualCount > 0)
        {
            originalText.assign(text, static_cast<size_t>(actualCount));
            WideToCodePageBytes(932, originalText, sourceBytes);
        }

        std::wstring replacement;
        if (!sourceBytes.empty())
        {
            originalKey = HashRawText(sourceBytes.data(),
                static_cast<int>(sourceBytes.size()));
            replacement = FindTranslation(originalKey);
            if (replacement.empty())
                replacement = FindTemplateTranslation(originalText);
        }
        const LPCWSTR renderedText = replacement.empty()
            ? text : replacement.c_str();
        const int renderedCount = replacement.empty()
            ? actualCount : static_cast<int>(replacement.size());

        int replayResult = 0;
        if (ReplayHistoricalPatchParagraph(dc, sourceBytes.data(),
            static_cast<int>(sourceBytes.size()), rect, format, callerAddress,
            replayResult))
            return replayResult;

        if (IsHistoricalParagraphMeasure(callerAddress) && text && rect &&
            (format & DT_CALCRECT) && actualCount > 0)
        {
            const LONG requestedWidth = rect->right - rect->left;
            const int result = g_drawTextW(dc, renderedText, renderedCount, rect, format);
            const LONG measuredWidth = rect->right - rect->left;
            if (measuredWidth >= 300 || requestedWidth <= 0)
                CachePatchParagraph(sourceBytes.data(),
                    static_cast<int>(sourceBytes.size()), originalText.data(),
                    actualCount, renderedText, renderedCount, dc, rect, format,
                    requestedWidth);
            WSC3_PUBLISH_DEBUG_SNAPSHOT(originalText.c_str(), actualCount, rect, format,
                1, originalKey, sourceBytes.data(),
                static_cast<int>(sourceBytes.size()));
            return result;
        }

        if ((format & DT_WORDBREAK) && rect &&
            IsLegacyReflowWidth(rect->right - rect->left) &&
            (g_patchParagraphPending || g_patchParagraphDrawn))
        {
            if (rect->top == 0)
            {
                if (g_patchParagraphPending)
                {
                    WSC3_PATCH_TRACE("[PATCHTRACE][W] reflow cached=%d\n", g_patchParagraphCount);
                    RECT expanded = *rect;
                    expanded.bottom = expanded.top + 512;
                    g_patchParagraphPending = false;
                    g_patchParagraphDrawn = true;
                    WSC3_PATCH_TRACE("[PATCHTRACE][W] reflow_rect l=%ld t=%ld r=%ld b=%ld text=\"%s\"\n",
                            expanded.left, expanded.top, expanded.right, expanded.bottom,
                            CopyWideText(g_patchParagraph, g_patchParagraphCount).c_str());
                    WSC3_PUBLISH_DEBUG_SNAPSHOT(g_patchOriginalParagraph,
                        g_patchOriginalParagraphCount, &expanded, format, 3,
                        g_patchParagraphKey, g_patchParagraphSourceBytes.data(),
                        static_cast<int>(g_patchParagraphSourceBytes.size()));
                    return g_drawTextW(dc, g_patchParagraph, g_patchParagraphCount, &expanded, format);
                }
                g_patchParagraphPending = false;
                g_patchParagraphDrawn = false;
            }
            else if (g_patchParagraphDrawn)
                return 0;
        }
        WSC3_PUBLISH_DEBUG_SNAPSHOT(originalText.c_str(), actualCount, rect, format,
            (format & DT_WORDBREAK) ? 2 : 0, originalKey, sourceBytes.data(),
            static_cast<int>(sourceBytes.size()));
        return g_drawTextW(dc, renderedText, renderedCount, rect, format);
    }

    void ScaleTargetLogfont(LOGFONTA* font)
    {
        if (!font || !g_narrowTargetFont || font->lfCharSet != SHIFTJIS_CHARSET)
            return;
        const int scale = g_targetFontScalePercent > 0 ? g_targetFontScalePercent : 100;
        font->lfHeight = (font->lfHeight * scale) / 100;
        if (font->lfHeight == 0)
            font->lfHeight = 1;
        if (font->lfWidth != 0)
            font->lfWidth = (font->lfWidth * scale) / 100;
    }

    bool GetLayoutTunerFontProfile(int height, LayoutTunerFontProfile* profile)
    {
        if (!g_layoutTunerEnabled || !profile)
            return false;
        const LayoutTunerSnapshot snapshot = LayoutTunerGetSnapshot();
        const int index = LayoutTunerFontIndexForHeight(height);
        if (index < 0 || !snapshot.fonts[index].enabled)
            return false;
        *profile = snapshot.fonts[index];
        return true;
    }

    HFONT WINAPI HookCreateFontA(int height, int width, int escapement, int orientation,
        int weight, DWORD italic, DWORD underline, DWORD strikeOut, DWORD charSet,
        DWORD outputPrecision, DWORD clipPrecision, DWORD quality, DWORD pitchAndFamily,
        LPCSTR faceName)
    {
        if (g_layoutTunerEnabled && charSet == SHIFTJIS_CHARSET && width == 0)
        {
            LayoutTunerFontProfile profile = {};
            if (GetLayoutTunerFontProfile(height, &profile) && profile.lfWidth > 0)
                width = profile.lfWidth;
        }
        else if (g_forceXpTextMetrics && charSet == SHIFTJIS_CHARSET && width == 0)
        {
            const int absoluteHeight = height < 0 ? -height : height;
            if (absoluteHeight == 12 || absoluteHeight == 13)
                width = 6;
            else if (absoluteHeight == 28)
                width = 15;
        }
        if (g_narrowTargetFont && charSet == SHIFTJIS_CHARSET)
        {
            const int scale = g_targetFontScalePercent > 0 ? g_targetFontScalePercent : 100;
            height = (height * scale) / 100;
            if (height == 0)
                height = 1;
            if (width != 0)
                width = (width * scale) / 100;
        }
        return g_createFontA(height, width, escapement, orientation, weight, italic, underline,
            strikeOut, charSet, outputPrecision, clipPrecision, quality, pitchAndFamily, faceName);
    }

    HFONT WINAPI HookCreateFontIndirectA(const LOGFONTA* input)
    {
        if (!input)
            return g_createFontIndirectA(input);
        LOGFONTA font = *input;
        if (g_layoutTunerEnabled && font.lfCharSet == SHIFTJIS_CHARSET && font.lfWidth == 0)
        {
            LayoutTunerFontProfile profile = {};
            if (GetLayoutTunerFontProfile(font.lfHeight, &profile) && profile.lfWidth > 0)
                font.lfWidth = profile.lfWidth;
        }
        else if (g_forceXpTextMetrics && font.lfCharSet == SHIFTJIS_CHARSET && font.lfWidth == 0)
        {
            const int absoluteHeight = font.lfHeight < 0 ? -font.lfHeight : font.lfHeight;
            if (absoluteHeight == 12 || absoluteHeight == 13)
                font.lfWidth = 6;
            else if (absoluteHeight == 28)
                font.lfWidth = 15;
        }
        ScaleTargetLogfont(&font);
        return g_createFontIndirectA(&font);
    }

    bool IsInterestingDeviceCap(int index)
    {
        return index == HORZRES || index == VERTRES || index == LOGPIXELSX ||
            index == LOGPIXELSY || index == BITSPIXEL || index == PLANES;
    }

    bool CopyAnsiBuffer(LPCSTR text, int count, char* buffer, int capacity)
    {
        if (!text || !buffer || capacity <= 1 || count <= 0)
            return false;
        const int limit = (count >= capacity) ? (capacity - 1) : count;
        __try
        {
            memcpy(buffer, text, limit);
            buffer[limit] = '\0';
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            buffer[0] = '\0';
            return false;
        }
        return true;
    }

    std::string CopyAnsiHex(LPCSTR text, int count)
    {
        if (!text || count <= 0)
            return std::string();
        const int limit = count > 48 ? 48 : count;
        char bytes[49] = {};
        if (!CopyAnsiBuffer(text, limit, bytes, sizeof(bytes)))
            return std::string("<invalid>");
        char hex[145] = {};
        for (int i = 0; i < limit; ++i)
            sprintf_s(hex + i * 3, sizeof(hex) - i * 3, "%02X ", static_cast<unsigned char>(bytes[i]));
        return std::string(hex);
    }

    bool CopyWideBuffer(LPCWSTR text, int count, wchar_t* buffer, int capacity)
    {
        if (!text || !buffer || capacity <= 1 || count <= 0)
            return false;
        const int limit = (count >= capacity) ? (capacity - 1) : count;
        __try
        {
            memcpy(buffer, text, limit * sizeof(wchar_t));
            buffer[limit] = L'\0';
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            buffer[0] = L'\0';
            return false;
        }
        return true;
    }

    std::string CopyAnsiText(LPCSTR text, int count)
    {
        char buffer[513] = {};
        if (!CopyAnsiBuffer(text, count, buffer, ARRAYSIZE(buffer)))
            return text && count > 0 ? "<unreadable>" : std::string();
        return buffer;
    }

    std::string CopyWideText(LPCWSTR text, int count)
    {
        wchar_t buffer[513] = {};
        if (!CopyWideBuffer(text, count, buffer, ARRAYSIZE(buffer)))
            return text && count > 0 ? "<unreadable>" : std::string();

        int bytes = WideCharToMultiByte(CP_UTF8, 0, buffer, -1, nullptr, 0, nullptr, nullptr);
        if (bytes <= 1)
            return {};

        std::string result(bytes, '\0');
        WideCharToMultiByte(CP_UTF8, 0, buffer, -1, &result[0], bytes, nullptr, nullptr);
        result.resize(bytes - 1);
        return result;
    }

    void CaptureShipTextA(LPCSTR text, int count, LPRECT rect, UINT format,
        uintptr_t caller, LPCWSTR original, int originalCount,
        LPCWSTR rendered, int renderedCount, uint32_t key)
    {
        if (!g_captureShipData || !text || count <= 0 || count > 128 || !rect ||
            rect->right - rect->left < 20 || rect->right - rect->left > 240 ||
            rect->top < 0 || rect->top > 240 || g_inHook)
            return;

        const std::string rawHex = CopyAnsiHex(text, count);
        const std::string cp932Text = CopyWideText(original, originalCount);
        const std::string renderedText = CopyWideText(rendered, renderedCount);
        g_inHook = true;
        log("[SHIPTRACE][A] caller=0x%08lX count=%d format=0x%08X rect=%ld,%ld,%ld,%ld key=%08X raw_hex=\"%s\" cp932=\"%s\" rendered=\"%s\"\n",
            static_cast<unsigned long>(caller), count, format,
            rect->left, rect->top, rect->right, rect->bottom, key,
            rawHex.c_str(), cp932Text.c_str(), renderedText.c_str());
        g_inHook = false;
    }

    void LogText(const char* api, int x, int y, int count, const std::string& text)
    {
        if (!g_logText || g_inHook)
            return;
        g_inHook = true;
        log("[UIDIAG][%s] x=%d y=%d count=%d text_utf8=\"%s\"\n",
            api, x, y, count, text.c_str());
        g_inHook = false;
    }

    int WINAPI HookGetSystemMetrics(int index)
    {
        int result = g_getSystemMetrics(index);
        if (g_logMetrics && !g_inHook)
        {
            g_inHook = true;
            log("[UIDIAG][GetSystemMetrics] index=%d result=%d\n", index, result);
            g_inHook = false;
        }
        return result;
    }

    UINT WINAPI HookGetACP()
    {
        if (g_layoutTunerEnabled)
            return LayoutTunerGetSnapshot().forceCp932 ? 932 : (g_getACP ? g_getACP() : 936);
        if (g_forceSystemCp932)
            return 932;
        return g_getACP ? g_getACP() : 936;
    }

    UINT WINAPI HookGetOEMCP()
    {
        if (g_layoutTunerEnabled)
            return LayoutTunerGetSnapshot().forceCp932 ? 932 : (g_getOEMCP ? g_getOEMCP() : 936);
        if (g_forceSystemCp932)
            return 932;
        return g_getOEMCP ? g_getOEMCP() : 936;
    }

    int WINAPI HookGetDeviceCaps(HDC dc, int index)
    {
        int result = g_getDeviceCaps(dc, index);
        if (g_logMetrics && IsInterestingDeviceCap(index) && !g_inHook)
        {
            g_inHook = true;
            log("[UIDIAG][GetDeviceCaps] dc=%p index=%d result=%d\n", dc, index, result);
            g_inHook = false;
        }
        return result;
    }

    BOOL WINAPI HookGetClientRect(HWND window, LPRECT rect)
    {
        BOOL result = g_getClientRect(window, rect);
        if (g_logMetrics && result && rect && !g_inHook)
        {
            g_inHook = true;
            log("[UIDIAG][GetClientRect] hwnd=%p l=%ld t=%ld r=%ld b=%ld\n",
                window, rect->left, rect->top, rect->right, rect->bottom);
            g_inHook = false;
        }
        return result;
    }

    BOOL WINAPI HookTextOutA(HDC dc, int x, int y, LPCSTR text, int count)
    {
        BOOL result = g_textOutA(dc, x, y, text, count);
        if (g_logText && !g_inHook)
            LogText("TextOutA", x, y, count, CopyAnsiText(text, count));
        return result;
    }

    BOOL WINAPI HookTextOutW(HDC dc, int x, int y, LPCWSTR text, int count)
    {
        BOOL result = g_textOutW(dc, x, y, text, count);
        if (g_logText && !g_inHook)
            LogText("TextOutW", x, y, count, CopyWideText(text, count));
        return result;
    }

    void ScaleXpTextExtentA(HDC dc, LPSIZE size);

    BOOL WINAPI HookGetTextExtentPoint32A(HDC dc, LPCSTR text, int count, LPSIZE size)
    {
        BOOL result = g_getTextExtentA(dc, text, count, size);
        if (result && size)
            ScaleXpTextExtentA(dc, size);
        if (g_logText && result && size && !g_inHook)
        {
            g_inHook = true;
            log("[UIDIAG][GetTextExtentPoint32A] count=%d cx=%ld cy=%ld text_utf8=\"%s\"\n",
                count, size->cx, size->cy, CopyAnsiText(text, count).c_str());
            g_inHook = false;
        }
        return result;
    }

    void ScaleXpTextExtentA(HDC dc, LPSIZE size)
    {
        if (!g_forceXpTextMetrics || !size || !g_getTextMetricsA)
            return;
        TEXTMETRICA metrics = {};
        if (!g_getTextMetricsA(dc, &metrics) || metrics.tmCharSet != SHIFTJIS_CHARSET ||
            metrics.tmAveCharWidth <= 0)
            return;

        int targetAverage = 0;
        if (g_layoutTunerEnabled)
        {
            const LayoutTunerSnapshot snapshot = LayoutTunerGetSnapshot();
            if (!snapshot.paragraphMeasure)
                return;
            const int index = LayoutTunerFontIndexForHeight(metrics.tmHeight);
            if (index >= 0 && snapshot.fonts[index].enabled)
                targetAverage = snapshot.fonts[index].averageWidth;
        }
        else
        {
            if (metrics.tmHeight == 12 || metrics.tmHeight == 13)
                targetAverage = 6;
            else if (metrics.tmHeight == 28)
                targetAverage = 15;
            else if (metrics.tmHeight == 38)
                targetAverage = 20;
            else if (metrics.tmHeight == 76)
                targetAverage = 39;
        }
        if (targetAverage > 0 && metrics.tmAveCharWidth != targetAverage)
            size->cx = MulDiv(size->cx, targetAverage, metrics.tmAveCharWidth);
    }

    BOOL WINAPI HookGetTextMetricsA(HDC dc, LPTEXTMETRICA metrics)
    {
        BOOL result = g_getTextMetricsA ? g_getTextMetricsA(dc, metrics) : FALSE;
        if (result && g_forceXpTextMetrics && metrics &&
            metrics->tmCharSet == SHIFTJIS_CHARSET)
        {
            if (g_layoutTunerEnabled)
            {
                LayoutTunerFontProfile profile = {};
                if (GetLayoutTunerFontProfile(metrics->tmHeight, &profile))
                {
                    metrics->tmAveCharWidth = profile.averageWidth;
                    metrics->tmMaxCharWidth = profile.maxWidth;
                }
            }
            else if (metrics->tmHeight == 12 || metrics->tmHeight == 13)
            {
                // Measured from the XP reference log for MS Gothic at the
                // font sizes used by the game.
                metrics->tmAveCharWidth = 6;
                metrics->tmMaxCharWidth = 12;
            }
            else if (metrics->tmHeight == 28)
            {
                metrics->tmAveCharWidth = 15;
                metrics->tmMaxCharWidth = 29;
            }
            else if (metrics->tmHeight == 38)
            {
                metrics->tmAveCharWidth = 20;
                metrics->tmMaxCharWidth = 39;
            }
            else if (metrics->tmHeight == 76)
            {
                metrics->tmAveCharWidth = 39;
                metrics->tmMaxCharWidth = 77;
            }
        }
        return result;
    }

    bool TryApplyLayoutTunerMeasure(HDC dc, LPCSTR text, int count, LPRECT rect,
        UINT format, uintptr_t callerAddress)
    {
        if (!g_layoutTunerEnabled || g_layoutTunerOnly ||
            !LayoutTunerGetSnapshot().paragraphMeasure || !dc || !text ||
            !rect || count <= 0 || callerAddress != kParagraphMeasureCaller ||
            !(format & DT_CALCRECT) || !g_getTextMetricsA)
            return false;

        TEXTMETRICA metrics = {};
        if (!HookGetTextMetricsA(dc, &metrics) ||
            metrics.tmCharSet != SHIFTJIS_CHARSET || metrics.tmAveCharWidth <= 0)
            return false;

        const LONG tunerWidth = static_cast<LONG>(XpCp932Width::Measure(
            text, count, metrics.tmAveCharWidth));
        if (tunerWidth <= 0)
            return false;
        rect->right = rect->left + tunerWidth;
        return true;
    }

    int WINAPI HookDrawTextXpTextMetricsA(HDC dc, LPCSTR text, int count,
        LPRECT rect, UINT format)
    {
        const uintptr_t callerAddress = reinterpret_cast<uintptr_t>(_ReturnAddress());
        const int actualCount = count < 0 ? (text ? lstrlenA(text) : 0) : count;
        const bool paragraphMeasure = g_layoutTunerOnly
            ? LayoutTunerGetSnapshot().paragraphMeasure
            : g_forceXpTextMetrics;

        if (g_layoutTunerOnly && paragraphMeasure && text && rect && actualCount > 0 &&
            callerAddress == kParagraphMeasureCaller && (format & DT_CALCRECT) &&
            g_getTextMetricsA)
        {
            TEXTMETRICA metrics = {};
            if (HookGetTextMetricsA(dc, &metrics) &&
                metrics.tmCharSet == SHIFTJIS_CHARSET && metrics.tmAveCharWidth > 0)
            {
                const LONG xpWidth = static_cast<LONG>(XpCp932Width::Measure(
                    text, actualCount, metrics.tmAveCharWidth));
                if (xpWidth > 0)
                {
                    RECT tunerRect = *rect;
                    tunerRect.right = tunerRect.left + xpWidth;
                    const int tunerResult = g_drawTextA(dc, text, count, &tunerRect, format);
                    *rect = tunerRect;
                    return tunerResult;
                }
            }
        }

        const int result = g_drawTextA(dc, text, count, rect, format);
        if (!g_layoutTunerOnly && g_forceXpTextMetrics && callerAddress == kParagraphDrawCaller &&
            dc && text && rect && actualCount > 0 && g_getTextExtentA &&
            InterlockedIncrement(&g_xpDrawEvents) <= 5000)
        {
            SIZE extent = {};
            TEXTMETRICA metrics = {};
            if (g_getTextExtentA(dc, text, actualCount, &extent) &&
                HookGetTextMetricsA(dc, &metrics))
            {
                log("[UIDIAG][XPDraw] count=%d format=0x%08X extentWidth=%ld extentHeight=%ld rectWidth=%ld rectHeight=%ld avgWidth=%ld maxWidth=%ld height=%ld\n",
                    actualCount, format, extent.cx, extent.cy,
                    rect->right - rect->left, rect->bottom - rect->top,
                    metrics.tmAveCharWidth, metrics.tmMaxCharWidth, metrics.tmHeight);
            }
        }
        if (!paragraphMeasure || !text || !rect ||
            callerAddress != kParagraphMeasureCaller ||
            !(format & DT_CALCRECT))
            return result;

        if (actualCount <= 0 || !g_getTextMetricsA)
            return result;

        TEXTMETRICA metrics = {};
        if (!HookGetTextMetricsA(dc, &metrics) ||
            metrics.tmCharSet != SHIFTJIS_CHARSET || metrics.tmAveCharWidth <= 0)
            return result;

        const LONG xpWidth = static_cast<LONG>(XpCp932Width::Measure(
            text, actualCount, metrics.tmAveCharWidth));
        if (xpWidth > 0)
        {
            const LONG originalWidth = rect->right - rect->left;
            rect->right = rect->left + xpWidth;
            if (!g_layoutTunerOnly)
            {
                log("[UIDIAG][XPWidth] caller=0x%08lX count=%d originalWidth=%ld xpWidth=%ld avgWidth=%ld height=%ld\n",
                    static_cast<unsigned long>(callerAddress), actualCount,
                    originalWidth, xpWidth,
                    metrics.tmAveCharWidth, metrics.tmHeight);
            }
        }
        return result;
    }

    BOOL WINAPI HookGetTextExtentPoint32W(HDC dc, LPCWSTR text, int count, LPSIZE size)
    {
        BOOL result = g_getTextExtentW(dc, text, count, size);
        if (g_logText && result && size && !g_inHook)
        {
            g_inHook = true;
            log("[UIDIAG][GetTextExtentPoint32W] count=%d cx=%ld cy=%ld text_utf8=\"%s\"\n",
                count, size->cx, size->cy, CopyWideText(text, count).c_str());
            g_inHook = false;
        }
        return result;
    }

    int WINAPI HookDrawTextA(HDC dc, LPCSTR text, int count, LPRECT rect, UINT format)
    {
        const uintptr_t callerAddress = reinterpret_cast<uintptr_t>(_ReturnAddress());
        if (g_patchOnly)
            return HookDrawTextPatch(dc, text, count, rect, format);

        RECT inputRect = {};
        if (rect)
            inputRect = *rect;
        RECT submittedRect = inputRect;
        const bool isTargetText = count == 102 || callerAddress == 0x0049B9F0;

        if (!g_patchOnly && g_reflowTargetText && callerAddress == 0x0049B324 &&
            text && rect && (format & DT_CALCRECT) && count >= 90)
        {
            log("[UIDIAG][ReflowProbe] measure count=%d right=%ld calcrect=%d\n",
                count, rect->right, (format & DT_CALCRECT) ? 1 : 0);
        }

        if (g_reflowTargetText && callerAddress == 0x0049B9F0 && rect &&
            (format & DT_WORDBREAK) && (rect->right - rect->left) == 254 &&
            g_cachedParagraphCount >= 90 && !g_cachedParagraph.empty())
        {
            if (rect->top == 0)
            {
                RECT reflowRect = *rect;
                reflowRect.bottom = reflowRect.top + 512;
                g_reflowDrawn = true;
                int reflowResult = g_drawTextA(dc, g_cachedParagraph.c_str(),
                    g_cachedParagraphCount, &reflowRect, format);
                log("[UIDIAG][Reflow] draw count=%d width=%ld height=%ld result=%d\n",
                    g_cachedParagraphCount, reflowRect.right - reflowRect.left,
                    reflowRect.bottom - reflowRect.top, reflowResult);
                return reflowResult;
            }
            if (g_reflowDrawn)
                return 0;
        }
        HFONT originalFont = nullptr;
        HFONT testFont = nullptr;
        HGDIOBJ previousFont = nullptr;
        if (g_narrowTargetFont && isTargetText && dc)
        {
            originalFont = reinterpret_cast<HFONT>(GetCurrentObject(dc, OBJ_FONT));
            LOGFONTA font = {};
            if (originalFont && GetObjectA(originalFont, sizeof(font), &font) == sizeof(font))
            {
                const int scale = g_targetFontScalePercent > 0 ? g_targetFontScalePercent : 100;
                font.lfHeight = (font.lfHeight * scale) / 100;
                if (font.lfHeight == 0)
                    font.lfHeight = 1;
                if (font.lfWidth != 0)
                    font.lfWidth = (font.lfWidth * scale) / 100;
                testFont = CreateFontIndirectA(&font);
                if (testFont)
                    previousFont = SelectObject(dc, testFont);
            }
        }
        if (g_doubleTargetWidth && isTargetText && rect)
        {
            rect->right = rect->left + 1148;
        }
        TryApplyLayoutTunerMeasure(dc, text, count, rect, format, callerAddress);
        if (rect)
            submittedRect = *rect;
        int result = g_drawTextA(dc, text, count, rect, format);

        // DT_CALCRECT writes the measured width into RECT only after the
        // original API returns. Cache the paragraph at that point.
        if (g_reflowTargetText && callerAddress == 0x0049B324 &&
            (format & DT_CALCRECT) && text && count >= 90 && rect && rect->right >= 490)
        {
            char buffer[2048] = {};
            if (CopyAnsiBuffer(text, count, buffer, sizeof(buffer)))
            {
                const int stored = count >= static_cast<int>(sizeof(buffer))
                    ? static_cast<int>(sizeof(buffer)) - 1 : count;
                g_cachedParagraph.assign(buffer, stored);
                g_cachedParagraphCount = stored;
                g_reflowDrawn = false;
                log("[UIDIAG][Reflow] cached count=%d measuredWidth=%ld\n", count, rect->right - rect->left);
            }
        }

        if (g_doubleTargetWidth && isTargetText && rect)
        {
            rect->right = rect->left + 1148;
        }
        if (!g_patchOnly && g_logText && callerAddress == 0x0049B9F0 && !g_inHook)
        {
            PVOID frames[8] = {};
            USHORT frameCount = CaptureStackBackTrace(1, 8, frames, nullptr);
            g_inHook = true;
            log("[UIDIAG][Stack] count=%d caller=0x%p frames=%hu", count, reinterpret_cast<PVOID>(callerAddress), frameCount);
            for (USHORT i = 0; i < frameCount; ++i)
                log(" f%u=0x%p", i, frames[i]);
            log("\n");
            g_inHook = false;
        }
        if (!g_patchOnly && g_logText && !g_inHook)
        {
            if (g_targetTextWidthOnly)
            {
                if (isTargetText && rect)
                {
                    LOGFONTA font = {};
                    TEXTMETRICA metrics = {};
                    HFONT hfont = reinterpret_cast<HFONT>(GetCurrentObject(dc, OBJ_FONT));
                    if (hfont)
                        GetObjectA(hfont, sizeof(font), &font);
                    GetTextMetricsA(dc, &metrics);
                    g_inHook = true;
                    log("[UIDIAG][TargetTextWidth] caller=0x%p input=%ldx%ld submitted=%ldx%ld output=%ldx%ld adjusted=%d count=%d format=0x%08X result=%d font=\"%s\" lfHeight=%ld lfWidth=%ld charset=%u weight=%ld tmHeight=%ld tmAve=%ld tmMax=%ld\n",
                        _ReturnAddress(),
                        inputRect.right - inputRect.left, inputRect.bottom - inputRect.top,
                        submittedRect.right - submittedRect.left, submittedRect.bottom - submittedRect.top,
                        rect->right - rect->left, rect->bottom - rect->top,
                        g_doubleTargetWidth ? 1 : 0, count, format, result, font.lfFaceName, font.lfHeight, font.lfWidth,
                        font.lfCharSet, font.lfWeight, metrics.tmHeight,
                        metrics.tmAveCharWidth, metrics.tmMaxCharWidth);
                    g_inHook = false;
                }
                if (testFont)
                {
                    SelectObject(dc, previousFont ? previousFont : originalFont);
                    DeleteObject(testFont);
                }
                return result;
            }
            g_inHook = true;
            log("[UIDIAG][DrawTextA] caller=0x%p text_ptr=0x%p count=%d format=0x%08X l=%ld t=%ld r=%ld b=%ld text_utf8=\"%s\" text_hex=\"%s\"\n",
                _ReturnAddress(),
                text,
                count, format, rect ? rect->left : 0, rect ? rect->top : 0,
                rect ? rect->right : 0, rect ? rect->bottom : 0, CopyAnsiText(text, count).c_str(),
                CopyAnsiHex(text, count).c_str());
            g_inHook = false;
        }
        if (testFont)
        {
            SelectObject(dc, previousFont ? previousFont : originalFont);
            DeleteObject(testFont);
        }
        return result;
    }

    int WINAPI HookDrawTextW(HDC dc, LPCWSTR text, int count, LPRECT rect, UINT format)
    {
        int result = g_drawTextW(dc, text, count, rect, format);
        if (g_logText && !g_inHook)
        {
            g_inHook = true;
            log("[UIDIAG][DrawTextW] count=%d format=0x%08X l=%ld t=%ld r=%ld b=%ld text_utf8=\"%s\"\n",
                count, format, rect ? rect->left : 0, rect ? rect->top : 0,
                rect ? rect->right : 0, rect ? rect->bottom : 0, CopyWideText(text, count).c_str());
            g_inHook = false;
        }
        return result;
    }

    BOOL WINAPI HookBitBlt(HDC dst, int x, int y, int width, int height, HDC src,
        int srcX, int srcY, DWORD rop)
    {
        BOOL result = g_bitBlt(dst, x, y, width, height, src, srcX, srcY, rop);
        if (g_logMetrics && !g_inHook)
        {
            g_inHook = true;
            log("[UIDIAG][BitBlt] dst=%p x=%d y=%d w=%d h=%d src=%p sx=%d sy=%d rop=0x%08X result=%d\n",
                dst, x, y, width, height, src, srcX, srcY, rop, result ? 1 : 0);
            g_inHook = false;
        }
        return result;
    }

    BOOL WINAPI HookStretchBlt(HDC dst, int x, int y, int width, int height, HDC src,
        int srcX, int srcY, int srcWidth, int srcHeight, DWORD rop)
    {
        BOOL result = g_stretchBlt(dst, x, y, width, height, src, srcX, srcY,
            srcWidth, srcHeight, rop);
        if (g_logMetrics && !g_inHook)
        {
            g_inHook = true;
            log("[UIDIAG][StretchBlt] dst=%p x=%d y=%d w=%d h=%d src=%p sx=%d sy=%d sw=%d sh=%d rop=0x%08X result=%d\n",
                dst, x, y, width, height, src, srcX, srcY, srcWidth, srcHeight, rop, result ? 1 : 0);
            g_inHook = false;
        }
        return result;
    }

    int WINAPI HookSetMapMode(HDC dc, int mode)
    {
        int result = g_setMapMode(dc, mode);
        if (g_logMetrics && !g_inHook)
        {
            g_inHook = true;
            log("[UIDIAG][SetMapMode] dc=%p mode=%d previous=%d\n", dc, mode, result);
            g_inHook = false;
        }
        return result;
    }

    BOOL WINAPI HookSetViewportOrgEx(HDC dc, int x, int y, LPPOINT previous)
    {
        BOOL result = g_setViewportOrgEx(dc, x, y, previous);
        if (g_logMetrics && !g_inHook)
        {
            g_inHook = true;
            log("[UIDIAG][SetViewportOrgEx] dc=%p x=%d y=%d previous=%d,%d result=%d\n",
                dc, x, y, previous ? previous->x : 0, previous ? previous->y : 0, result ? 1 : 0);
            g_inHook = false;
        }
        return result;
    }

    BOOL WINAPI HookSetWindowExtEx(HDC dc, int x, int y, LPSIZE previous)
    {
        BOOL result = g_setWindowExtEx(dc, x, y, previous);
        if (g_logMetrics && !g_inHook)
        {
            g_inHook = true;
            log("[UIDIAG][SetWindowExtEx] dc=%p x=%d y=%d previous=%ld,%ld result=%d\n",
                dc, x, y, previous ? previous->cx : 0, previous ? previous->cy : 0, result ? 1 : 0);
            g_inHook = false;
        }
        return result;
    }

    BOOL WINAPI HookSetViewportExtEx(HDC dc, int x, int y, LPSIZE previous)
    {
        BOOL result = g_setViewportExtEx(dc, x, y, previous);
        if (g_logMetrics && !g_inHook)
        {
            g_inHook = true;
            log("[UIDIAG][SetViewportExtEx] dc=%p x=%d y=%d previous=%ld,%ld result=%d\n",
                dc, x, y, previous ? previous->cx : 0, previous ? previous->cy : 0, result ? 1 : 0);
            g_inHook = false;
        }
        return result;
    }

    HRESULT STDMETHODCALLTYPE HookD3DCreateDevice(IDirect3D8* d3d, UINT adapter,
        D3DDEVTYPE deviceType, HWND window, DWORD behaviorFlags,
        D3DPRESENT_PARAMETERS* params, IDirect3DDevice8** device)
    {
        if (g_logMetrics && params && !g_inHook)
        {
            g_inHook = true;
            log("[UIDIAG][D3D8::CreateDevice] adapter=%u type=%u hwnd=%p windowed=%d backbuffer=%ux%u count=%u format=%u swap=%u msaa=%u refresh=%u interval=%u flags=0x%08X\n",
                adapter, static_cast<unsigned>(deviceType), window,
                params->Windowed ? 1 : 0, params->BackBufferWidth, params->BackBufferHeight,
                params->BackBufferCount, static_cast<unsigned>(params->BackBufferFormat),
                static_cast<unsigned>(params->SwapEffect), static_cast<unsigned>(params->MultiSampleType),
                params->FullScreen_RefreshRateInHz, params->FullScreen_PresentationInterval,
                behaviorFlags);
            g_inHook = false;
        }
        const HRESULT result = g_d3dCreateDevice(d3d, adapter, deviceType, window,
            behaviorFlags, params, device);
        return result;
    }

    IDirect3D8* WINAPI HookDirect3DCreate8(UINT sdkVersion)
    {
        IDirect3D8* d3d = g_direct3DCreate8(sdkVersion);
        if ((g_logMetrics || g_d3d8FrameTrace) && d3d && !g_d3dCreateDeviceHooked)
        {
            void** vtable = *reinterpret_cast<void***>(d3d);
            MH_STATUS createStatus = MH_ERROR_NOT_INITIALIZED;
            MH_STATUS enableStatus = MH_ERROR_NOT_INITIALIZED;
            if (vtable && vtable[15])
            {
                createStatus = MH_CreateHook(vtable[15],
                    reinterpret_cast<LPVOID>(&HookD3DCreateDevice),
                    reinterpret_cast<LPVOID*>(&g_d3dCreateDevice));
                if (createStatus == MH_OK)
                    enableStatus = MH_EnableHook(vtable[15]);
            }
            if (createStatus == MH_OK && enableStatus == MH_OK)
            {
                g_d3dCreateDeviceHooked = true;
                log("[UIDIAG][Direct3DCreate8] sdk=%u interface=%p CreateDeviceHooked=1 create=%d enable=%d\n",
                    sdkVersion, d3d, createStatus, enableStatus);
            }
            else
            {
                log("[UIDIAG][Direct3DCreate8] sdk=%u interface=%p CreateDeviceHooked=0 create=%d enable=%d\n",
                    sdkVersion, d3d, createStatus, enableStatus);
            }
        }
        return d3d;
    }

    bool AddApiHook(LPCWSTR module, LPCSTR name, LPVOID detour, LPVOID* original)
    {
        MH_STATUS status = MH_CreateHookApi(module, name, detour, original);
        if (status != MH_OK)
        {
            log("[UIDIAG] unable to hook %S!%s status=%d\n", module, name, status);
            return false;
        }
        return true;
    }
}

void SetSourceTrackingEnabled(bool enabled)
{
    InterlockedExchange(&g_sourceTrackingEnabled, enabled ? 1 : 0);
    if (!enabled)
        ClearSourceTrackingCache();
}

void ClearSourceTrackingCache()
{
    AcquireSRWLockExclusive(&g_sourceLock);
    g_sourceHandles.clear();
    g_sourceMappings.clear();
    g_sourceViews.clear();
    g_sourceSnapshots.clear();
    g_sourceCacheBytes = 0;
    ReleaseSRWLockExclusive(&g_sourceLock);
}

void TrackSourceFileHandle(HANDLE handle, LPCWSTR path)
{
    if (!IsSourceTrackingEnabledInternal())
        return;
    if (!handle || handle == INVALID_HANDLE_VALUE || !path ||
        GetFileType(handle) != FILE_TYPE_DISK)
        return;
    const std::wstring normalized = NormalizeSourcePath(path);
    if (normalized.empty())
        return;
    AcquireSRWLockExclusive(&g_sourceLock);
    SourceHandleContext context;
    context.path = normalized;
    g_sourceHandles[handle] = std::move(context);
    ReleaseSRWLockExclusive(&g_sourceLock);
}

void TrackSourceFileHandleA(HANDLE handle, LPCSTR path)
{
    if (!IsSourceTrackingEnabledInternal())
        return;
    if (!path)
        return;
    UINT codePage = CP_ACP;
    int length = MultiByteToWideChar(codePage, 0, path, -1, nullptr, 0);
    if (length <= 1)
    {
        codePage = 932;
        length = MultiByteToWideChar(codePage, 0, path, -1, nullptr, 0);
    }
    if (length <= 1)
        return;
    std::vector<wchar_t> converted(static_cast<size_t>(length));
    if (MultiByteToWideChar(codePage, 0, path, -1, converted.data(), length) <= 0)
        return;
    TrackSourceFileHandle(handle, converted.data());
}

void TrackSourceFileClose(HANDLE handle)
{
    if (!IsSourceTrackingEnabledInternal())
        return;
    if (!handle || handle == INVALID_HANDLE_VALUE)
        return;
    AcquireSRWLockExclusive(&g_sourceLock);
    g_sourceHandles.erase(handle);
    ReleaseSRWLockExclusive(&g_sourceLock);
}

void TrackSourceMappingClose(HANDLE handle)
{
    if (!IsSourceTrackingEnabledInternal())
        return;
    if (!handle || handle == INVALID_HANDLE_VALUE)
        return;
    AcquireSRWLockExclusive(&g_sourceLock);
    g_sourceMappings.erase(handle);
    ReleaseSRWLockExclusive(&g_sourceLock);
}

// mapping source snapshots are copied from the mapped view, never from a
// second disk read, so the original game resource remains untouched.
void TrackSourceFileMapping(HANDLE mapping, HANDLE file, ULONGLONG mappingSize)
{
    if (!IsSourceTrackingEnabledInternal())
        return;
    if (!mapping || mapping == INVALID_HANDLE_VALUE || !file ||
        file == INVALID_HANDLE_VALUE)
        return;

    SourceMappingContext context;
    AcquireSRWLockShared(&g_sourceLock);
    auto found = g_sourceHandles.find(file);
    if (found != g_sourceHandles.end())
        context.path = found->second.path;
    ReleaseSRWLockShared(&g_sourceLock);
    if (context.path.empty())
    {
        using GetFinalPathNameByHandleFn = DWORD(WINAPI*)(HANDLE, LPWSTR, DWORD, DWORD);
        HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
        auto getFinalPath = kernel32
            ? reinterpret_cast<GetFinalPathNameByHandleFn>(
                GetProcAddress(kernel32, "GetFinalPathNameByHandleW"))
            : nullptr;
        wchar_t finalPath[32768] = {};
        if (getFinalPath && getFinalPath(file, finalPath, ARRAYSIZE(finalPath), 0) > 0)
            context.path = NormalizeSourcePath(finalPath);
    }
    if (context.path.empty())
        return;

    if (mappingSize == 0)
    {
        LARGE_INTEGER fileSize = {};
        if (GetFileSizeEx(file, &fileSize) && fileSize.QuadPart > 0)
            mappingSize = static_cast<ULONGLONG>(fileSize.QuadPart);
    }
    context.fileSize = mappingSize;
    AcquireSRWLockExclusive(&g_sourceLock);
    g_sourceMappings[mapping] = std::move(context);
    ReleaseSRWLockExclusive(&g_sourceLock);
}

bool CopyMappedViewBytes(LPCVOID view, char* destination, size_t length)
{
    if (!view || !destination || length == 0)
        return false;
    __try
    {
        memcpy(destination, view, length);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
    return true;
}

void TrackSourceMappedView(LPVOID view, HANDLE mapping, ULONGLONG offset,
    SIZE_T bytesToMap)
{
    if (!IsSourceTrackingEnabledInternal())
        return;
    if (!view || !mapping)
        return;
    SourceMappedViewContext viewContext;
    AcquireSRWLockExclusive(&g_sourceLock);
    auto found = g_sourceMappings.find(mapping);
    if (found == g_sourceMappings.end())
    {
        ReleaseSRWLockExclusive(&g_sourceLock);
        return;
    }
    viewContext.mapping = mapping;
    viewContext.path = found->second.path;
    viewContext.offset = offset;
    SIZE_T available = bytesToMap;
    if (available == 0 && found->second.fileSize > offset)
        available = static_cast<SIZE_T>(found->second.fileSize - offset);
    viewContext.length = available;
    g_sourceViews[view] = viewContext;

    const size_t copyLength = IsSourceTrackingEnabledInternal()
        ? (available < kSourcePerFileLimit
            ? static_cast<size_t>(available) : kSourcePerFileLimit)
        : 0;
    if (copyLength > 0)
    {
        std::string copied(copyLength, '\0');
        if (CopyMappedViewBytes(view, &copied[0], copyLength))
            MergeSourceSnapshotLocked(viewContext.path, offset, true,
                copied.data(), copied.size());
    }
    ReleaseSRWLockExclusive(&g_sourceLock);
}

void TrackSourceUnmappedView(LPCVOID view)
{
    if (!IsSourceTrackingEnabledInternal())
        return;
    if (!view)
        return;
    AcquireSRWLockExclusive(&g_sourceLock);
    g_sourceViews.erase(view);
    ReleaseSRWLockExclusive(&g_sourceLock);
}

SourceTrackingStats GetSourceTrackingStats()
{
    SourceTrackingStats stats;
    stats.enabled = IsSourceTrackingEnabledInternal();
    AcquireSRWLockShared(&g_sourceLock);
    stats.handles = static_cast<DWORD>(g_sourceHandles.size());
    stats.mappings = static_cast<DWORD>(g_sourceMappings.size());
    stats.views = static_cast<DWORD>(g_sourceViews.size());
    stats.snapshots = static_cast<DWORD>(g_sourceSnapshots.size());
    stats.cachedBytes = static_cast<ULONGLONG>(g_sourceCacheBytes);
    ReleaseSRWLockShared(&g_sourceLock);
    return stats;
}

void TrackSourceFilePointer(HANDLE handle, ULONGLONG newPosition, bool known)
{
    if (!IsSourceTrackingEnabledInternal())
        return;
    AcquireSRWLockExclusive(&g_sourceLock);
    auto found = g_sourceHandles.find(handle);
    if (found != g_sourceHandles.end())
    {
        found->second.cursor = newPosition;
        found->second.cursorKnown = known;
    }
    ReleaseSRWLockExclusive(&g_sourceLock);
}

void TrackSourceRead(HANDLE handle, LPCVOID buffer, DWORD bytesRead,
    const OVERLAPPED* overlapped)
{
    if (!IsSourceTrackingEnabledInternal() || !buffer || bytesRead == 0)
        return;
    AcquireSRWLockExclusive(&g_sourceLock);
    auto found = g_sourceHandles.find(handle);
    if (found == g_sourceHandles.end())
    {
        ReleaseSRWLockExclusive(&g_sourceLock);
        return;
    }

    const size_t length = bytesRead < kSourcePerFileLimit
        ? static_cast<size_t>(bytesRead) : kSourcePerFileLimit;
    ULONGLONG offset = found->second.cursor;
    bool offsetKnown = found->second.cursorKnown;
    if (overlapped)
    {
        offset = (static_cast<ULONGLONG>(overlapped->OffsetHigh) << 32) |
            overlapped->Offset;
        offsetKnown = true;
    }
    MergeSourceSnapshotLocked(found->second.path, offset, offsetKnown,
        static_cast<const char*>(buffer), length);
    if (!overlapped)
    {
        if (found->second.cursorKnown)
            found->second.cursor += bytesRead;
    }
    ReleaseSRWLockExclusive(&g_sourceLock);
}

bool InstallUiDiagnostics(bool logText, bool logMetrics, bool targetTextWidthOnly,
    bool doubleTargetWidth, bool narrowTargetFont, int targetFontScalePercent, bool reflowTargetText,
    bool patchOnly, bool patchTrace, bool debugWindow, UINT patchTextCodePage,
    bool captureShipData, bool forceSystemCp932,
    bool d3d8FrameTrace, bool layoutTunerWindow, bool sourceTracking)
{
#ifdef WSC3_MINIMAL_RELEASE
    (void)logText;
    (void)logMetrics;
    (void)targetTextWidthOnly;
    (void)doubleTargetWidth;
    (void)narrowTargetFont;
    (void)targetFontScalePercent;
    (void)patchOnly;
    (void)patchTrace;
    (void)debugWindow;
    (void)captureShipData;
    (void)d3d8FrameTrace;
    (void)layoutTunerWindow;
    (void)sourceTracking;
    g_patchOnly = true;
    g_patchTrace = false;
    g_forceSystemCp932 = true;
    g_patchTextCodePage = 932;
    g_reflowTargetText = true;
    ResolveTranslationPath();
    LoadTranslations();

    InstallFrameLimiterFromGameIni();

    bool ok = true;
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    g_drawTextW = user32 ? reinterpret_cast<DrawTextWFn>(GetProcAddress(user32, "DrawTextW")) : nullptr;
    ok = AddApiHook(L"user32", "DrawTextA", (LPVOID)&HookDrawTextPatch,
        reinterpret_cast<LPVOID*>(&g_drawTextA)) && ok;
    ok = AddApiHook(L"user32", "DrawTextW", (LPVOID)&HookDrawTextWPatch,
        reinterpret_cast<LPVOID*>(&g_drawTextW)) && ok;
    ok = AddApiHook(L"kernel32", "MultiByteToWideChar", (LPVOID)&HookMultiByteToWideChar,
        reinterpret_cast<LPVOID*>(&g_multiByteToWideChar)) && ok;
    ok = AddApiHook(L"kernel32", "GetACP", (LPVOID)&HookGetACP,
        reinterpret_cast<LPVOID*>(&g_getACP)) && ok;
    ok = AddApiHook(L"kernel32", "GetOEMCP", (LPVOID)&HookGetOEMCP,
        reinterpret_cast<LPVOID*>(&g_getOEMCP)) && ok;
    return ok;
#else
    g_patchOnly = patchOnly;
    g_patchTrace = patchTrace;
    g_captureShipData = captureShipData;
    g_forceSystemCp932 = forceSystemCp932;
    g_d3d8FrameTrace = false;
    (void)d3d8FrameTrace;
    g_patchTextCodePage = patchTextCodePage == 932 ? 932 : 936;
    g_debugWindowEnabled = debugWindow;
    g_logText = patchOnly ? false : logText;
    g_logMetrics = patchOnly ? false : logMetrics;
    g_targetTextWidthOnly = targetTextWidthOnly;
    g_doubleTargetWidth = doubleTargetWidth;
    g_narrowTargetFont = narrowTargetFont;
    g_targetFontScalePercent = targetFontScalePercent > 0 ? targetFontScalePercent : 100;
    g_reflowTargetText = reflowTargetText;
    g_layoutTunerEnabled = false;
    g_layoutTunerOnly = false;
    SetSourceTrackingEnabled(sourceTracking && debugWindow);

    ResolveTranslationPath();
    LoadTranslations();

    InstallFrameLimiterFromGameIni();

    if (g_debugWindowEnabled)
        StartDebugWindow();

    bool ok = true;
    if (!g_patchOnly && (g_logMetrics || g_d3d8FrameTrace))
    {
        ok = AddApiHook(L"user32", "GetSystemMetrics", (LPVOID)&HookGetSystemMetrics,
            reinterpret_cast<LPVOID*>(&g_getSystemMetrics)) && ok;
        ok = AddApiHook(L"gdi32", "GetDeviceCaps", (LPVOID)&HookGetDeviceCaps,
            reinterpret_cast<LPVOID*>(&g_getDeviceCaps)) && ok;
        ok = AddApiHook(L"user32", "GetClientRect", (LPVOID)&HookGetClientRect,
            reinterpret_cast<LPVOID*>(&g_getClientRect)) && ok;
        ok = AddApiHook(L"gdi32", "BitBlt", (LPVOID)&HookBitBlt,
            reinterpret_cast<LPVOID*>(&g_bitBlt)) && ok;
        ok = AddApiHook(L"gdi32", "StretchBlt", (LPVOID)&HookStretchBlt,
            reinterpret_cast<LPVOID*>(&g_stretchBlt)) && ok;
        ok = AddApiHook(L"gdi32", "SetMapMode", (LPVOID)&HookSetMapMode,
            reinterpret_cast<LPVOID*>(&g_setMapMode)) && ok;
        ok = AddApiHook(L"gdi32", "SetViewportOrgEx", (LPVOID)&HookSetViewportOrgEx,
            reinterpret_cast<LPVOID*>(&g_setViewportOrgEx)) && ok;
        ok = AddApiHook(L"gdi32", "SetWindowExtEx", (LPVOID)&HookSetWindowExtEx,
            reinterpret_cast<LPVOID*>(&g_setWindowExtEx)) && ok;
        ok = AddApiHook(L"gdi32", "SetViewportExtEx", (LPVOID)&HookSetViewportExtEx,
            reinterpret_cast<LPVOID*>(&g_setViewportExtEx)) && ok;
    }

    if (g_patchOnly)
    {
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        g_drawTextW = user32 ? reinterpret_cast<DrawTextWFn>(GetProcAddress(user32, "DrawTextW")) : nullptr;
        ok = AddApiHook(L"user32", "DrawTextA", (LPVOID)&HookDrawTextPatch,
            reinterpret_cast<LPVOID*>(&g_drawTextA)) && ok;
        ok = AddApiHook(L"user32", "DrawTextW", (LPVOID)&HookDrawTextWPatch,
            reinterpret_cast<LPVOID*>(&g_drawTextW)) && ok;
        ok = AddApiHook(L"kernel32", "MultiByteToWideChar", (LPVOID)&HookMultiByteToWideChar,
            reinterpret_cast<LPVOID*>(&g_multiByteToWideChar)) && ok;
        if (g_forceSystemCp932)
        {
            ok = AddApiHook(L"kernel32", "GetACP", (LPVOID)&HookGetACP,
                reinterpret_cast<LPVOID*>(&g_getACP)) && ok;
            ok = AddApiHook(L"kernel32", "GetOEMCP", (LPVOID)&HookGetOEMCP,
                reinterpret_cast<LPVOID*>(&g_getOEMCP)) && ok;
        }
    }
    else if (g_logText)
    {
        ok = AddApiHook(L"gdi32", "TextOutA", (LPVOID)&HookTextOutA,
            reinterpret_cast<LPVOID*>(&g_textOutA)) && ok;
        ok = AddApiHook(L"gdi32", "TextOutW", (LPVOID)&HookTextOutW,
            reinterpret_cast<LPVOID*>(&g_textOutW)) && ok;
        ok = AddApiHook(L"gdi32", "GetTextExtentPoint32A", (LPVOID)&HookGetTextExtentPoint32A,
            reinterpret_cast<LPVOID*>(&g_getTextExtentA)) && ok;
        ok = AddApiHook(L"gdi32", "GetTextExtentPoint32W", (LPVOID)&HookGetTextExtentPoint32W,
            reinterpret_cast<LPVOID*>(&g_getTextExtentW)) && ok;
        ok = AddApiHook(L"gdi32", "CreateFontA", (LPVOID)&HookCreateFontA,
            reinterpret_cast<LPVOID*>(&g_createFontA)) && ok;
        ok = AddApiHook(L"gdi32", "CreateFontIndirectA", (LPVOID)&HookCreateFontIndirectA,
            reinterpret_cast<LPVOID*>(&g_createFontIndirectA)) && ok;
        ok = AddApiHook(L"user32", "DrawTextA", (LPVOID)&HookDrawTextA,
            reinterpret_cast<LPVOID*>(&g_drawTextA)) && ok;
        ok = AddApiHook(L"user32", "DrawTextW", (LPVOID)&HookDrawTextW,
            reinterpret_cast<LPVOID*>(&g_drawTextW)) && ok;
    }

#ifdef WSC3_FULL_DEBUG
    // Apply the saved XP profile even when the tuning window is hidden.
    ok = InstallLayoutTunerWindowHooks(layoutTunerWindow) && ok;
#else
    if (layoutTunerWindow)
        ok = InstallLayoutTunerWindowHooks(true) && ok;
#endif

    if (!g_patchOnly)
    {
        log("[UIDIAG] installed text=%d metrics=%d targetWidthOnly=%d doubleWidth=%d narrowFont=%d fontScale=%d reflow=%d result=%d\n",
            logText ? 1 : 0, logMetrics ? 1 : 0, targetTextWidthOnly ? 1 : 0,
            doubleTargetWidth ? 1 : 0, narrowTargetFont ? 1 : 0,
            g_targetFontScalePercent, reflowTargetText ? 1 : 0, ok ? 1 : 0);
    }
    return ok;
#endif
}

bool InstallCp932CompatibilityHooks()
{
    g_forceSystemCp932 = true;
    bool ok = true;
    ok = AddApiHook(L"kernel32", "GetACP", (LPVOID)&HookGetACP,
        reinterpret_cast<LPVOID*>(&g_getACP)) && ok;
    ok = AddApiHook(L"kernel32", "GetOEMCP", (LPVOID)&HookGetOEMCP,
        reinterpret_cast<LPVOID*>(&g_getOEMCP)) && ok;
    return ok;
}

bool InstallCp932XpTextMetricsHooks()
{
    g_forceSystemCp932 = true;
    g_forceXpTextMetrics = true;
    bool ok = true;
    ok = AddApiHook(L"kernel32", "GetACP", (LPVOID)&HookGetACP,
        reinterpret_cast<LPVOID*>(&g_getACP)) && ok;
    ok = AddApiHook(L"kernel32", "GetOEMCP", (LPVOID)&HookGetOEMCP,
        reinterpret_cast<LPVOID*>(&g_getOEMCP)) && ok;
    ok = AddApiHook(L"gdi32", "GetTextMetricsA", (LPVOID)&HookGetTextMetricsA,
        reinterpret_cast<LPVOID*>(&g_getTextMetricsA)) && ok;
    ok = AddApiHook(L"gdi32", "GetTextExtentPoint32A", (LPVOID)&HookGetTextExtentPoint32A,
        reinterpret_cast<LPVOID*>(&g_getTextExtentA)) && ok;
    ok = AddApiHook(L"gdi32", "CreateFontA", (LPVOID)&HookCreateFontA,
        reinterpret_cast<LPVOID*>(&g_createFontA)) && ok;
    ok = AddApiHook(L"gdi32", "CreateFontIndirectA", (LPVOID)&HookCreateFontIndirectA,
        reinterpret_cast<LPVOID*>(&g_createFontIndirectA)) && ok;
    ok = AddApiHook(L"user32", "DrawTextA", (LPVOID)&HookDrawTextXpTextMetricsA,
        reinterpret_cast<LPVOID*>(&g_drawTextA)) && ok;
    log("[UIDIAG] CP932 XP text metrics-only hooks result=%d\n", ok ? 1 : 0);
    return ok;
}

bool InstallLayoutTunerHooks()
{
    g_layoutTunerEnabled = true;
    g_layoutTunerOnly = true;
    g_forceSystemCp932 = true;
    g_forceXpTextMetrics = true;
    LayoutTunerInitialize();
    bool ok = true;
    ok = AddApiHook(L"kernel32", "GetACP", (LPVOID)&HookGetACP,
        reinterpret_cast<LPVOID*>(&g_getACP)) && ok;
    ok = AddApiHook(L"kernel32", "GetOEMCP", (LPVOID)&HookGetOEMCP,
        reinterpret_cast<LPVOID*>(&g_getOEMCP)) && ok;
    ok = AddApiHook(L"gdi32", "GetTextMetricsA", (LPVOID)&HookGetTextMetricsA,
        reinterpret_cast<LPVOID*>(&g_getTextMetricsA)) && ok;
    ok = AddApiHook(L"gdi32", "GetTextExtentPoint32A", (LPVOID)&HookGetTextExtentPoint32A,
        reinterpret_cast<LPVOID*>(&g_getTextExtentA)) && ok;
    ok = AddApiHook(L"gdi32", "CreateFontA", (LPVOID)&HookCreateFontA,
        reinterpret_cast<LPVOID*>(&g_createFontA)) && ok;
    ok = AddApiHook(L"gdi32", "CreateFontIndirectA", (LPVOID)&HookCreateFontIndirectA,
        reinterpret_cast<LPVOID*>(&g_createFontIndirectA)) && ok;
    ok = AddApiHook(L"user32", "DrawTextA", (LPVOID)&HookDrawTextXpTextMetricsA,
        reinterpret_cast<LPVOID*>(&g_drawTextA)) && ok;
    if (ok)
        ok = LayoutTunerWindowStart(reinterpret_cast<HINSTANCE>(GetModuleHandleW(nullptr)));
    return ok;
}

bool InstallLayoutTunerWindowHooks(bool showWindow)
{
    g_layoutTunerEnabled = true;
    g_layoutTunerOnly = false;
    g_forceSystemCp932 = true;
    g_forceXpTextMetrics = true;
    LayoutTunerInitialize();

    bool ok = true;
    if (!g_getACP)
        ok = AddApiHook(L"kernel32", "GetACP", (LPVOID)&HookGetACP,
            reinterpret_cast<LPVOID*>(&g_getACP)) && ok;
    if (!g_getOEMCP)
        ok = AddApiHook(L"kernel32", "GetOEMCP", (LPVOID)&HookGetOEMCP,
            reinterpret_cast<LPVOID*>(&g_getOEMCP)) && ok;
    if (!g_getTextMetricsA)
        ok = AddApiHook(L"gdi32", "GetTextMetricsA", (LPVOID)&HookGetTextMetricsA,
            reinterpret_cast<LPVOID*>(&g_getTextMetricsA)) && ok;
    if (!g_getTextExtentA)
        ok = AddApiHook(L"gdi32", "GetTextExtentPoint32A", (LPVOID)&HookGetTextExtentPoint32A,
            reinterpret_cast<LPVOID*>(&g_getTextExtentA)) && ok;
    if (!g_createFontA)
        ok = AddApiHook(L"gdi32", "CreateFontA", (LPVOID)&HookCreateFontA,
            reinterpret_cast<LPVOID*>(&g_createFontA)) && ok;
    if (!g_createFontIndirectA)
        ok = AddApiHook(L"gdi32", "CreateFontIndirectA", (LPVOID)&HookCreateFontIndirectA,
            reinterpret_cast<LPVOID*>(&g_createFontIndirectA)) && ok;
    if (!g_drawTextA)
        ok = AddApiHook(L"user32", "DrawTextA", (LPVOID)&HookDrawTextA,
            reinterpret_cast<LPVOID*>(&g_drawTextA)) && ok;
    if (ok && showWindow)
        ok = LayoutTunerWindowStart(reinterpret_cast<HINSTANCE>(GetModuleHandleW(nullptr)));
    return ok;
}
