#pragma once

namespace XpCp932Width
{
    inline bool IsLeadByte(unsigned char value)
    {
        return (value >= 0x81 && value <= 0x9F) ||
            (value >= 0xE0 && value <= 0xFC);
    }

    inline int Measure(const char* text, int count, int averageWidth)
    {
        if (!text || count <= 0 || averageWidth <= 0)
            return 0;

        int current = 0;
        int widest = 0;
        for (int i = 0; i < count; ++i)
        {
            const unsigned char value = static_cast<unsigned char>(text[i]);
            if (value == '\r' || value == '\n')
            {
                if (current > widest)
                    widest = current;
                current = 0;
                if (value == '\r' && i + 1 < count && text[i + 1] == '\n')
                    ++i;
                continue;
            }

            current += IsLeadByte(value) && i + 1 < count
                ? averageWidth * 2 : averageWidth;
            if (IsLeadByte(value) && i + 1 < count)
                ++i;
        }

        return current > widest ? current : widest;
    }
}
