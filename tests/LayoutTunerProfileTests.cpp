#include <cstdio>
#include "../src/LayoutTuner.h"

static int Fail(const char* message)
{
    std::printf("FAIL: %s\n", message);
    return 1;
}

static bool SameFont(const LayoutTunerFontProfile& actual, int lfWidth,
    int averageWidth, int maxWidth, bool enabled)
{
    return actual.lfWidth == lfWidth &&
        actual.averageWidth == averageWidth &&
        actual.maxWidth == maxWidth &&
        actual.enabled == enabled;
}

int main()
{
    LayoutTunerInitialize();
    LayoutTunerSnapshot snapshot = LayoutTunerGetSnapshot();
    if (!snapshot.forceCp932 || snapshot.paragraphMeasure)
        return Fail("manual defaults must enable CP932 and leave paragraph measurement disabled");
    if (!SameFont(snapshot.fonts[0], 5, 5, 11, true) ||
        !SameFont(snapshot.fonts[1], 0, 7, 32, true) ||
        !SameFont(snapshot.fonts[2], 15, 15, 34, true) ||
        !SameFont(snapshot.fonts[3], 0, 20, 39, false) ||
        !SameFont(snapshot.fonts[4], 0, 39, 77, false))
        return Fail("manual default font profiles are incorrect");

    if (LayoutTunerFontIndexForHeight(12) != 0 ||
        LayoutTunerFontIndexForHeight(13) != 0 ||
        LayoutTunerFontIndexForHeight(16) != 1 ||
        LayoutTunerFontIndexForHeight(28) != 2 ||
        LayoutTunerFontIndexForHeight(38) != 3 ||
        LayoutTunerFontIndexForHeight(76) != 4 ||
        LayoutTunerFontIndexForHeight(20) != -1)
        return Fail("font height mapping is incorrect");

    const LayoutTunerFontProfile changed = { 17, 21, 42, true };
    LayoutTunerSetFont(3, changed);
    snapshot = LayoutTunerGetSnapshot();
    if (!SameFont(snapshot.fonts[3], 17, 21, 42, true))
        return Fail("font profile update must publish a complete row");

    const LayoutTunerFontProfile invalid = { -10, 0, 5000, true };
    LayoutTunerSetFont(1, invalid);
    snapshot = LayoutTunerGetSnapshot();
    if (!SameFont(snapshot.fonts[1], 0, 1, 4096, true))
        return Fail("invalid metrics must be clamped to safe values");

    LayoutTunerSetFlags(false, false);
    snapshot = LayoutTunerGetSnapshot();
    if (snapshot.forceCp932 || snapshot.paragraphMeasure)
        return Fail("flags must update together");

    LayoutTunerResetXpDefaults();
    snapshot = LayoutTunerGetSnapshot();
    if (!snapshot.forceCp932 || snapshot.paragraphMeasure ||
        !SameFont(snapshot.fonts[3], 0, 20, 39, false))
        return Fail("reset must restore all manual defaults");

    std::puts("LayoutTuner profile tests: PASS");
    return 0;
}
