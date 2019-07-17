/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2015 Pedro Côrte-Real

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

#include "decoders/DcsDecoder.h"
#include "decoders/RawDecoderException.h"           // for RawDecoderExcept...
#include "decompressors/UncompressedDecompressor.h" // for UncompressedDeco...
#include "tiff/TiffEntry.h"                         // for TiffEntry, TiffD...
#include "tiff/TiffIFD.h"                           // for TiffRootIFD
#include "tiff/TiffTag.h"                           // for TiffTag::GRAYRES...
#include <cassert>                                  // for assert
#include <memory>                                   // for unique_ptr
#include <string>                                   // for operator==, string

namespace rawspeed {

class CameraMetaData;

bool DcsDecoder::isAppropriateDecoder(const TiffRootIFD* rootIFD,
                                      const Buffer* file) {
  const auto id = rootIFD->getID();
  const std::string& make = id.make;

  // FIXME: magic

  return make == "KODAK";
}

void DcsDecoder::checkImageDimensions() {
  if (width > 3072 || height > 2048)
    ThrowRDE("Unexpected image dimensions found: (%u; %u)", width, height);
}

RawImage DcsDecoder::decodeRawInternal() {
  SimpleTiffDecoder::prepareForRawDecoding();

  TiffEntry *linearization = mRootIFD->getEntryRecursive(GRAYRESPONSECURVE);
  if (!linearization || linearization->count != 256 || linearization->type != TIFF_SHORT)
    ThrowRDE("Couldn't find the linearization table");

  assert(linearization != nullptr);
  auto table = linearization->getU16Array(256);

  RawImageCurveGuard curveHandler(&mRaw, table, uncorrectedRawValues);

  // This raw is uncompressed, 1 byte (8 bits) per pixel.

  uint8_t* data = mRaw->getData();
  uint32_t pitch = mRaw->pitch;
  const uint8_t* in = mFile->getSubView(off, c2).getData(0, width * height);
  uint32_t random = 0;
  for (uint32_t y = 0; y < height; y++) {
    auto* dest = reinterpret_cast<uint16_t*>(&data[y * pitch]);
    for (uint32_t x = 0; x < width; x++) {
      if (uncorrectedRawValues)
        dest[x] = *in;
      else
        mRaw->setWithLookUp(*in, reinterpret_cast<uint8_t*>(&dest[x]), &random);
      in++;
    }
  }

  return mRaw;
}

void DcsDecoder::decodeMetaDataInternal(const CameraMetaData* meta) {
  setMetaData(meta, "", 0);
}

} // namespace rawspeed
