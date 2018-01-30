/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev

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

#include "decompressors/AbstractHuffmanTable.h" // for AbstractHuffmanTable
#include "common/Common.h"                      // for uchar8
#include "decoders/RawDecoderException.h"       // for RawDecoderException
#include "io/Buffer.h"                          // for Buffer
#include <algorithm>                            // for max
#include <array>                                // for array
#include <bitset>                               // for bitset
#include <gtest/gtest.h>                        // for AssertionResult, ...
#include <utility>                              // for make_pair, pair, move
#include <utility>                              // for move
#include <vector>                               // for vector

using rawspeed::AbstractHuffmanTable;
using rawspeed::Buffer;
using rawspeed::uchar8;
using std::make_tuple;

namespace rawspeed {

bool operator!=(const AbstractHuffmanTable& lhs,
                const AbstractHuffmanTable& rhs) {
  return !(lhs == rhs);
}

::std::ostream& operator<<(::std::ostream& os,
                           const AbstractHuffmanTable::CodeSymbol s) {
  if (s.code_len == 0)
    return os;

  auto str = std::bitset<32>(s.code).to_string();

  // Skip all zero bits, until either we find the first non-zero bit, or the
  // remaining length is s.code_len
  size_t size = s.code_len;
  const auto last_set_bit = str.find_first_of('1');
  if (last_set_bit != std::string::npos)
    size = std::max(size, str.size() - last_set_bit);
  // FIXME ^ all that is glossing over the real bug.

  str = str.substr(str.size() - size);
  return os << "0b" << str;
}

bool operator==(const AbstractHuffmanTable::CodeSymbol& lhs,
                const AbstractHuffmanTable::CodeSymbol& rhs) {
  return lhs.code == rhs.code && lhs.code_len == rhs.code_len;
}

bool operator!=(const AbstractHuffmanTable::CodeSymbol& lhs,
                const AbstractHuffmanTable::CodeSymbol& rhs) {
  return !(lhs == rhs);
}

} // namespace rawspeed

