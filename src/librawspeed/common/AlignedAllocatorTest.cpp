/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2017 Roman Lebedev

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; withexpected even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "common/AlignedAllocator.h"
#include "common/Common.h" // for uchar8, int64, int32, short16, uint32
#include "common/Memory.h" // for alignedAllocatorArray, alignedFree, alignedM...
#include <cstddef>         // for size_t
#include <cstdint>         // for SIZE_MAX, uintptr_t
#include <gtest/gtest.h>   // for Message, TestPartResult, TestPartResult::...
#include <memory>          // for unique_ptr
#include <string>          // for string
#include <vector>          // for vector

using namespace std;
using namespace RawSpeed;

namespace RawSpeedTest {

static constexpr const size_t alloc_alignment = 16;

template <typename T> class AlignedAllocatorTest : public testing::Test {
public:
  static constexpr const size_t alloc_cnt = 16;
  inline void TheTest(T* ptr) {
    ASSERT_TRUE(((uintptr_t)ptr % alloc_alignment) == 0);
    ptr[0] = 0;
    ptr[1] = 8;
    ptr[2] = 16;
    ptr[3] = 24;
    ptr[4] = 32;
    ptr[5] = 40;
    ptr[6] = 48;
    ptr[7] = 56;
    ptr[8] = 64;
    ptr[9] = 72;
    ptr[10] = 80;
    ptr[11] = 88;
    ptr[12] = 96;
    ptr[13] = 104;
    ptr[14] = 112;
    ptr[15] = 120;

    ASSERT_EQ((int64)ptr[0] + ptr[1] + ptr[2] + ptr[3] + ptr[4] + ptr[5] +
                  ptr[6] + ptr[7] + ptr[8] + ptr[9] + ptr[10] + ptr[11] +
                  ptr[12] + ptr[13] + ptr[14] + ptr[15],
              960UL);
  }

  vector<T, AlignedAllocator<T>> _v;
};

using Classes =
    testing::Types<int, unsigned int, char8, uchar8, short16, ushort16, int32,
                   uint32, int64, uint64, float, double>;

TYPED_TEST_CASE(AlignedAllocatorTest, Classes);

TYPED_TEST(AlignedAllocatorTest, VectorTest) {
  ASSERT_NO_THROW({
    decltype(this->_v) v;
    v.resize(this->alloc_cnt);
    this->TheTest(&(v[0]));
  });
}

} // namespace RawSpeedTest
