#pragma once

#include <windows.h>

enum { kLayoutTunerFontCount = 5 };

struct LayoutTunerFontProfile
{
    int lfWidth;
    int averageWidth;
    int maxWidth;
    bool enabled;
};

struct LayoutTunerSnapshot
{
    bool forceCp932;
    bool paragraphMeasure;
    LayoutTunerFontProfile fonts[kLayoutTunerFontCount];
};

void LayoutTunerInitialize();
LayoutTunerSnapshot LayoutTunerGetSnapshot();
void LayoutTunerSetFont(int index, const LayoutTunerFontProfile& profile);
void LayoutTunerSetFlags(bool forceCp932, bool paragraphMeasure);
void LayoutTunerResetXpDefaults();
int LayoutTunerFontIndexForHeight(int height);
