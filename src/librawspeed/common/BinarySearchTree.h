/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2018 Roman Lebedev

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

#include <cassert>    // for assert
#include <functional> // for less, equal_to
#include <memory>     // for unique_ptr, make_unique

namespace rawspeed {

template <typename T, typename Compare = std::less<T>,
          typename EqualyCompare = std::equal_to<T>>
class BinarySearchTree final {
public:
  struct Node final {
    T value;

    std::unique_ptr<Node> left;
    std::unique_ptr<Node> right;

    Node(T value_) : value(value_) {}

    // If the value is less than the value of this node, then it should go into
    // the left sub-tree of this node. Else, to the right sub-tree.
    std::unique_ptr<Node>& direction(T value_) {
      assert(!EqualyCompare()(value_, value));
      return Compare()(value_, value) ? left : right;
    }

    static void add(std::unique_ptr<Node>* node, T value_) {
      // First, traverse/descend the tree, and find the nonexistant leaf
      while (node && *node)
        node = &((*node)->direction(value_));

      // And add this node/leaf to tree in the given position
      *node = std::make_unique<Node>(value_);
    }

    static const T* find(const std::unique_ptr<Node>* node, T value_) {
      // If there is a node
      while (node && *node) {
        // If the node's value matches what we are looking for
        if (EqualyCompare()(value_, (*node)->value))
          return &((*node)->value); // Then return the pointer to the value
        // Else, pick the subtree (a next, lower node) where it might be
        node = &((*node)->direction(value_));
      }
      // Did not find the node.
      return nullptr;
    }
  };

  std::unique_ptr<Node> root;

  void add(T value) { Node::add(&root, value); }

  const T* find(T value) const { return Node::find(&root, value); }
};

} // namespace rawspeed
