#include "LayoutTuner.h"

namespace
{
    const int kMaxMetric = 4096;
    volatile LONG g_state = 0;
    CRITICAL_SECTION g_lock;
    LayoutTunerSnapshot g_snapshot = {};

    LayoutTunerSnapshot XpDefaults()
    {
        LayoutTunerSnapshot defaults = {};
        defaults.forceCp932 = true;
        defaults.paragraphMeasure = false;
        defaults.fonts[0] = { 5, 5, 11, true };
        defaults.fonts[1] = { 0, 7, 32, true };
        defaults.fonts[2] = { 15, 15, 34, true };
        defaults.fonts[3] = { 0, 20, 39, false };
        defaults.fonts[4] = { 0, 39, 77, false };
        return defaults;
    }

    int ClampMetric(int value)
    {
        if (value < 0)
            return 0;
        if (value > kMaxMetric)
            return kMaxMetric;
        return value;
    }

    int ClampPositiveMetric(int value)
    {
        const int clamped = ClampMetric(value);
        return clamped > 0 ? clamped : 1;
    }

    void EnsureInitialized()
    {
        if (InterlockedCompareExchange(&g_state, 2, 2) == 2)
            return;
        if (InterlockedCompareExchange(&g_state, 1, 0) == 0)
        {
            InitializeCriticalSection(&g_lock);
            g_snapshot = XpDefaults();
            InterlockedExchange(&g_state, 2);
            return;
        }
        while (InterlockedCompareExchange(&g_state, 2, 2) != 2)
            Sleep(0);
    }
}

void LayoutTunerInitialize()
{
    EnsureInitialized();
}

LayoutTunerSnapshot LayoutTunerGetSnapshot()
{
    EnsureInitialized();
    EnterCriticalSection(&g_lock);
    const LayoutTunerSnapshot snapshot = g_snapshot;
    LeaveCriticalSection(&g_lock);
    return snapshot;
}

void LayoutTunerSetFont(int index, const LayoutTunerFontProfile& profile)
{
    EnsureInitialized();
    if (index < 0 || index >= kLayoutTunerFontCount)
        return;
    EnterCriticalSection(&g_lock);
    g_snapshot.fonts[index] = profile;
    g_snapshot.fonts[index].lfWidth = ClampMetric(profile.lfWidth);
    g_snapshot.fonts[index].averageWidth = ClampPositiveMetric(profile.averageWidth);
    g_snapshot.fonts[index].maxWidth = ClampPositiveMetric(profile.maxWidth);
    LeaveCriticalSection(&g_lock);
}

void LayoutTunerSetFlags(bool forceCp932, bool paragraphMeasure)
{
    EnsureInitialized();
    EnterCriticalSection(&g_lock);
    g_snapshot.forceCp932 = forceCp932;
    g_snapshot.paragraphMeasure = paragraphMeasure;
    LeaveCriticalSection(&g_lock);
}

void LayoutTunerResetXpDefaults()
{
    EnsureInitialized();
    EnterCriticalSection(&g_lock);
    g_snapshot = XpDefaults();
    LeaveCriticalSection(&g_lock);
}

int LayoutTunerFontIndexForHeight(int height)
{
    const int absoluteHeight = height < 0 ? -height : height;
    if (absoluteHeight == 12 || absoluteHeight == 13)
        return 0;
    if (absoluteHeight == 16)
        return 1;
    if (absoluteHeight == 28)
        return 2;
    if (absoluteHeight == 38)
        return 3;
    if (absoluteHeight == 76)
        return 4;
    return -1;
}
