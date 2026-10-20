#ifndef AVI_READER_VIDEO1_DIMENSIONS_DATA_H
#define AVI_READER_VIDEO1_DIMENSIONS_DATA_H
#include "AVIReaderTestData.h"

// Hand-authored CRAM blocks. No production muxer or encoder is used.
static void MakeAviVideo1Dimensions(AviReaderTestData &a, int width, int height,
                                   unsigned bpp = 8, unsigned mode = 0,
                                   bool initialSkip = false, bool indexed = true,
                                   bool paletteChange = false, uint32_t codec = 0x4d415243u) {
    a.Start(bpp, codec, width, height, false, codec);
    if (bpp == 8) {
        for (unsigned i = 0; i + 48 < a.Size; ++i)
            if (!std::memcmp(a.Data + i, "strf", 4)) {
                a.Word(i + 48, 0x00204080u); // palette[0] = RGB(32,64,128).
                break;
            }
    }
    const unsigned blocks = unsigned(width / 4) * unsigned(height / 4);
    uint8_t bytes[1024] = {};
    unsigned size = 0;
    for (unsigned i = 0; i < blocks && !initialSkip; ++i) {
        if (mode == 0) {
            bytes[size++] = bpp == 8 ? uint8_t(i % 2 ? 2 : 1) : uint8_t(i % 2 ? 0x1f : 0);
            bytes[size++] = bpp == 8 ? 0x80 : uint8_t(i % 2 ? 0x80 : 0xfc);
        } else {
            const uint8_t two[] = {0xaa, 0x55, 1, 2};
            const uint8_t eight[] = {0xaa, 0x95, 1, 2, 2, 1, 1, 2, 2, 1};
            const unsigned n = mode == 1 ? sizeof(two) : sizeof(eight);
            std::memcpy(bytes + size, mode == 1 ? two : eight, n);
            size += n;
        }
    }
    if (initialSkip) { bytes[size++] = uint8_t(blocks); bytes[size++] = 0x84; }
    if (!blocks) { bytes[0] = bytes[1] = 0; size = 2; }
    a.Frame(bytes, size);
    if (paletteChange) {
        const uint8_t rgb[] = {255, 255, 0, 0, 255, 0};
        a.Palette(0, 2, rgb);
    }
    const uint8_t skip[] = {uint8_t(blocks), 0x84};
    const uint8_t none[] = {0, 0};
    a.Frame(blocks ? skip : none, 2);
    a.Index[a.Entries - 1].Flags = 0;
    size = 0;
    if (blocks) {
        bytes[size++] = bpp == 8 ? 2 : 0x1f;
        bytes[size++] = 0x80;
        if (blocks > 1) { bytes[size++] = uint8_t(blocks - 1); bytes[size++] = 0x84; }
    } else { bytes[size++] = 0; bytes[size++] = 0; }
    a.Frame(bytes, size);
    a.Index[a.Entries - 1].Flags = 0;
    a.Finish(indexed);
}
#endif
