#ifndef AVI_READER_SEEK_DATA_H
#define AVI_READER_SEEK_DATA_H
#include "AVIReaderTestData.h"

enum AviSeekOptions {
    SeekBadDelta = 1, SeekPartialKey = 2, SeekNoKey = 4, SeekNoIndex = 8,
    SeekStandardIndex = 16, SeekPalette = 32, SeekDrop = 64, SeekLayout = 128,
    SeekFalseLastKey = 256, SeekAbsoluteRle = 512
};

static unsigned AddAviSeekHeader(AviReaderTestData &a, const char *tag, unsigned size) {
    const unsigned at = a.Movi, total = size + 8;
    std::memmove(a.Data + at + total, a.Data + at, a.Size - at);
    std::memset(a.Data + at, 0, total); std::memcpy(a.Data + at, tag, 4); a.Word(at + 4, size);
    for (unsigned i = 8; i + 12 <= at; ++i)
        if (!std::memcmp(a.Data + i, "LIST", 4) &&
            (!std::memcmp(a.Data + i + 8, "hdrl", 4) || !std::memcmp(a.Data + i + 8, "strl", 4))) {
            unsigned value = 0;
            for (unsigned j = 0; j < 4; ++j) value |= unsigned(a.Data[i + 4 + j]) << (8 * j);
            a.Word(i + 4, value + total);
        }
    a.Movi += total; a.Size += total;
    return at;
}

// Four pictures: red key, damaged or green delta, blue/partial key, held picture.
// codec 0/1 = RGB555/indexed Video1; 2/3 = RLE8/RLE4.
static void MakeAviSeek(AviReaderTestData &a, unsigned codec = 0, unsigned options = SeekBadDelta) {
    const unsigned bpp = codec == 0 ? 16 : codec == 3 ? 4 : 8;
    a.Start(bpp, codec < 2 ? 0x4d415243u : codec == 2 ? 1 : 2, 4, 4);
    for (unsigned i = 0; i + 64 < a.Movi; ++i)
        if (!std::memcmp(a.Data + i, "strf", 4) && bpp <= 8) a.Word(i + 60, 0x0000ff00u);
    const unsigned standard = options & SeekStandardIndex ? AddAviSeekHeader(a, "indx", 56) : 0;
    if (options & SeekLayout) {
        const unsigned at = AddAviSeekHeader(a, "vprp", 68);
        a.Word(at + 40, 1); // One full-size field, exercising the layout wrapper.
        for (unsigned j = 0; j < 4; ++j) a.Word(at + 44 + j * 4, 4);
    }
    unsigned offsets[4] = {}, sizes[4] = {}, keys[4] = {};
    for (unsigned frame = 0; frame < 4; ++frame) {
        uint8_t data[128] = {}; unsigned size = 0;
        const bool partial = frame == 3 || (frame == 2 && (options & SeekPartialKey));
        const unsigned color = frame == 0 ? 1 : frame == 1 ? 3 : 2;
        if (frame == 1 && (options & SeekBadDelta)) { data[size++] = 0; }
        else if (codec < 2) {
            if (partial) { data[size++] = 1; data[size++] = 0x84; }
            else if (codec == 1) { data[size++] = uint8_t(color); data[size++] = 0x80; }
            else { data[size++] = color == 1 ? 0 : color == 3 ? 0xe0 : 0x1f; data[size++] = color == 1 ? 0xfc : color == 3 ? 0x83 : 0x80; }
        } else {
            if (!partial) for (unsigned row = 0; row < 4; ++row) {
                if (frame == 2 && (options & SeekAbsoluteRle)) {
                    data[size++] = 0; data[size++] = 4;
                    for (unsigned i = 0; i < (codec == 2 ? 4u : 2u); ++i) data[size++] = codec == 2 ? 2 : 0x22;
                } else { data[size++] = 4; data[size++] = uint8_t(codec == 2 ? color : color * 17); }
                data[size++] = 0; data[size++] = 0;
            }
            else if (frame == 2) { data[size++] = 1; data[size++] = codec == 2 ? 2 : 0x22; }
            data[size++] = 0; data[size++] = 1;
        }
        if (frame == 3 && (options & SeekPalette)) { const uint8_t yellow[] = {255,255,0}; a.Palette(2,1,yellow); }
        a.Frame(data, frame == 3 && (options & SeekDrop) ? 0 : size);
        AviReaderTestData::Entry &entry = a.Index[a.Entries - 1];
        keys[frame] = frame == 0 || (frame == 2 && !(options & SeekNoKey)) || (frame == 3 && (options & SeekFalseLastKey));
        entry.Flags = keys[frame] ? 0x10 : 0;
        offsets[frame] = entry.Offset; sizes[frame] = entry.Size;
    }
    if (standard) {
        a.Word(standard + 8, 2, 2); a.Word(standard + 11, 1, 1); a.Word(standard + 12, 4);
        std::memcpy(a.Data + standard + 16, "00dc", 4);
        for (unsigned i = 0; i < 4; ++i) {
            a.Word(standard + 32 + i * 8, offsets[i] + 8);
            a.Word(standard + 36 + i * 8, sizes[i] | (keys[i] ? 0 : 0x80000000u));
        }
    }
    a.Finish(!(options & (SeekNoIndex | SeekStandardIndex)));
}
#endif
