#ifndef AVI_READER_STREAM_HEADER_DATA_H
#define AVI_READER_STREAM_HEADER_DATA_H
#include "AVIReaderTestData.h"
#include "AVIReaderSeekData.h"
#include "AVIReaderYvyuData.h"

static unsigned AviHeaderReadWord(const uint8_t *data) {
    return unsigned(data[0]) | (unsigned(data[1]) << 8) | (unsigned(data[2]) << 16) | (unsigned(data[3]) << 24);
}
static void AviHeaderPutWord(uint8_t *data, unsigned value, unsigned bytes = 4) {
    for (unsigned i = 0; i < bytes; ++i) data[i] = uint8_t(value >> (i * 8));
}
static unsigned AviHeaderFind(const AviReaderTestData &a, const char *tag) {
    for (unsigned i = 0; i + 4 <= a.Size; ++i) if (!std::memcmp(a.Data + i, tag, 4)) return i;
    return 0;
}

// Change only the manual RIFF header; classic index offsets remain movi-relative.
static void ResizeAviStreamHeader(AviReaderTestData &a, unsigned size) {
    const unsigned at = AviHeaderFind(a, "strh"), rounded = (size + 1) & ~1u;
    const int delta = int(rounded) - 56;
    std::memmove(a.Data + at + 8 + rounded, a.Data + at + 64, a.Size - at - 64);
    if (rounded > 56) std::memset(a.Data + at + 64, 0, rounded - 56);
    if (size & 1) a.Data[at + 8 + size] = 0xcd;
    a.Word(at + 4, size);
    for (unsigned i = 0; i + 12 <= at; ++i)
        if (!std::memcmp(a.Data + i, "LIST", 4) &&
            (!std::memcmp(a.Data + i + 8, "hdrl", 4) || !std::memcmp(a.Data + i + 8, "strl", 4)))
            a.Word(i + 4, unsigned(int(AviHeaderReadWord(a.Data + i + 4)) + delta));
    a.Size = unsigned(int(a.Size) + delta); a.Movi = unsigned(int(a.Movi) + delta);
    if (a.Record) a.Record = unsigned(int(a.Record) + delta);
    for (unsigned i = 0; i < a.Entries; ++i) a.Index[i].Offset = unsigned(int(a.Index[i].Offset) + delta);
    a.Word(4, a.Size - 8);
}

static void PrependAviAudioStream(AviReaderTestData &a, unsigned size) {
    uint8_t audio[128] = {};
    const unsigned total = size + 44, strf = size + 20;
    std::memcpy(audio, "LIST", 4); AviHeaderPutWord(audio + 4, total - 8); std::memcpy(audio + 8, "strlstrh", 8);
    AviHeaderPutWord(audio + 16, size); std::memcpy(audio + 20, "auds", 4);
    AviHeaderPutWord(audio + 40, 1); AviHeaderPutWord(audio + 44, 8000);
    if (size >= 48) AviHeaderPutWord(audio + 64, 2);
    std::memcpy(audio + strf, "strf", 4); AviHeaderPutWord(audio + strf + 4, 16);
    AviHeaderPutWord(audio + strf + 8, 1, 2); AviHeaderPutWord(audio + strf + 10, 1, 2);
    AviHeaderPutWord(audio + strf + 12, 8000); AviHeaderPutWord(audio + strf + 16, 16000);
    AviHeaderPutWord(audio + strf + 20, 2, 2); AviHeaderPutWord(audio + strf + 22, 16, 2);
    const unsigned at = AviHeaderFind(a, "strl") - 8, hdrl = AviHeaderFind(a, "hdrl") - 8;
    std::memmove(a.Data + at + total, a.Data + at, a.Size - at); std::memcpy(a.Data + at, audio, total);
    a.Size += total; a.Movi += total; a.StreamCountAt += total;
    if (a.Record) a.Record += total;
    a.Word(hdrl + 4, AviHeaderReadWord(a.Data + hdrl + 4) + total);
    a.Word(AviHeaderFind(a, "avih") + 32, 2); a.Word(4, a.Size - 8);
    const unsigned idx = AviHeaderFind(a, "idx1");
    for (unsigned i = 0; i < a.Entries; ++i) {
        a.Index[i].Offset += total; a.Index[i].Id[1] = '1'; a.Data[a.Index[i].Offset + 1] = '1';
        if (idx) a.Data[idx + 8 + i * 16 + 1] = '1';
    }
}

static void MakeAviStreamHeader(AviReaderTestData &a, unsigned size = 36,
                                bool indexed = true, bool records = false) {
    a.Start(24, 0, 2, 2, records);
    const uint8_t first[] = {0,0,255,0,255,0,0,0,255,0,0,255,255,255,0,0};
    const uint8_t last[] = {255,0,0,255,255,255,0,0,0,0,255,0,255,0,0,0};
    a.Frame(first, sizeof(first)); a.Frame(nullptr, 0); a.Frame(last, sizeof(last)); a.Finish(indexed);
    ResizeAviStreamHeader(a, size);
}
#endif
