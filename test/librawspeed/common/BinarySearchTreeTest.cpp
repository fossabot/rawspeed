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

#include "common/BinarySearchTree.h" // for BinarySearchTree
#include <gtest/gtest.h> // for AssertionResult, DeathTest, Test, AssertHe...

using rawspeed::BinarySearchTree;

namespace rawspeed_test {

TEST(BinarySearchTreeTest, EmptyByDefault) {
  {
    const BinarySearchTree<int> b;
    for (auto i : {0, -1, 1}) {
      ASSERT_FALSE(b.root);
      ASSERT_EQ(b.find(i), nullptr);
    }
  }
  {
    BinarySearchTree<int> b;
    for (auto i : {0, -1, 1}) {
      ASSERT_FALSE(b.root);
      ASSERT_EQ(b.find(i), nullptr);
    }
  }
}

TEST(BinarySearchTreeTest, CanAdd) {
  BinarySearchTree<int> b;
  ASSERT_FALSE(b.root);
  b.add(0);
  ASSERT_TRUE(b.root);
  ASSERT_EQ(b.root->value, 0);
  ASSERT_FALSE(b.root->left);
  ASSERT_FALSE(b.root->right);
}

TEST(BinarySearchTreeTest, CanFindAfterAdd) {
  BinarySearchTree<int> b;
  b.add(0);
  const auto val = b.find(0);
  ASSERT_NE(val, nullptr);
  ASSERT_EQ(*val, 0);
}

TEST(BinarySearchTreeTest, CantFindNotAdded) {
  BinarySearchTree<int> b;
  b.add(0);
  ASSERT_EQ(b.find(1), nullptr);
  ASSERT_EQ(b.find(-1), nullptr);
}

#ifndef NDEBUG
TEST(BinarySearchTreeDeathTest, NoDuplicates) {
  ASSERT_DEATH(
      {
        BinarySearchTree<int> b;

        b.add(0);
        b.add(0);

        exit(0);
      },
      "!EqualyCompare");
}

TEST(BinarySearchTreeDeathTest, DirectionNoDuplicates) {
  ASSERT_DEATH(
      {
        BinarySearchTree<int> b;

        b.add(0);
        b.root->direction(0);

        exit(0);
      },
      "!EqualyCompare");
  ASSERT_DEATH(
      {
        BinarySearchTree<int> b;

        b.add(0);
        const auto& r = b.root;
        r->direction(0);

        exit(0);
      },
      "!EqualyCompare");
}
#endif

TEST(BinarySearchTreeTest, DirectionLesserLeavesToTheLeft) {
  BinarySearchTree<int> b;
  b.add(0);
  {
    auto& r = b.root;
    ASSERT_EQ(r->direction(-1), b.root->left);
  }
  {
    const auto& r = b.root;
    ASSERT_EQ(r->direction(-1), b.root->left);
  }
}

TEST(BinarySearchTreeTest, DirectionGreaterLeavesToTheRight) {
  BinarySearchTree<int> b;
  b.add(0);
  {
    auto& r = b.root;
    ASSERT_EQ(r->direction(1), b.root->right);
  }
  {
    const auto& r = b.root;
    ASSERT_EQ(r->direction(1), b.root->right);
  }
}

TEST(BinarySearchTreeTest, LesserLeavesToTheLeft) {
  BinarySearchTree<int> b;
  b.add(0);
  b.add(-1);
  ASSERT_TRUE(b.root->left);
  ASSERT_EQ(b.root->left->value, -1);
  ASSERT_FALSE(b.root->left->left);
  ASSERT_FALSE(b.root->left->right);
  ASSERT_FALSE(b.root->right);
}

TEST(BinarySearchTreeTest, GreaterLeavesToTheRight) {
  BinarySearchTree<int> b;
  b.add(0);
  b.add(1);
  ASSERT_FALSE(b.root->left);
  ASSERT_TRUE(b.root->right);
  ASSERT_EQ(b.root->right->value, 1);
  ASSERT_FALSE(b.root->right->left);
  ASSERT_FALSE(b.root->right->right);
}

TEST(BinarySearchTreeDeathTest, CanHandleManyNodes) {
  ASSERT_EXIT(
      {
        BinarySearchTree<int> b;

        // Arbitrairly-picked value. Should be large, but not too large to not
        // overly-inflate test time.
        static constexpr int Limit = 1UL << 8UL;

        for (int i = 0; i < Limit; i++) {
          // This could be implemented recursively, which could result in
          // stack overflow due to too deep recursion.
          b.add(i);
        }

        ASSERT_EQ(b.find(-1), nullptr);
        for (int i = 0; i < Limit; i++) {
          // This could be implemented recursively, which could result in
          // stack overflow due to too deep recursion.
          ASSERT_NE(b.find(i), nullptr);
        }
        ASSERT_EQ(b.find(Limit + 1), nullptr);

        exit(0);
      },
      ::testing::ExitedWithCode(0), "");
}

} // namespace rawspeed_test
