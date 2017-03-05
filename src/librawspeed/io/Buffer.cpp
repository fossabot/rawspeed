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

#include "rawspeedconfig.h" // for HAVE_MPROTECT
#include "io/Buffer.h"
#include "common/Common.h"  // for uchar8
#include "common/Memory.h"  // for alignedFree
#include "io/IOException.h" // for IOException (ptr only), ThrowIOE
#include <memory>           // for unique_ptr

#if defined(DEBUG) && defined(HAVE_MPROTECT)
#include <sys/mman.h> // for mprotect, PROT_READ
#include <unistd.h>   // for sysconf, _SC_PAGE_SIZE
#endif

using std::unique_ptr;

namespace RawSpeed {

unique_ptr<uchar8, decltype(&alignedFree)> Buffer::Create(size_type size) {
  if (!size)
    ThrowIOE("Trying to allocate 0 bytes sized buffer.");

#if !(defined(DEBUG) && defined(HAVE_MPROTECT))
  unique_ptr<uchar8, decltype(&alignedFree)> data(
      (uchar8*)alignedMalloc<16>(roundUp(size + BUFFER_PADDING, 16)),
      alignedFree);
#else
  static const long int pagesize = sysconf(_SC_PAGE_SIZE);
  if (pagesize < 1)
    ThrowIOE("Unknown pagesize: %li", pagesize);

  unique_ptr<uchar8, decltype(&alignedFree)> data(
      (uchar8*)alignedMalloc(roundUp(size + BUFFER_PADDING, pagesize),
                             pagesize),
      alignedFree);
#endif

  if (!data.get())
    ThrowIOE("Failed to allocate %uz bytes memory buffer.", size);

  return data;
}

Buffer::Buffer(unique_ptr<uchar8, decltype(&alignedFree)> data_,
               size_type size_)
    : size(size_) {
  if (!size)
    ThrowIOE("Buffer has zero size?");

  if (data_.get_deleter() != alignedFree)
    ThrowIOE("Wrong deleter. Expected RawSpeed::alignedFree()");

  data = data_.release();
  if (!data)
    ThrowIOE("Memory buffer is nonexistant");

  isOwner = true;

#if defined(DEBUG) && defined(HAVE_MPROTECT)
  // owning buffer is strictly read-only.
  if (mprotect(static_cast<void*>(const_cast<uchar8*>(data)), size,
               PROT_READ) == -1)
    ThrowIOE("Failed to set read-only protection on a buffer.");
#endif
}

Buffer::Buffer(size_type size_) : Buffer(Create(size_), size_) {}

Buffer::~Buffer() {
  if (isOwner) {
#if defined(DEBUG) && defined(HAVE_MPROTECT)
    /*if (*/ mprotect(static_cast<void*>(const_cast<uchar8*>(data)), size,
                      PROT_READ | PROT_WRITE) /* == -1)
      ThrowIOE("Failed to release read-only protection of a buffer.")*/;
#endif

    alignedFree(const_cast<uchar8*>(data));
  }
}

Buffer& Buffer::operator=(const Buffer &rhs)
{
  this->~Buffer();
  data = rhs.data;
  size = rhs.size;
  isOwner = false;
  return *this;
}

#if 0
Buffer* Buffer::clone() {
  Buffer *new_map = new Buffer(size);
  memcpy(new_map->data, data, size);
  return new_map;
}

Buffer* Buffer::cloneRandomSize() {
  uint32 new_size = (rand() | (rand() << 15)) % size;
  Buffer *new_map = new Buffer(new_size);
  memcpy(new_map->data, data, new_size);
  return new_map;
}

void Buffer::corrupt(int errors) {
  for (int i = 0; i < errors; i++) {
    uint32 pos = (rand() | (rand() << 15)) % size;
    data[pos] = rand() & 0xff;
  }
}
#endif

} // namespace RawSpeed