namespace rawspeed_test {

TEST(AbstractHuffmanTableCodeSymbolTest, Equality) {
#define s AbstractHuffmanTable::CodeSymbol
  ASSERT_NE(s(0, 1), s(0, 0));
  ASSERT_NE(s(0, 0), s(0, 1));
  ASSERT_NE(s(1, 0), s(0, 0));
  ASSERT_NE(s(0, 0), s(1, 0));

  ASSERT_EQ(s(0, 0), s(0, 0));
  ASSERT_EQ(s(0, 1), s(0, 1));
  ASSERT_EQ(s(1, 0), s(1, 0));
  ASSERT_EQ(s(1, 1), s(1, 1));
#undef s
}

using CodeSymbolPrintDataType = std::tr1::tuple<int, int, std::string>;
class CodeSymbolPrintTest
    : public ::testing::TestWithParam<CodeSymbolPrintDataType> {
protected:
  CodeSymbolPrintTest() = default;
  virtual void SetUp() {
    auto p = GetParam();

    val = std::tr1::get<0>(p);
    len = std::tr1::get<1>(p);
    str = std::tr1::get<2>(p);
  }

  int val;
  int len;
  std::string str;
};
static const CodeSymbolPrintDataType CodeSymbolPrintData[]{
    // clang-format off
    make_tuple(0b00, 0, ""),
    make_tuple(0b00, 1, "0b0"),
    make_tuple(0b00, 2, "0b00"),
    make_tuple(0b01, 0, ""),
    make_tuple(0b01, 1, "0b1"),
    make_tuple(0b01, 2, "0b01"),
    make_tuple(0b10, 0, ""),
    make_tuple(0b10, 1, "0b10"),
    make_tuple(0b10, 2, "0b10"),
    make_tuple(0b11, 0, ""),
    make_tuple(0b11, 1, "0b11"),
    make_tuple(0b11, 2, "0b11"),
    // clang-format on
};
INSTANTIATE_TEST_CASE_P(CodeSymbolPrintTest, CodeSymbolPrintTest,
                        ::testing::ValuesIn(CodeSymbolPrintData));
TEST_P(CodeSymbolPrintTest, CodeSymbolPrintTest) {
  ASSERT_EQ(
      ::testing::PrintToString(AbstractHuffmanTable::CodeSymbol(val, len)),
      str);
}

auto genHT = [](std::initializer_list<uchar8>&& nCodesPerLength)
    -> AbstractHuffmanTable {
  AbstractHuffmanTable ht;
  std::vector<uchar8> v(nCodesPerLength.begin(), nCodesPerLength.end());
  v.resize(16);
  Buffer b(v.data(), v.size());
  ht.setNCodesPerLength(b);

  return ht;
};

auto genHTCount =
    [](std::initializer_list<uchar8>&& nCodesPerLength) -> rawspeed::uint32 {
  AbstractHuffmanTable ht;
  std::vector<uchar8> v(nCodesPerLength.begin(), nCodesPerLength.end());
  v.resize(16);
  Buffer b(v.data(), v.size());
  return ht.setNCodesPerLength(b);
};

auto genHTFull =
    [](std::initializer_list<uchar8>&& nCodesPerLength,
       std::initializer_list<uchar8>&& codeValues) -> AbstractHuffmanTable {
  auto ht = genHT(std::move(nCodesPerLength));
  std::vector<uchar8> v(codeValues.begin(), codeValues.end());
  Buffer b(v.data(), v.size());
  ht.setCodeValues(b);
  return ht;
};

#ifndef NDEBUG
TEST(AbstractHuffmanTableDeathTest, setNCodesPerLengthRequires16Lengths) {
  for (int i = 0; i < 32; i++) {
    std::vector<uchar8> v(i, 1);
    ASSERT_EQ(v.size(), i);

    Buffer b(v.data(), v.size());
    ASSERT_EQ(b.getSize(), v.size());

    AbstractHuffmanTable ht;

    if (b.getSize() != 16) {
      ASSERT_DEATH({ ht.setNCodesPerLength(b); },
                   "setNCodesPerLength.*data.getSize\\(\\) == 16");
    } else {
      ASSERT_EXIT(
          {
            ht.setNCodesPerLength(b);

            exit(0);
          },
          ::testing::ExitedWithCode(0), "");
    }
  }
}
#endif

TEST(AbstractHuffmanTableTest, setNCodesPerLengthEqualCompareAndTrimming) {
  {
    AbstractHuffmanTable a;
    AbstractHuffmanTable b;

    ASSERT_EQ(a, b);
  }

  ASSERT_EQ(genHT({1}), genHT({1}));
  ASSERT_EQ(genHT({1}), genHT({1, 0}));
  ASSERT_EQ(genHT({1, 0}), genHT({1}));
  ASSERT_EQ(genHT({1, 0}), genHT({1, 0}));
  ASSERT_EQ(genHT({0, 1}), genHT({0, 1}));
  ASSERT_EQ(genHT({1, 1}), genHT({1, 1}));

  ASSERT_NE(genHT({1, 0}), genHT({1, 1}));
  ASSERT_NE(genHT({0, 1}), genHT({1}));
  ASSERT_NE(genHT({0, 1}), genHT({1, 0}));
  ASSERT_NE(genHT({0, 1}), genHT({1, 1}));
  ASSERT_NE(genHT({1}), genHT({1, 1}));
}

TEST(AbstractHuffmanTableTest, setNCodesPerLengthEmptyIsBad) {
  ASSERT_THROW(genHT({}), rawspeed::RawDecoderException);
  ASSERT_THROW(genHT({0}), rawspeed::RawDecoderException);
  ASSERT_THROW(genHT({0, 0}), rawspeed::RawDecoderException);
}

TEST(AbstractHuffmanTableTest, setNCodesPerLengthTooManyCodesTotal) {
  ASSERT_NO_THROW(genHT({0, 0, 0, 0, 0, 0, 0, 162}));
  ASSERT_THROW(genHT({0, 0, 0, 0, 0, 0, 0, 163}),
               rawspeed::RawDecoderException);
}

TEST(AbstractHuffmanTableTest, setNCodesPerLengthTooManyCodesForLenght) {
  for (int len = 1; len < 8; len++) {
    AbstractHuffmanTable ht;
    std::vector<uchar8> v(16, 0);
    Buffer b(v.data(), v.size());
    for (auto i = 1U; i <= (1U << len) - 1U; i++) {
      v[len - 1] = i;
      ASSERT_NO_THROW(ht.setNCodesPerLength(b););
    }
    v[len - 1]++;
    ASSERT_THROW(ht.setNCodesPerLength(b), rawspeed::RawDecoderException);
  }
}

TEST(AbstractHuffmanTableTest, setNCodesPerLengthCounts) {
  ASSERT_EQ(genHTCount({1}), 1);
  ASSERT_EQ(genHTCount({1, 0}), 1);
  ASSERT_EQ(genHTCount({0, 1}), 1);
  ASSERT_EQ(genHTCount({0, 2}), 2);
  ASSERT_EQ(genHTCount({0, 3}), 3);
  ASSERT_EQ(genHTCount({1, 1}), 2);
  ASSERT_EQ(genHTCount({1, 2}), 3);
  ASSERT_EQ(genHTCount({1, 3}), 4);
}

#ifndef NDEBUG
TEST(AbstractHuffmanTableDeathTest, setCodeValuesRequiresCount) {
  for (int len = 1; len < 8; len++) {
    AbstractHuffmanTable ht;
    std::vector<uchar8> l(16, 0);
    Buffer bl(l.data(), l.size());
    l[len - 1] = (1U << len) - 1U;
    const auto count = ht.setNCodesPerLength(bl);
    std::vector<uchar8> v;
    v.reserve(count + 1);
    for (auto cnt = count - 1; cnt <= count + 1; cnt++) {
      v.resize(cnt);
      Buffer bv(v.data(), v.size());
      if (cnt != count) {
        ASSERT_DEATH(
            { ht.setCodeValues(bv); },
            "setCodeValues\\(.*\\).*data.getSize\\(\\) == maxCodesCount\\(\\)");
      } else {
        ASSERT_EXIT(
            {
              ht.setCodeValues(bv);
              exit(0);
            },
            ::testing::ExitedWithCode(0), "");
      }
    }
  }
}

TEST(AbstractHuffmanTableDeathTest, setCodeValuesRequiresLessThan162) {
  auto ht = genHT({0, 0, 0, 0, 0, 0, 0, 162});
  std::vector<uchar8> v(163, 0);
  Buffer bv(v.data(), v.size());
  ASSERT_DEATH({ ht.setCodeValues(bv); },
               "setCodeValues\\(.*\\).*data.getSize\\(\\) <= 162");
}
#endif

TEST(AbstractHuffmanTableTest, setCodeValuesValueLessThan16) {
  auto ht = genHT({1});
  std::vector<uchar8> v(1);

  for (int i = 0; i < 256; i++) {
    v[0] = i;
    Buffer b(v.data(), v.size());
    if (i <= 16)
      ASSERT_NO_THROW(ht.setCodeValues(b););
    else
      ASSERT_THROW(ht.setCodeValues(b), rawspeed::RawDecoderException);
  }
}

TEST(AbstractHuffmanTableTest, EqualCompareAndTrimming) {
  ASSERT_EQ(genHTFull({1}, {0}), genHTFull({1}, {0}));
  ASSERT_EQ(genHTFull({1}, {1}), genHTFull({1}, {1}));

  ASSERT_EQ(genHTFull({1}, {0}), genHTFull({1, 0}, {0}));
  ASSERT_EQ(genHTFull({1, 0}, {0}), genHTFull({1, 0}, {0}));
  ASSERT_EQ(genHTFull({1, 0}, {0}), genHTFull({1}, {0}));

  ASSERT_NE(genHTFull({1}, {0}), genHTFull({1}, {1}));
  ASSERT_NE(genHTFull({1}, {1}), genHTFull({1}, {0}));

  ASSERT_NE(genHTFull({1}, {0}), genHTFull({1, 0}, {1}));
  ASSERT_NE(genHTFull({1, 0}, {0}), genHTFull({1, 0}, {1}));
  ASSERT_NE(genHTFull({1, 0}, {0}), genHTFull({1}, {1}));
}

using SignExtendDataType =
    std::tr1::tuple<rawspeed::uint32, rawspeed::uint32, int>;
class SignExtendTest : public ::testing::TestWithParam<SignExtendDataType> {
protected:
  SignExtendTest() = default;
  virtual void SetUp() {
    auto p = GetParam();

    diff = std::tr1::get<0>(p);
    len = std::tr1::get<1>(p);
    value = std::tr1::get<2>(p);
  }

