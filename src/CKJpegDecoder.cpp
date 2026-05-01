#include "CKJpegDecoder.h"

#include "XArray.h"

#include <climits>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"

void CKJpegEncoderWriteCallback(void *context, void *data, int size) {
    if (!context || !data || size <= 0)
        return;

    XArray<CKBYTE> *buffer = static_cast<XArray<CKBYTE> *>(context);
    const int oldSize = buffer->Size();
    if (size > INT_MAX - oldSize)
        return;

    const CKBYTE *bytes = static_cast<const CKBYTE *>(data);
    buffer->Expand(size);
    memcpy(buffer->Begin() + oldSize, bytes, size);
}

CKBOOL CKJpegDecoder::DecodeGrayscalePlane(const CKBYTE *encodedData,
                                           size_t encodedSize,
                                           int expectedWidth,
                                           int expectedHeight,
                                           CKBYTE **outPlane) {
    if (outPlane)
        *outPlane = nullptr;

    if (!encodedData || encodedSize == 0 || !outPlane || expectedWidth <= 0 || expectedHeight <= 0)
        return FALSE;
    if (encodedSize > static_cast<size_t>(INT_MAX))
        return FALSE;

    int decodedWidth = 0;
    int decodedHeight = 0;
    int decodedChannels = 0;
    stbi_uc *decoded = stbi_load_from_memory(encodedData,
                                             static_cast<int>(encodedSize),
                                             &decodedWidth,
                                             &decodedHeight,
                                             &decodedChannels,
                                             1); // Force single channel output
    if (!decoded)
        return FALSE;

    if (decodedWidth != expectedWidth || decodedHeight != expectedHeight) {
        stbi_image_free(decoded);
        return FALSE;
    }

    const size_t planeSize = static_cast<size_t>(decodedWidth) * static_cast<size_t>(decodedHeight);
    CKBYTE *plane = new CKBYTE[planeSize];
    if (!plane) {
        stbi_image_free(decoded);
        return FALSE;
    }

    memcpy(plane, decoded, planeSize);
    stbi_image_free(decoded);

    *outPlane = plane;
    return TRUE;
}

CKBOOL CKJpegDecoder::EncodeGrayscalePlane(const CKBYTE *planeData,
                                           int width,
                                           int height,
                                           int quality,
                                           CKBYTE **outBuffer,
                                           int &outSize) {
    if (outBuffer)
        *outBuffer = nullptr;
    outSize = 0;

    if (!planeData || !outBuffer || width <= 0 || height <= 0)
        return FALSE;

    if (quality < 1)
        quality = 1;
    else if (quality > 100)
        quality = 100;

    XArray<CKBYTE> encodedBytes;
    const size_t initialCapacity = static_cast<size_t>(width) * static_cast<size_t>(height) / 4 + 512;
    if (initialCapacity > static_cast<size_t>(INT_MAX))
        return FALSE;
    encodedBytes.Reserve(static_cast<int>(initialCapacity));

    if (!stbi_write_jpg_to_func(CKJpegEncoderWriteCallback,
                                &encodedBytes,
                                width,
                                height,
                                1,
                                planeData,
                                quality)) {
        return FALSE;
    }

    const int encodedSize = encodedBytes.Size();
    if (encodedSize <= 0)
        return FALSE;

    CKBYTE *buffer = new CKBYTE[encodedSize];
    if (!buffer)
        return FALSE;

    memcpy(buffer, encodedBytes.Begin(), encodedSize);
    *outBuffer = buffer;
    outSize = encodedSize;
    return TRUE;
}
