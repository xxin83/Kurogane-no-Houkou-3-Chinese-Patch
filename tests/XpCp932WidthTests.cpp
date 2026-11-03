#include <cstdio>
#include <cstring>
#include "../src/XpCp932Width.h"

static int Fail(const char* message)
{
    std::printf("FAIL: %s\n", message);
    return 1;
}

int main()
{
    const char paragraph[] =
        "\x8D\xEC\x90\xED\x8E\x77\x97\xDF\x82\xF0\x8E\xF3\x82\xAF\x82\xE9"
        "\x82\xC9\x82\xC2\x82\xCD\x81\x41\x8D\xB6\x8F\xE3\x82\xCC\x81\x75"
        "\x8D\xEC\x90\xED\x8E\x77\x97\xDF";
    if (static_cast<int>(std::strlen(paragraph)) != 40)
        return Fail("fixture must contain 40 CP932 bytes");
    if (XpCp932Width::Measure(paragraph, 40, 6) != 240)
        return Fail("20 Japanese characters must measure as 240 pixels");

    char eighty[81] = {};
    std::memcpy(eighty, paragraph, 40);
    std::memcpy(eighty + 40, paragraph, 40);
    if (XpCp932Width::Measure(eighty, 80, 6) != 480)
        return Fail("the 80-byte XP reference paragraph must measure as 480 pixels");

    const char mixed[] = "ABC\x81\x40" "DEF";
    if (XpCp932Width::Measure(mixed, 8, 6) != 48)
        return Fail("ASCII and a full-width CP932 space must use 6 and 12 pixels");

    const char lines[] = "\x82\xa0\x82\xa2\r\n\x82\xa4";
    if (XpCp932Width::Measure(lines, 8, 6) != 24)
        return Fail("measurement must return the widest explicit line");

    std::puts("XP CP932 width tests: PASS");
    return 0;
}
