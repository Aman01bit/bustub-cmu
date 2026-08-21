//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// count_min_sketch.cpp
//
// Identification: src/primer/count_min_sketch.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "primer/count_min_sketch.h"

#include <atomic>
#include <stdexcept>
#include <string>

namespace bustub {

/**
 * Constructor for the count-min sketch.
 *
 * @param width The width of the sketch matrix.
 * @param depth The depth of the sketch matrix.
 * @throws std::invalid_argument if width or depth are zero.
 */
template <typename KeyType>
CountMinSketch<KeyType>::CountMinSketch(uint32_t width, uint32_t depth)
    : width_(width), depth_(depth), matrix_(width_ * depth_) {
  /** @TODO(student) Implement this function! */

  if (width_ == 0 || depth_ == 0) {
    throw std::invalid_argument("Width and depth must be greater than zero.");
  }

  /** @spring2026 PLEASE DO NOT MODIFY THE FOLLOWING */
  // Initialize seeded hash functions
  hash_functions_.reserve(depth_);
  for (size_t i = 0; i < depth_; i++) {
    hash_functions_.push_back(this->HashFunction(i));
  }
}

template <typename KeyType>
CountMinSketch<KeyType>::CountMinSketch(CountMinSketch &&other) noexcept : width_(other.width_), depth_(other.depth_) {
  /** @TODO(student) Implement this function! */
  matrix_ = std::move(other.matrix_);
  hash_functions_.clear();
  for (uint32_t i = 0; i < depth_; ++i) {
    hash_functions_.push_back(HashFunction(i));
  }

  other.width_ = 0;
  other.depth_ = 0;
}

template <typename KeyType>
auto CountMinSketch<KeyType>::operator=(CountMinSketch &&other) noexcept -> CountMinSketch & {
  /** @TODO(student) Implement this function! */

  if (this != &other) {
    width_ = other.width_;
    depth_ = other.depth_;
    matrix_ = std::move(other.matrix_);
    hash_functions_.clear();
    for (uint32_t i = 0; i < depth_; ++i) {
      hash_functions_.push_back(HashFunction(i));
    }

    other.width_ = 0;
    other.depth_ = 0;
  }

  return *this;
}

template <typename KeyType>
void CountMinSketch<KeyType>::Insert(const KeyType &item) {
  /** @TODO(student) Implement this function! */

  for (size_t row = 0; row < depth_; row += 1) {
    // get the col index from hash_functions_
    size_t col = hash_functions_[row](item);

    // calculated the index for 1D matrix_
    size_t index = row * width_ + col;

    // increment safely efficiently here std::memory_order_relaxed makes faster increment
    matrix_[index].fetch_add(1, std::memory_order_relaxed);
  }
}

template <typename KeyType>
void CountMinSketch<KeyType>::Merge(const CountMinSketch<KeyType> &other) {
  if (width_ != other.width_ || depth_ != other.depth_) {
    throw std::invalid_argument("Incompatible CountMinSketch dimensions for merge.");
  }
  /** @TODO(student) Implement this function! */

  for (size_t i = 0; i < matrix_.size(); i += 1) {
    uint32_t other_val = other.matrix_[i].load(std::memory_order_relaxed);
    matrix_[i].fetch_add(other_val, std::memory_order_relaxed);
  }
}

template <typename KeyType>
auto CountMinSketch<KeyType>::Count(const KeyType &item) const -> uint32_t {
  uint32_t min_count = std::numeric_limits<uint32_t>::max();

  for (size_t row = 0; row < depth_; row += 1) {
    size_t col = hash_functions_[row](item);
    size_t index = row * width_ + col;

    uint32_t current_count = matrix_[index].load(std::memory_order_relaxed);
    min_count = std::min(min_count, current_count);
  }

  return min_count;
}

template <typename KeyType>
void CountMinSketch<KeyType>::Clear() {
  /** @TODO(student) Implement this function! */
  for (auto &cell : matrix_) {
    cell.store(0, std::memory_order_relaxed);
  }
}

template <typename KeyType>
auto CountMinSketch<KeyType>::TopK(uint16_t k, const std::vector<KeyType> &candidates)
    -> std::vector<std::pair<KeyType, uint32_t>> {
  /** @TODO(student) Implement this function! */
  std::vector<std::pair<KeyType, uint32_t>> top_k;
  top_k.reserve(candidates.size());

  for (const auto &item : candidates) {
    uint32_t count = Count(item);
    top_k.push_back({item, count});
  }

  std::sort(
      top_k.begin(), top_k.end(),
      [](const std::pair<KeyType, uint32_t> &a, const std::pair<KeyType, uint32_t> &b) { return a.second > b.second; });

  size_t return_size = std::min(static_cast<size_t>(k), top_k.size());

  top_k.resize(return_size);
  return top_k;
}

// Explicit instantiations for all types used in tests
template class CountMinSketch<std::string>;
template class CountMinSketch<int64_t>;  // For int64_t tests
template class CountMinSketch<int>;      // This covers both int and int32_t
}  // namespace bustub
