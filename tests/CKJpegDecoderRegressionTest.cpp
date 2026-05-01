#include <gtest/gtest.h>

#include <climits>

#include "CKJpegDecoder.h"

namespace {

void DeleteByteBuffer(CKBYTE *buffer) {
    delete[] buffer;
}

int AbsoluteByteDifference(CKBYTE lhs, CKBYTE rhs) {
    const int difference = static_cast<int>(lhs) - static_cast<int>(rhs);
    return difference < 0 ? -difference : difference;
}

} // namespace

TEST(CKJpegDecoderRegressionTest, EncodeClampsQualityAndDecodePreservesPlaneShape) {
    const int width = 8;
    const int height = 8;
    CKBYTE plane[width * height];

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            plane[y * width + x] = static_cast<CKBYTE>(x * 24 + y * 3);
        }
    }

    CKBYTE *encoded = nullptr;
    int encodedSize = 0;
    ASSERT_TRUE(CKJpegDecoder::EncodeGrayscalePlane(plane, width, height, 500, &encoded, encodedSize));
    ASSERT_NE(nullptr, encoded);
    ASSERT_GT(encodedSize, 0);

    CKBYTE *decoded = nullptr;
    ASSERT_TRUE(CKJpegDecoder::DecodeGrayscalePlane(encoded,
                                                    static_cast<size_t>(encodedSize),
                                                    width,
                                                    height,
                                                    &decoded));
    ASSERT_NE(nullptr, decoded);

    for (int i = 0; i < width * height; ++i) {
        EXPECT_LE(AbsoluteByteDifference(plane[i], decoded[i]), 24);
    }

    DeleteByteBuffer(decoded);
    DeleteByteBuffer(encoded);
}

TEST(CKJpegDecoderRegressionTest, DecodeRejectsUnexpectedDimensionsAndClearsOutput) {
    const int width = 4;
    const int height = 4;
    CKBYTE plane[width * height];

    for (int i = 0; i < width * height; ++i) {
        plane[i] = static_cast<CKBYTE>(i * 11);
    }

    CKBYTE *encoded = nullptr;
    int encodedSize = 0;
    ASSERT_TRUE(CKJpegDecoder::EncodeGrayscalePlane(plane, width, height, 85, &encoded, encodedSize));
    ASSERT_NE(nullptr, encoded);

    CKBYTE sentinel = 0;
    CKBYTE *decoded = &sentinel;
    EXPECT_FALSE(CKJpegDecoder::DecodeGrayscalePlane(encoded,
                                                     static_cast<size_t>(encodedSize),
                                                     width + 1,
                                                     height,
                                                     &decoded));
    EXPECT_EQ(nullptr, decoded);

    DeleteByteBuffer(encoded);
}

TEST(CKJpegDecoderRegressionTest, EncodeRejectsInvalidInputAndClearsOutputs) {
    CKBYTE sentinel = 0;
    CKBYTE *encoded = &sentinel;
    int encodedSize = 123;

    EXPECT_FALSE(CKJpegDecoder::EncodeGrayscalePlane(nullptr, 4, 4, 85, &encoded, encodedSize));
    EXPECT_EQ(nullptr, encoded);
    EXPECT_EQ(0, encodedSize);

    encoded = &sentinel;
    encodedSize = 123;
    CKBYTE plane[4] = {0, 64, 128, 255};
    EXPECT_FALSE(CKJpegDecoder::EncodeGrayscalePlane(plane, 0, 2, 85, &encoded, encodedSize));
    EXPECT_EQ(nullptr, encoded);
    EXPECT_EQ(0, encodedSize);
}