  rawspeed::uint32 diff;
  rawspeed::uint32 len;
  int value;
};

auto zeroDiff = [](int len) { return make_tuple(0, len, -((1 << len) - 1)); };
auto passthrough = [](int len) {
  return make_tuple(((1 << len) - 1), len, ((1 << len) - 1));
};
auto one = [](int len) { return make_tuple((1 << len), len, 1); };
static const SignExtendDataType signExtendData[]{
    // clang-format off
    zeroDiff(1),
    zeroDiff(2),
    zeroDiff(3),
    zeroDiff(4),
    zeroDiff(5),
    zeroDiff(6),
    zeroDiff(7),
    zeroDiff(8),
    zeroDiff(9),
    zeroDiff(10),
    zeroDiff(11),
    zeroDiff(12),
    zeroDiff(13),
    zeroDiff(14),
    zeroDiff(15),
    zeroDiff(16),

    passthrough(1),
    passthrough(2),
    passthrough(3),
    passthrough(4),
    passthrough(5),
    passthrough(6),
    passthrough(7),
    passthrough(8),
    passthrough(9),
    passthrough(10),
    passthrough(11),
    passthrough(12),
    passthrough(13),
    passthrough(14),
    passthrough(15),
    passthrough(16),

    one(1),
    one(2),
    one(3),
    one(4),
    one(5),
    one(6),
    one(7),
    one(8),
    one(9),
    one(10),
    one(11),
    one(12),
    one(13),
    one(14),
    one(15),
    one(16),

    make_tuple(0b00, 0b01, -0b001),
    make_tuple(0b01, 0b01,  0b001),
    make_tuple(0b10, 0b01,  0b001),
    make_tuple(0b11, 0b01,  0b011),
    make_tuple(0b00, 0b10, -0b011),
    make_tuple(0b01, 0b10, -0b010),
    make_tuple(0b10, 0b10,  0b010),
    make_tuple(0b11, 0b10,  0b011),
    make_tuple(0b00, 0b11, -0b111),
    make_tuple(0b01, 0b11, -0b110),
    make_tuple(0b10, 0b11, -0b101),
    make_tuple(0b11, 0b11, -0b100),
    // clang-format on
};
INSTANTIATE_TEST_CASE_P(SignExtendTest, SignExtendTest,
                        ::testing::ValuesIn(signExtendData));
TEST_P(SignExtendTest, SignExtendTest) {
  ASSERT_EQ(AbstractHuffmanTable::signExtended(diff, len), value);
}

class DummyHuffmanTableTest : protected AbstractHuffmanTable,
                              public ::testing::Test {};

TEST_F(DummyHuffmanTableTest, CodeSymbolDoesNotOverflow) {
  std::vector<uchar8> v({1, 3, 7, 15, 31, 63, 1, 1, 1, 1, 1, 1, 1, 1});
  v.resize(16);
  Buffer bl(v.data(), v.size());
  const auto cnt = setNCodesPerLength(bl);
  std::vector<uchar8> cv(cnt, 0);
  Buffer bv(cv.data(), cv.size());
  setCodeValues(bv);

  // The code value would overflow u16
  ASSERT_THROW(generateCodeSymbols(), rawspeed::RawDecoderException);
}

using generateCodeSymbolsDataType =
    std::tr1::tuple<std::vector<uchar8>,
                    std::vector<AbstractHuffmanTable::CodeSymbol>>;
class generateCodeSymbolsTest
    : protected AbstractHuffmanTable,
      public ::testing::TestWithParam<generateCodeSymbolsDataType> {
protected:
  generateCodeSymbolsTest() = default;
  virtual void SetUp() {
    auto p = GetParam();

    ncpl = std::tr1::get<0>(p);
    ncpl.resize(16);
    expectedSymbols = std::tr1::get<1>(p);
  }

  std::vector<uchar8> ncpl;
  std::vector<AbstractHuffmanTable::CodeSymbol> expectedSymbols;
};
static const generateCodeSymbolsDataType generateCodeSymbolsData[]{
    make_tuple(std::vector<rawspeed::uchar8>{1},
               std::vector<AbstractHuffmanTable::CodeSymbol>{{0b0, 1}}),

    make_tuple(std::vector<rawspeed::uchar8>{0, 1},
               std::vector<AbstractHuffmanTable::CodeSymbol>{
                   {0b00, 2},
               }),
    make_tuple(std::vector<rawspeed::uchar8>{0, 2},
               std::vector<AbstractHuffmanTable::CodeSymbol>{
                   {0b00, 2},
                   {0b01, 2},
               }),
    make_tuple(std::vector<rawspeed::uchar8>{0, 3},
               std::vector<AbstractHuffmanTable::CodeSymbol>{
                   {0b00, 2},
                   {0b01, 2},
                   {0b10, 2},
               }),

    make_tuple(std::vector<rawspeed::uchar8>{1, 1},
               std::vector<AbstractHuffmanTable::CodeSymbol>{
                   {0b0, 1},
                   {0b10, 2},
               }),
    make_tuple(std::vector<rawspeed::uchar8>{1, 2},
               std::vector<AbstractHuffmanTable::CodeSymbol>{
                   {0b0, 1},
                   {0b10, 2},
                   {0b11, 2},
               }),
    make_tuple(
        std::vector<rawspeed::uchar8>{1, 3},
        std::vector<AbstractHuffmanTable::CodeSymbol>{
            {0b0, 1}, {0b10, 2}, {0b11, 2}, {0b100, 2}, // FIXME <- BROKEN!
        }),

};
INSTANTIATE_TEST_CASE_P(generateCodeSymbolsTest, generateCodeSymbolsTest,
                        ::testing::ValuesIn(generateCodeSymbolsData));
TEST_P(generateCodeSymbolsTest, generateCodeSymbolsTest) {
  Buffer bl(ncpl.data(), ncpl.size());
  const auto cnt = setNCodesPerLength(bl);
  std::vector<uchar8> cv(cnt, 0);
  Buffer bv(cv.data(), cv.size());
  setCodeValues(bv);

  ASSERT_EQ(generateCodeSymbols(), expectedSymbols);
}

} // namespace rawspeed_test