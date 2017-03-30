/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Axel Waggershauser

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "decompressors/Cr2Decompressor.h"
#include "common/Common.h"                // for unroll_loop, uint32, ushort16
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImageData
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitPumpJPEG.h"               // for BitStream<>::getBufferPosi...
#include "io/ByteStream.h"                // for ByteStream
#include <algorithm>                      // for min, copy_n, move
#include <cassert>                        // for assert
#include <numeric>                        // for accumulate

using namespace std;

namespace RawSpeed {

void Cr2Decompressor::decodeScan()
{
  if (predictorMode != 1)
    ThrowRDE("Unsupported predictor mode.");

  if (slicesWidths.empty())
    slicesWidths.push_back(frame.w * frame.cps);

  bool isSubSampled = false;
  for (uint32 i = 0; i < frame.cps;  i++)
    isSubSampled |= frame.compInfo[i].superH != 1 || frame.compInfo[i].superV != 1;

  if (isSubSampled) {
    if (mRaw->isCFA)
      ThrowRDE("Cannot decode subsampled image to CFA data");

    if (mRaw->getCpp() != frame.cps)
      ThrowRDE("Subsampled component count does not match image.");

    if (frame.cps != 3 || frame.compInfo[0].superH != 2 ||
        (frame.compInfo[0].superV != 2 && frame.compInfo[0].superV != 1) ||
        frame.compInfo[1].superH != 1 || frame.compInfo[1].superV != 1 ||
        frame.compInfo[2].superH != 1 || frame.compInfo[2].superV != 1)
      ThrowRDE("Unsupported subsampling");

    if (frame.compInfo[0].superV == 2)
      decodeN_X_Y<3, 2, 2>(); // Cr2 sRaw1/mRaw
    else {
      assert(frame.compInfo[0].superV == 1);
      decodeN_X_Y<3, 2, 1>(); // Cr2 sRaw2/sRaw
    }
  } else {
    switch (frame.cps) {
    case 2:
      decodeN_X_Y<2, 1, 1>();
      break;
    case 4:
      decodeN_X_Y<4, 1, 1>();
      break;
    default:
      ThrowRDE("Unsupported number of components: %u", frame.cps);
    }
  }
}

void Cr2Decompressor::decode(std::vector<int> slicesWidths_)
{
  slicesWidths = move(slicesWidths_);
  AbstractLJpegDecompressor::decode();
}

// N_COMP == number of components (2, 3 or 4)
// X_S_F  == x/horizontal sampling factor (1 or 2)
// Y_S_F  == y/vertical   sampling factor (1 or 2)

template <int N_COMP, int X_S_F, int Y_S_F>
void Cr2Decompressor::decodeN_X_Y()
{
  BitPumpJPEG bitStream(input);

  if (frame.cps != 3 && frame.w * frame.cps > 2 * frame.h) {
    // Fix Canon double height issue where Canon doubled the width and halfed
    // the height (e.g. with 5Ds), ask Canon. frame.w needs to stay as is here
    // because the number of pixels after which the predictor gets updated is
    // still the doubled width.
    // see: FIX_CANON_HALF_HEIGHT_DOUBLE_WIDTH
    frame.h *= 2;
  }

  if (X_S_F == 2 && Y_S_F == 1)
  {
    // fix the inconsistent slice width in sRaw mode, ask Canon.
    for (auto& sliceWidth : slicesWidths)
      sliceWidth = sliceWidth * 3 / 2;
  }

  // what is the total width of all the slices?
  const auto fullWidth =
      accumulate(slicesWidths.begin(), slicesWidths.end(), 0u);
  assert(fullWidth > 0);
  assert(fullWidth > frame.w);
  // however, fullWidth is not guaranteed to be multiple of frame.w

  // what is the total count of pixels in all the slices?
  const auto fullArea = fullWidth * frame.h;
  assert(fullArea > 0);

  // make sure that it a multiple of frame.w
  const auto adjustedFullArea = roundUp(fullArea, frame.w);
  assert(isAligned(adjustedFullArea, frame.w));
  assert(adjustedFullArea >= fullArea);

  // so if we want each line to be of frame.w size, how much lines total?
  const auto adjustedHeight = adjustedFullArea / frame.w;
  assert(adjustedHeight > frame.h);

  // each row has it's own predictor
  // (first column needs to be predicted sequentially)
  iPoint2D slicedDims(frame.w, adjustedHeight);

  // will need tmp image because will first decode without unslicing.
  RawImage sRaw = RawImage::create(slicedDims, TYPE_USHORT16, frame.cps);

  uint32 inPixelPitch = sRaw->pitch / 2;  // Pitch in pixel
  uint32 outPixelPitch = mRaw->pitch / 2; // Pitch in pixel

  // To understand the CR2 slice handling and sampling factor behavior, see
  // https://github.com/lclevy/libcraw2/blob/master/docs/cr2_lossless.pdf?raw=true

  // inner loop decodes one group of pixels at a time
  //  * for <N,1,1>: N  = N*1*1 (full raw)
  //  * for <3,2,1>: 6  = 3*2*1
  //  * for <3,2,2>: 12 = 3*2*2
  // and advances x by N_COMP*X_S_F and y by Y_S_F
  constexpr int xStepSize = N_COMP * X_S_F;
  constexpr int yStepSize = Y_S_F;

  // STEP ONE: decode. this must be done fully sequentially, because HT.

  auto ht = getHuffmanTables<N_COMP>();
  unsigned processedPixels = 0;

  for (unsigned y = 0; y < adjustedHeight; y += yStepSize) {
    auto src = (ushort16*)sRaw->getDataUncropped(0, y);

    // last row may be larger than the data size, see roundUp() above
    for (unsigned x = 0; x < frame.w && processedPixels < fullArea;
         x += xStepSize, processedPixels += xStepSize, src += xStepSize) {
      if (X_S_F == 1) { // will be optimized out
        unroll_loop<N_COMP>(
            [&](int i) { src[i] = ht[i]->decodeNext(bitStream); });
      } else {
        unroll_loop<Y_S_F>([&](int i) {
          src[0 + i * inPixelPitch] = ht[0]->decodeNext(bitStream);
          src[3 + i * inPixelPitch] = ht[0]->decodeNext(bitStream);
        });

        src[1] = ht[1]->decodeNext(bitStream);
        src[2] = ht[2]->decodeNext(bitStream);
      }
    }
  }
  input.skipBytes(bitStream.getBufferPosition());

  // STEP TWO: bootstrap predicting by sequentially predicting the first column.

  auto pred = getInitialPredictors<N_COMP>();

  for (unsigned y = 0; y < adjustedHeight; y += yStepSize) {
    auto src = (ushort16*)sRaw->getDataUncropped(0, y);

    if (X_S_F == 1) { // will be optimized out
      unroll_loop<N_COMP>([&](int i) {
        src[i] = pred[i] += src[i];
      });
    } else {
      unroll_loop<Y_S_F>([&](int i) {
        src[0 + i * inPixelPitch] = pred[0] += src[0 + i * inPixelPitch];
        src[3 + i * inPixelPitch] = pred[0] += src[3 + i * inPixelPitch];
      });

      src[1] = pred[1] += src[1];
      src[2] = pred[2] += src[2];
    }
  }

  // STEP THREE: predict!

  for (unsigned y = 0; y < adjustedHeight; y += yStepSize) {
    auto src = (ushort16*)sRaw->getDataUncropped(0, y);

    // first pixel is already predicted, so setup predictor with that data
    auto lPred = getInitialPredictors<N_COMP>();
    copy_n(src, N_COMP, lPred.data());

    // careful not to re-predict that first pixel over again!
    unsigned x = xStepSize;
    src += xStepSize;

    // and now start prediction from the second pixel onwards...
    for (; x < frame.w; x += xStepSize, src += xStepSize) {
      if (X_S_F == 1) { // will be optimized out
        unroll_loop<N_COMP>([&](int i) {
          src[i] = lPred[i] += src[i];
        });
      } else {
        unroll_loop<Y_S_F>([&](int i) {
          src[0 + i * inPixelPitch] = lPred[0] += src[0 + i * inPixelPitch];
          src[3 + i * inPixelPitch] = lPred[0] += src[3 + i * inPixelPitch];
        });

        src[1] = lPred[1] += src[1];
        src[2] = lPred[2] += src[2];
      }
    }
  }

  // STEP FOUR: unslice...

  unsigned processedLineSlices = 0;

  for (unsigned sliceWidth : slicesWidths) {
    for (unsigned y = 0; y < adjustedHeight;
         y += yStepSize, processedLineSlices += yStepSize) {
      unsigned srcX =
          processedLineSlices / frame.h * slicesWidths[0] / sRaw->getCpp();
      if (srcX >= (unsigned)sRaw->dim.x)
        break;
      auto src = (ushort16*)sRaw->getDataUncropped(srcX, y);

      // Fix for Canon 80D mraw format.
      // In that format, `frame` is 4032x3402, while `mRaw` is 4536x3024.
      // Consequently, the slices in `frame` wrap around plus there are few
      // 'extra' sliced lines because sum(slicesW) * sliceH > mRaw->dim.area()
      // Those would overflow, hence the break.
      // see FIX_CANON_FRAME_VS_IMAGE_SIZE_MISMATCH
      unsigned destY = processedLineSlices % mRaw->dim.y;
      unsigned destX =
          processedLineSlices / mRaw->dim.y * slicesWidths[0] / mRaw->getCpp();
      if (destX >= (unsigned)mRaw->dim.x)
        break;
      auto dest = (ushort16*)mRaw->getDataUncropped(destX, destY);

      for (unsigned x = 0; x < sliceWidth;
           x += xStepSize, src += xStepSize, dest += xStepSize) {

        if (X_S_F == 1) { // will be optimized out
          unroll_loop<N_COMP>([&](int i) {
            dest[i] = src[i];
          });
        } else {
          unroll_loop<Y_S_F>([&](int i) {
            dest[0 + i * outPixelPitch] = src[0 + i * inPixelPitch];
            dest[3 + i * outPixelPitch] = src[3 + i * inPixelPitch];
          });

          dest[1] = src[1];
          dest[2] = src[2];
        }
      }
    }
  }
}

} // namespace RawSpeed
