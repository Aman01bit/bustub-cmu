//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// index_iterator.cpp
//
// Identification: src/storage/index/index_iterator.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

/**
 * index_iterator.cpp
 */
#include <cassert>

#include "storage/index/index_iterator.h"
#include "storage/page/b_plus_tree_leaf_page.h"

namespace bustub {

/**
 * @note you can change the destructor/constructor method here
 * set your own input parameters
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::IndexIterator() = default;

FULL_INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::IndexIterator(std::shared_ptr<TracedBufferPoolManager> bpm, ReadPageGuard guard, int index)
    : guard_(std::move(guard)), bpm_(std::move(bpm)), index_(index) {
  SkipTombstones();
}

FULL_INDEX_TEMPLATE_ARGUMENTS
INDEXITERATOR_TYPE::~IndexIterator() = default;  // NOLINT

FULL_INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::IsEnd() -> bool { return !guard_.has_value(); }

FULL_INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator*() -> std::pair<const KeyType &, const ValueType &> {
  const auto *leaf = guard_->As<LeafPage>();
  return {leaf->key_array_[index_], leaf->rid_array_[index_]};
}

FULL_INDEX_TEMPLATE_ARGUMENTS
void INDEXITERATOR_TYPE::SkipTombstones() {
  while (guard_.has_value()) {
    const auto *leaf = guard_->As<LeafPage>();

    while (index_ < leaf->GetSize() && IsTombStonedConst(leaf, index_)) {
      index_ += 1;
    }

    if (index_ < leaf->GetSize()) {
      return;
    }

    page_id_t next = leaf->GetNextPageId();
    if (next == INVALID_PAGE_ID) {
      guard_ = std::nullopt;
      index_ = 0;
      return;
    }

    guard_ = bpm_->ReadPage(next);
    index_ = 0;
  }
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator++() -> INDEXITERATOR_TYPE & {
  index_ += 1;
  SkipTombstones();
  return *this;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator==(const IndexIterator &other) const -> bool {
  if (!guard_.has_value() && !other.guard_.has_value()) {
    return true;
  }
  if (!guard_.has_value() || !other.guard_.has_value()) {
    return false;
  }
  return guard_->GetPageId() == other.guard_->GetPageId() && index_ == other.index_;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto INDEXITERATOR_TYPE::operator!=(const IndexIterator &other) const -> bool { return !(*this == other); }

template class IndexIterator<GenericKey<4>, RID, GenericComparator<4>>;

template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>>;
template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>, 3>;
template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>, 2>;
template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>, 1>;
template class IndexIterator<GenericKey<8>, RID, GenericComparator<8>, -1>;

template class IndexIterator<GenericKey<16>, RID, GenericComparator<16>>;

template class IndexIterator<GenericKey<32>, RID, GenericComparator<32>>;

template class IndexIterator<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
