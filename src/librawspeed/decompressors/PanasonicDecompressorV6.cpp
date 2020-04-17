/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2019 LibRaw LLC (info@libraw.org)
    Copyright (C) 2020 Roman Lebedev

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "rawspeedconfig.h" // for HAVE_OPENMP
#include "decompressors/PanasonicDecompressorV6.h"
#include "common/Array2DRef.h"            // for Array2DRef
#include "common/Common.h"                // for rawspeed_get_numb...
#include "common/Point.h"                 // for iPoint2D
#include "common/RawImage.h"              // for RawImage, RawImag...
#include "decoders/RawDecoderException.h" // for ThrowRDE
#include "io/BitStream.h"                 // for BitStream, BitStr...
#include "io/ByteStream.h"                // for ByteStream
#include "io/Endianness.h"                // for getLE
#include "io/IOException.h"               // for ThrowIOE
#include <algorithm>                      // for min
#include <array>                          // for array
#include <cassert>                        // for assert
#include <cstdint>                        // for uint8_t, uint16_t
#include <cstring>                        // for memcpy

namespace rawspeed {

constexpr int PanasonicDecompressorV6::PixelsPerBlock;
constexpr int PanasonicDecompressorV6::BytesPerBlock;

class Buffer;

namespace {

constexpr unsigned PanaV6BitPumpMCU = 4U;

struct BitStreamBackwardSequentialReplenisher final : BitStreamReplenisherBase {
  explicit BitStreamBackwardSequentialReplenisher(const Buffer& input)
      : BitStreamReplenisherBase(input) {}

  inline void markNumBytesAsConsumed(size_type numBytes) { pos += numBytes; }

