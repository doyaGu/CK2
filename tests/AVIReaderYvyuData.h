#ifndef AVI_READER_YVYU_DATA_H
#define AVI_READER_YVYU_DATA_H
#include "AVIReaderTestData.h"

// Literal BT.601 packed pairs, independent of the production conversion code.
static void MakeAviYvyu(AviReaderTestData &a, int width = 4, int height = 2,
                        uint32_t codec = 0x55595659u, bool drop = false,
                        bool indexed = true, bool records = false,
                        uint32_t handler = 0x55595659u) {
    a.Start(16, codec, width, height, records, handler);
    const int rows = height < 0 ? -height : height;
    const uint8_t pairs[2][2][4] = {
        {{16, 128, 235, 128}, {81, 90, 81, 240}},
        {{235, 128, 16, 128}, {41, 240, 41, 110}}
    };
    for (int frame = 0; frame < 3; ++frame) {
        uint8_t bytes[512] = {};
        unsigned used = 0;
        for (int y = 0; y < rows; ++y) for (int x = 0; x < width; x += 2) {
            const uint8_t *p = pairs[(y + (frame == 2)) % 2][x / 2 % 2];
            if (codec == 0x59565955u) {
                bytes[used++] = p[1]; bytes[used++] = p[0];
                bytes[used++] = p[3]; bytes[used++] = p[2];
            } else if (codec == 0x55595659u) {
                bytes[used++] = p[0]; bytes[used++] = p[3];
                bytes[used++] = p[2]; bytes[used++] = p[1];
            } else {
                std::memcpy(bytes + used, p, 4); used += 4;
            }
        }
        a.Frame(drop && frame == 1 ? nullptr : bytes, drop && frame == 1 ? 0 : used);
    }
    a.Finish(indexed);
}
#endif
