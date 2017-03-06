/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Roman Lebedev

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

#pragma once

#include "common/Memory.h"

namespace RawSpeed {

template <class Tp, int alignment = 16> struct AlignedAllocator {
  typedef Tp value_type;

  template <class U, int align = alignment> struct rebind {
    typedef AlignedAllocator<U, align> other;
  };

  AlignedAllocator() = default;

  template <class T, int align = alignment>
  AlignedAllocator(const AlignedAllocator<T, align>&) {}

  Tp* allocate(std::size_t n) {
    return static_cast<Tp*>(alignedMallocArray<alignment, Tp>(n));
  }

  void deallocate(Tp* p, std::size_t n) { alignedFree(p); }
};

template <class T1, int A1, class T2, int A2>
bool operator==(const AlignedAllocator<T1, A1>&,
                const AlignedAllocator<T2, A2>&) {
  return A1 == A2;
}

template <class T1, int A1, class T2, int A2>
bool operator!=(const AlignedAllocator<T1, A1>&,
                const AlignedAllocator<T2, A2>&) {
  return A1 != A2;
}

} // namespace RawSpeed