  inline const uint8_t* getInput() {
#if !defined(DEBUG)
    // Do we have PanaV6BitPumpMCU or more bytes left in the input buffer?
    // If so, then we can just read from said buffer.
    if (pos + PanaV6BitPumpMCU <= size)
      return data + size;
#endif

    // We have to use intermediate buffer, either because the input is running
    // out of bytes, or because we want to enforce bounds checking.

    // Note that in order to keep all fill-level invariants we must allow to
    // over-read past-the-end a bit.
    if (pos > size + PanaV6BitPumpMCU)
      ThrowIOE("Buffer overflow read in BitStream");

    tmp.fill(0);

    // How many bytes are left in input buffer?
    // Since pos can be past-the-end we need to carefully handle overflow.
    size_type bytesRemaining = (size >= pos) ? size - pos : 0;
    // And if we are not at the end of the input, we may have more than we need.
    bytesRemaining = std::min(PanaV6BitPumpMCU, bytesRemaining);

    memcpy(tmp.data(), data + pos, bytesRemaining);
    return tmp.data();
  }
};

struct PanaV6BitPumpTag;

} // namespace

using BitPumpPanaV6 = BitStream<PanaV6BitPumpTag, BitStreamCacheRightInLeftOut,
                                BitStreamBackwardSequentialReplenisher>;

template <>
inline BitPumpPanaV6::size_type BitPumpPanaV6::fillCache(const uint8_t* input) {
  static_assert(BitStreamCacheBase::MaxGetBits >= 32, "check implementation");

  cache.push(getByteSwapped<uint32_t>(input, /*bswap=*/false), 32);
  return 4;
}

PanasonicDecompressorV6::PanasonicDecompressorV6(const RawImage& img,
                                                 const ByteStream& input_)
    : mRaw(img) {
  if (mRaw->getCpp() != 1 || mRaw->getDataType() != TYPE_USHORT16 ||
      mRaw->getBpp() != sizeof(uint16_t))
    ThrowRDE("Unexpected component count / data type");

  if (!mRaw->dim.hasPositiveArea() ||
      mRaw->dim.x % PanasonicDecompressorV6::PixelsPerBlock != 0) {
    ThrowRDE("Unexpected image dimensions found: (%i; %i)", mRaw->dim.x,
             mRaw->dim.y);
  }

  // How many blocks are needed for the given image size?
  const auto numBlocks = mRaw->dim.area() / PixelsPerBlock;

  // How many full blocks does the input contain? This is truncating division.
  const auto haveBlocks = input_.getRemainSize() / BytesPerBlock;

  // Does the input contain enough blocks?
  if (haveBlocks < numBlocks)
    ThrowRDE("Insufficient count of input blocks for a given image");

  // We only want those blocks we need, no extras.
  input = input_.peekStream(numBlocks, BytesPerBlock);
}

inline void
// NOLINTNEXTLINE(bugprone-exception-escape): no exceptions will be thrown.
PanasonicDecompressorV6::decompressBlock(ByteStream* rowInput, int row,
                                         int col) const noexcept {
  const Array2DRef<uint16_t> out(mRaw->getU16DataAsUncroppedArray2DRef());

  const std::array<uint8_t, PanasonicDecompressorV6::BytesPerBlock> tmp =
      [rowInput]() {
        ByteStream bs =
            rowInput->getStream(PanasonicDecompressorV6::BytesPerBlock);
        std::array<uint8_t, PanasonicDecompressorV6::BytesPerBlock> inv;
        for (int byte = 0; byte != PanasonicDecompressorV6::BytesPerBlock;
             byte += sizeof(uint32_t)) {
          uint32_t val = bs.getU32();
          memcpy(&inv[PanasonicDecompressorV6::BytesPerBlock -
                      sizeof(uint32_t) - byte],
                 &val, sizeof(val));
        }
        return inv;
      }();

  BitPumpPanaV6 pump(Buffer(tmp.data(), 16));
  pump.fill(32);

  std::array<unsigned, 2> oddeven = {0, 0};
  std::array<unsigned, 2> nonzero = {0, 0};
  unsigned pmul = 0;
  unsigned pixel_base = 0;
  for (int pix = 0; pix < PanasonicDecompressorV6::PixelsPerBlock;
       pix++, col++) {
    if (pix % 3 == 2) {
      uint16_t base = pump.getBitsNoFill(2);
      pump.fill(32);
      if (base == 3)
        base = 4;
      pixel_base = 0x200 << base;
      pmul = 1 << base;
    }
    uint16_t epixel = pump.getBitsNoFill(pix < 2 ? 14 : 10);
    if (oddeven[pix % 2]) {
      epixel *= pmul;
      if (pixel_base < 0x2000 && nonzero[pix % 2] > pixel_base)
        epixel += nonzero[pix % 2] - pixel_base;
      nonzero[pix % 2] = epixel;
    } else {
      oddeven[pix % 2] = epixel;
      if (epixel)
        nonzero[pix % 2] = epixel;
      else
        epixel = nonzero[pix % 2];
    }
    auto spix = static_cast<unsigned>(static_cast<int>(epixel) - 0xf);
    if (spix <= 0xffff)
      out(row, col) = spix & 0xffff;
    else {
      epixel = static_cast<int>(epixel + 0x7ffffff1) >> 0x1f;
      out(row, col) = epixel & 0x3fff;
    }
  }
}

// NOLINTNEXTLINE(bugprone-exception-escape): no exceptions will be thrown.
void PanasonicDecompressorV6::decompressRow(int row) const noexcept {
  assert(mRaw->dim.x % PanasonicDecompressorV6::PixelsPerBlock == 0);
  const int blocksperrow =
      mRaw->dim.x / PanasonicDecompressorV6::PixelsPerBlock;
  const int bytesPerRow = PanasonicDecompressorV6::BytesPerBlock * blocksperrow;

  ByteStream rowInput = input.getSubStream(bytesPerRow * row, bytesPerRow);
  for (int rblock = 0, col = 0; rblock < blocksperrow;
       rblock++, col += PanasonicDecompressorV6::PixelsPerBlock)
    decompressBlock(&rowInput, row, col);
}

void PanasonicDecompressorV6::decompress() const {
#ifdef HAVE_OPENMP
#pragma omp parallel for num_threads(rawspeed_get_number_of_processor_cores()) \
    schedule(static) default(none)
#endif
  for (int row = 0; row < mRaw->dim.y;
       ++row) { // NOLINT(openmp-exception-escape): we know no exceptions will
                // be thrown.
    decompressRow(row);
  }
}

} // namespace rawspeed
