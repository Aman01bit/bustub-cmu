//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// b_plus_tree.cpp
//
// Identification: src/storage/index/b_plus_tree.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "storage/index/b_plus_tree.h"
#include "buffer/traced_buffer_pool_manager.h"
#include "storage/index/b_plus_tree_debug.h"
#include <unordered_map>
#include <algorithm>

namespace bustub {

FULL_INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(std::string name, page_id_t header_page_id, BufferPoolManager *buffer_pool_manager,
                          const KeyComparator &comparator, int leaf_max_size, int internal_max_size)
    : bpm_(std::make_shared<TracedBufferPoolManager>(buffer_pool_manager)),
      index_name_(std::move(name)),
      comparator_(std::move(comparator)),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size),
      header_page_id_(header_page_id) {
  WritePageGuard guard = bpm_->WritePage(header_page_id_);
  auto root_page = guard.AsMut<BPlusTreeHeaderPage>();
  root_page->root_page_id_ = INVALID_PAGE_ID;
}

/**
 * @brief Helper function to decide whether current b+tree is empty
 * @return Returns true if this B+ tree has no keys and values.
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsEmpty() const -> bool {
  // UNIMPLEMENTED("TODO(P2): Add implementation.");
  auto guard = bpm_->ReadPage(header_page_id_);
  auto header = guard.As<BPlusTreeHeaderPage>();
  return header->root_page_id_ == INVALID_PAGE_ID;
}

/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/**
 * @brief Return the only value that associated with input key
 *
 * This method is used for point query
 *
 * @param key input key
 * @param[out] result vector that stores the only value that associated with input key, if the value exists
 * @return : true means key exists
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetValue(const KeyType &key, std::vector<ValueType> *result) -> bool {
  // UNIMPLEMENTED("TODO(P2): Add implementation.");
  //  Declaration of context instance. Using the Context is not necessary but advised.
  Context ctx;
  ctx.root_page_id_ = GetRootPageId();

  if (ctx.root_page_id_ == INVALID_PAGE_ID) {
    return false;
  }

  ReadPageGuard guard = bpm_->ReadPage(ctx.root_page_id_);
  auto current_page = guard.As<BPlusTreePage>();

  while (!current_page->IsLeafPage()) {
    const auto *internal_page = guard.As<InternalPage>();

    int left = 1;
    int right = internal_page->GetSize() - 1;
    int child_index = 0;

    while (left <= right) {
      int mid = left + (right - left) / 2;
      if (comparator_(internal_page->KeyAt(mid), key) <= 0) {
        child_index = mid;
        left = mid + 1;
      } else {
        right = mid - 1;
      }
    }

    page_id_t child_page_id = internal_page->ValueAt(child_index);
    ReadPageGuard child_guard = bpm_->ReadPage(child_page_id);

    guard = std::move(child_guard);

    current_page = guard.As<BPlusTreePage>();
  }

  const auto *leaf_page = guard.As<LeafPage>();

  int left = 0;
  int right = leaf_page->GetSize() - 1;
  int key_index = -1;

  while (left <= right) {
    int mid = left + (right - left) / 2;
    if (comparator_(leaf_page->KeyAt(mid), key) <= 0) {
      key_index = mid;
      left = mid + 1;
    } else {
      right = mid - 1;
    }
  }

  if (key_index == -1 || comparator_(leaf_page->KeyAt(key_index), key) != 0) {
    return false;
  }

  for (size_t index = 0; index < leaf_page->num_tombstones_; index += 1) {
    if (leaf_page->tombstones_[index] == static_cast<size_t>(key_index)) {
      return false;
    }
  }

  result->push_back(leaf_page->rid_array_[key_index]);
  return true;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/**
 * @brief Insert constant key & value pair into b+ tree
 *
 * if current tree is empty, start new tree, update root page id and insert
 * entry; otherwise, insert into leaf page.
 *
 * @param key the key to insert
 * @param value the value associated with key
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false; otherwise, return true.
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value) -> bool {
  // UNIMPLEMENTED("TODO(P2): Add implementation.");
  //  Declaration of context instance. Using the Context is not necessary but advised.
  Context ctx;

  ctx.header_page_ = bpm_->WritePage(header_page_id_);
  auto header = ctx.header_page_->AsMut<BPlusTreeHeaderPage>();
  ctx.root_page_id_ = header->root_page_id_;

  if (ctx.root_page_id_ == INVALID_PAGE_ID) {
    page_id_t root_id = bpm_->NewPage();
    header->root_page_id_ = root_id;

    ctx.root_page_id_ = root_id;

    WritePageGuard root_guard = bpm_->WritePage(root_id);
    auto *root = root_guard.AsMut<LeafPage>();

    root->Init(leaf_max_size_);

    root->key_array_[0] = key;
    root->rid_array_[0] = value;

    root->ChangeSizeBy(1);

    return true;
  }

  WritePageGuard guard = bpm_->WritePage(ctx.root_page_id_);
  ctx.write_set_.push_back(std::move(guard));

  while (true) {
    auto page = ctx.write_set_.back().AsMut<BPlusTreePage>();
    if (page->IsLeafPage()) {
      break;
    }

    auto *internal_page = ctx.write_set_.back().AsMut<InternalPage>();

    int left = 1;
    int right = internal_page->GetSize() - 1;
    int child_index = 0;

    while (left <= right) {
      int mid = left + (right - left) / 2;
      if (comparator_(internal_page->KeyAt(mid), key) <= 0) {
        child_index = mid;
        left = mid + 1;
      } else {
        right = mid - 1;
      }
    }

    page_id_t child_page_id = internal_page->ValueAt(child_index);

    WritePageGuard child_guard = bpm_->WritePage(child_page_id);
    auto child_page = child_guard.AsMut<BPlusTreePage>();

    if (child_page->GetMaxSize() > child_page->GetSize()) {
      ctx.header_page_ = std::nullopt;
      while (!ctx.write_set_.empty()) {
        ctx.write_set_.pop_front();
      }
    }

    ctx.write_set_.push_back(std::move(child_guard));
  }

  KeyType seperator;
  page_id_t child_page_id;

  while (!ctx.write_set_.empty()) {
    auto guard = std::move(ctx.write_set_.back());
    ctx.write_set_.pop_back();

    auto page = guard.AsMut<BPlusTreePage>();

    if (ctx.root_page_id_ == guard.GetPageId()) {
      if (page->IsLeafPage()) {
        auto *leaf = guard.AsMut<LeafPage>();
        if (IsDuplicateKey(leaf, key)) {
          return false;
        }

        if (leaf->GetMaxSize() == leaf->GetSize()) {
          child_page_id = SplitLeaf(leaf);
          auto new_guard = bpm_->WritePage(child_page_id);
          auto *child_page = new_guard.AsMut<LeafPage>();

          seperator = child_page->KeyAt(0);

          if (comparator_(key, seperator) < 0) {
            InsertIntoLeaf(leaf, key, value);
          } else {
            InsertIntoLeaf(child_page, key, value);
          }

          page_id_t root_id = bpm_->NewPage();
          WritePageGuard root_guard = bpm_->WritePage(root_id);
          auto *new_root = root_guard.AsMut<InternalPage>();

          new_root->Init(internal_max_size_);
          new_root->SetKeyAt(1, seperator);
          new_root->page_id_array_[0] = guard.GetPageId();
          new_root->page_id_array_[1] = child_page_id;
          new_root->SetSize(2);

          auto new_header = ctx.header_page_->AsMut<BPlusTreeHeaderPage>();
          new_header->root_page_id_ = root_id;
          ctx.root_page_id_ = root_id;
        } else {
          InsertIntoLeaf(leaf, key, value);
        }

        return true;
      }

      auto *internal = guard.AsMut<InternalPage>();

      if (internal->GetMaxSize() == internal->GetSize()) {
        auto [new_seperator, new_child_page_id] = SplitInternal(internal);

        WritePageGuard new_guard = bpm_->WritePage(new_child_page_id);
        auto *internal_page = new_guard.AsMut<InternalPage>();

        if (comparator_(seperator, new_seperator) < 0) {
          InsertIntoInternal(internal, seperator, child_page_id);
        } else {
          InsertIntoInternal(internal_page, seperator, child_page_id);
        }

        seperator = new_seperator;
        child_page_id = new_child_page_id;

        page_id_t root_id = bpm_->NewPage();
        WritePageGuard root_guard = bpm_->WritePage(root_id);
        auto *new_root = root_guard.AsMut<InternalPage>();

        new_root->Init(internal_max_size_);
        new_root->SetKeyAt(1, seperator);
        new_root->page_id_array_[0] = guard.GetPageId();
        new_root->page_id_array_[1] = child_page_id;
        new_root->SetSize(2);

        auto new_header = ctx.header_page_->AsMut<BPlusTreeHeaderPage>();
        new_header->root_page_id_ = root_id;
        ctx.root_page_id_ = root_id;
      } else {
        InsertIntoInternal(internal, seperator, child_page_id);
      }

      return true;
    }

    if (page->IsLeafPage()) {
      auto *leaf = guard.AsMut<LeafPage>();

      if (IsDuplicateKey(leaf, key)) {
        return false;
      }

      if (leaf->GetMaxSize() == leaf->GetSize()) {
        child_page_id = SplitLeaf(leaf);
        auto new_guard = bpm_->WritePage(child_page_id);
        auto *child_page = new_guard.AsMut<LeafPage>();

        seperator = child_page->KeyAt(0);

        if (comparator_(key, seperator) < 0) {
          InsertIntoLeaf(leaf, key, value);
        } else {
          InsertIntoLeaf(child_page, key, value);
        }
      } else {
        InsertIntoLeaf(leaf, key, value);
        return true;
      }
    } else {
      auto *internal = guard.AsMut<InternalPage>();

      if (internal->GetMaxSize() == internal->GetSize()) {
        auto [new_seperator, new_child_page_id] = SplitInternal(internal);

        WritePageGuard new_guard = bpm_->WritePage(new_child_page_id);
        auto *internal_page = new_guard.AsMut<InternalPage>();

        if (comparator_(seperator, new_seperator) < 0) {
          InsertIntoInternal(internal, seperator, child_page_id);
        } else {
          InsertIntoInternal(internal_page, seperator, child_page_id);
        }

        seperator = new_seperator;
        child_page_id = new_child_page_id;
      } else {
        InsertIntoInternal(internal, seperator, child_page_id);
        return true;
      }
    }
  }
  // BUSTUB_ASSERT(false, "Insert: reached unreachable fallback — a code path is missing a return");
  return false;
}

/**
 * Helpers for insertion of the new Key.
 * Like Removing tombstoned, checking if tombstoned, updating the tombstoned in case
 * of insertion because after insertion of new key we need to upate all the tombstoned
 * after that insert_index.
 */

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsDuplicateKey(LeafPage *leaf, const KeyType &key) -> bool {
  int left = 0;
  int right = leaf->GetSize() - 1;
  int idx = -1;

  while (left <= right) {
    int mid = left + (right - left) / 2;
    if (comparator_(leaf->KeyAt(mid), key) <= 0) {
      idx = mid;
      left = mid + 1;
    } else {
      right = mid - 1;
    }
  }

  if (idx == -1 || comparator_(leaf->KeyAt(idx), key) != 0) {
    return false;
  }

  for (size_t i = 0; i < leaf->num_tombstones_; i += 1) {
    if (leaf->tombstones_[i] == static_cast<size_t>(idx)) {
      return false;
    }
  }
  return true;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::InsertIntoLeaf(LeafPage *leaf, const KeyType &key, const ValueType &value) -> bool {
  int left = 0;
  int right = leaf->GetSize() - 1;
  int insert_idx = 0;

  while (left <= right) {
    int mid = left + (right - left) / 2;

    if (comparator_(leaf->KeyAt(mid), key) < 0) {
      left = mid + 1;
    } else {
      right = mid - 1;
    }
  }
  insert_idx = left;

  if (insert_idx < leaf->GetSize() && comparator_(leaf->KeyAt(insert_idx), key) == 0) {
    if (auto tomb = IsTombStoned(leaf, insert_idx)) {
      RemoveTombStoned(leaf, static_cast<int>(*tomb));
      leaf->rid_array_[insert_idx] = value;
      return true;
    }
    return false;
  }

  for (int i = leaf->GetSize(); i > insert_idx; i -= 1) {
    leaf->key_array_[i] = leaf->key_array_[i - 1];
    leaf->rid_array_[i] = leaf->rid_array_[i - 1];
  }

  leaf->key_array_[insert_idx] = key;
  leaf->rid_array_[insert_idx] = value;

  UpdateTombStoned(leaf, insert_idx, 1);

  leaf->ChangeSizeBy(1);

  return true;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::SplitLeaf(LeafPage *leaf) -> page_id_t {
  page_id_t new_leaf_id = bpm_->NewPage();

  WritePageGuard guard = bpm_->WritePage(new_leaf_id);
  auto *new_leaf_page = guard.AsMut<LeafPage>();

  new_leaf_page->Init(leaf_max_size_);

  size_t split_index = leaf->GetMinSize();
  size_t new_size = leaf->GetSize() - split_index;

  for (size_t i = 0; i < new_size; i += 1) {
    new_leaf_page->key_array_[i] = leaf->key_array_[split_index + i];
    new_leaf_page->rid_array_[i] = leaf->rid_array_[split_index + i];
  }

  leaf->SetSize(split_index);
  new_leaf_page->SetSize(new_size);

  size_t left = 0;
  size_t right = 0;

  for (size_t i = 0; i < leaf->num_tombstones_; i += 1) {
    if (leaf->tombstones_[i] < split_index) {
      leaf->tombstones_[left] = leaf->tombstones_[i];
      left += 1;
    } else {
      new_leaf_page->tombstones_[right] = leaf->tombstones_[i] - split_index;
      right += 1;
    }
  }

  leaf->num_tombstones_ = left;
  new_leaf_page->num_tombstones_ = right;

  new_leaf_page->SetNextPageId(leaf->GetNextPageId());
  leaf->SetNextPageId(new_leaf_id);

  return new_leaf_id;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::InsertIntoInternal(InternalPage *internal, KeyType &seperator, page_id_t child_page_id) -> bool {
  int left = 1;
  int right = internal->GetSize() - 1;

  while (left <= right) {
    int mid = left + (right - left) / 2;

    if (comparator_(internal->KeyAt(mid), seperator) < 0) {
      left = mid + 1;
    } else {
      right = mid - 1;
    }
  }

  int insert_idx = left;

  for (int i = internal->GetSize(); i > insert_idx; i -= 1) {
    internal->key_array_[i] = internal->key_array_[i - 1];
    internal->page_id_array_[i] = internal->page_id_array_[i - 1];
  }

  internal->SetKeyAt(insert_idx, seperator);
  internal->page_id_array_[insert_idx] = child_page_id;

  internal->ChangeSizeBy(1);

  return true;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::SplitInternal(InternalPage *internal) -> std::pair<KeyType, page_id_t> {
  page_id_t new_page_id = bpm_->NewPage();
  WritePageGuard guard = bpm_->WritePage(new_page_id);

  auto *new_page = guard.AsMut<InternalPage>();

  new_page->Init(internal_max_size_);

  size_t split_index = internal->GetMinSize();
  size_t new_size = internal->GetSize() - split_index;

  for (size_t i = 1; i < new_size; i += 1) {
    new_page->key_array_[i] = internal->key_array_[split_index + i];
    new_page->page_id_array_[i] = internal->page_id_array_[split_index + i];
  }

  KeyType seperator = internal->KeyAt(split_index);
  new_page->page_id_array_[0] = internal->page_id_array_[split_index];

  internal->SetSize(split_index);
  new_page->SetSize(new_size);

  std::pair<KeyType, page_id_t> p = {seperator, new_page_id};
  return p;
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/**
 * @brief Delete key & value pair associated with input key
 * If current tree is empty, return immediately.
 * If not, User needs to first find the right leaf page as deletion target, then
 * delete entry from leaf page. Remember to deal with redistribute or merge if
 * necessary.
 *
 * @param key input key
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key) {
  // Declaration of context instance.
  // UNIMPLEMENTED("TODO(P2): Add implementation.");

  Context ctx;

  ctx.header_page_ = bpm_->WritePage(header_page_id_);
  auto header = ctx.header_page_->AsMut<BPlusTreeHeaderPage>();
  ctx.root_page_id_ = header->root_page_id_;

  if (ctx.root_page_id_ == INVALID_PAGE_ID) {
    return;
  }

  WritePageGuard guard = bpm_->WritePage(ctx.root_page_id_);
  ctx.write_set_.push_back(std::move(guard));

  while (true) {
    auto page = ctx.write_set_.back().AsMut<BPlusTreePage>();

    if (page->IsLeafPage()) {
      break;
    }

    auto *internal = ctx.write_set_.back().AsMut<InternalPage>();

    int left = 1;
    int right = internal->GetSize() - 1;
    int child_index = 0;

    while (left <= right) {
      int mid = left + (right - left) / 2;
      if (comparator_(internal->KeyAt(mid), key) <= 0) {
        child_index = mid;
        left = mid + 1;
      } else {
        right = mid - 1;
      }
    }

    page_id_t child_page_id = internal->ValueAt(child_index);
    WritePageGuard child_guard = bpm_->WritePage(child_page_id);
    auto child_page = child_guard.AsMut<BPlusTreePage>();

    bool child_safe = child_page->IsLeafPage() ? IsLeafSafeToRemove(child_guard.AsMut<LeafPage>())
                                               : IsInternalSafeToRemove(child_guard.AsMut<InternalPage>());

    if (child_safe) {
      ctx.header_page_ = std::nullopt;
      ctx.write_set_.clear();
    }

    ctx.write_set_.push_back(std::move(child_guard));
  }

  while (!ctx.write_set_.empty()) {
    auto guard = std::move(ctx.write_set_.back());
    ctx.write_set_.pop_back();

    auto page = guard.AsMut<BPlusTreePage>();

    if (guard.GetPageId() == ctx.root_page_id_) {
      if (page->IsLeafPage()) {
        auto *leaf = guard.AsMut<LeafPage>();
        DeleteFromLeaf(leaf, key);

        int effective_size = leaf->GetSize() - static_cast<int>(leaf->num_tombstones_);
        if (effective_size == 0) {
          auto header = ctx.header_page_->AsMut<BPlusTreeHeaderPage>();
          header->root_page_id_ = INVALID_PAGE_ID;
          bpm_->DeletePage(guard.GetPageId());
        }

        return;
      }

      auto *root = guard.AsMut<InternalPage>();

      if (root->GetSize() == 1) {
        page_id_t new_root = root->ValueAt(0);

        auto header = ctx.header_page_->AsMut<BPlusTreeHeaderPage>();
        header->root_page_id_ = new_root;
        ctx.root_page_id_ = new_root;

        bpm_->DeletePage(guard.GetPageId());
      }

      return;
    }

    if (page->IsLeafPage()) {
      auto *leaf = guard.AsMut<LeafPage>();

      auto underflow = DeleteFromLeaf(leaf, key);
      if (underflow == std::nullopt) {
        return;
      }
      // BUSTUB_ASSERT(!ctx.write_set_.empty(), "internal branch: write_set_ empty, no parent available");
      auto *parent_internal = ctx.write_set_.back().AsMut<InternalPage>();

      int left = 1;
      int right = parent_internal->GetSize() - 1;
      int child_index = 0;

      while (left <= right) {
        int mid = left + (right - left) / 2;
        if (comparator_(parent_internal->KeyAt(mid), key) <= 0) {
          child_index = mid;
          left = mid + 1;
        } else {
          right = mid - 1;
        }
      }

      page_id_t left_sibling = INVALID_PAGE_ID;
      page_id_t right_sibling = INVALID_PAGE_ID;

      if (child_index > 0) {
        left_sibling = parent_internal->ValueAt(child_index - 1);
      }

      if (child_index + 1 < parent_internal->GetSize()) {
        right_sibling = parent_internal->ValueAt(child_index + 1);
      }

      int need = leaf->GetMinSize() - leaf->GetSize();
      bool redistributed = false;
      if (left_sibling != INVALID_PAGE_ID) {
        auto left_guard = bpm_->WritePage(left_sibling);
        auto *left_leaf = left_guard.AsMut<LeafPage>();

        int can_move = left_leaf->GetSize() - left_leaf->GetMinSize();

        if (can_move >= need) {
          RedistributeLeaf(leaf, left_leaf, LEFT_TO_RIGHT);
          parent_internal->SetKeyAt(child_index, leaf->KeyAt(0));
          if (IsLeafBalanced(leaf)) {
            redistributed = true;
          }
        }
      }

      if (!redistributed && right_sibling != INVALID_PAGE_ID) {
        auto right_guard = bpm_->WritePage(right_sibling);
        auto *right_leaf = right_guard.AsMut<LeafPage>();

        int can_move = right_leaf->GetSize() - right_leaf->GetMinSize();

        if (can_move >= need) {
          RedistributeLeaf(leaf, right_leaf, RIGHT_TO_LEFT);
          parent_internal->SetKeyAt(child_index + 1, right_leaf->KeyAt(0));
          if (IsLeafBalanced(leaf)) {
            redistributed = true;
          }
        }
      }

      if (redistributed) {
        return;
      }

      if (left_sibling != INVALID_PAGE_ID) {
        auto left_guard = bpm_->WritePage(left_sibling);
        auto *left_leaf = left_guard.AsMut<LeafPage>();

        MergeLeaf(left_leaf, leaf);
        DeleteFromInternal(parent_internal, child_index);

        bpm_->DeletePage(guard.GetPageId());
      } else if (right_sibling != INVALID_PAGE_ID) {
        auto right_guard = bpm_->WritePage(right_sibling);
        auto *right_leaf = right_guard.AsMut<LeafPage>();

        MergeLeaf(leaf, right_leaf);
        DeleteFromInternal(parent_internal, child_index + 1);

        bpm_->DeletePage(right_sibling);
      }

      if (IsInternalBalanced(parent_internal)) {
        return;
      }

    } else {
      auto *internal = guard.AsMut<InternalPage>();
      if (IsInternalBalanced(internal)) {
        return;
      }

      // BUSTUB_ASSERT(!ctx.write_set_.empty(), "internal branch: write_set_ empty, no parent available");
      auto *parent = ctx.write_set_.back().AsMut<InternalPage>();

      int left = 1;
      int right = parent->GetSize() - 1;
      int child_index = 0;

      while (left <= right) {
        int mid = left + (right - left) / 2;

        if (comparator_(parent->KeyAt(mid), key) <= 0) {
          child_index = mid;
          left = mid + 1;
        } else {
          right = mid - 1;
        }
      }

      page_id_t left_sibling = INVALID_PAGE_ID;
      page_id_t right_sibling = INVALID_PAGE_ID;

      if (child_index > 0) {
        left_sibling = parent->ValueAt(child_index - 1);
      }
      if (child_index + 1 < parent->GetSize()) {
        right_sibling = parent->ValueAt(child_index + 1);
      }

      const int need = internal->GetMinSize() - internal->GetSize();
      bool redistributed = false;
      if (left_sibling != INVALID_PAGE_ID) {
        auto left_guard = bpm_->WritePage(left_sibling);
        auto *left_internal = left_guard.AsMut<InternalPage>();

        const int can_move = left_internal->GetSize() - left_internal->GetMinSize();

        if (can_move >= need) {
          const KeyType seperator = parent->KeyAt(child_index);
          std::optional<KeyType> new_seperator =
              RedistributeInternal(internal, left_internal, seperator, LEFT_TO_RIGHT);
          parent->SetKeyAt(child_index, new_seperator.value());
          if (IsInternalBalanced(internal)) {
            redistributed = true;
          }
        }
      }

      if (!redistributed && right_sibling != INVALID_PAGE_ID) {
        auto right_guard = bpm_->WritePage(right_sibling);
        auto *right_internal = right_guard.AsMut<InternalPage>();

        const int can_move = right_internal->GetSize() - right_internal->GetMinSize();

        if (can_move >= need) {
          const KeyType seperator = parent->KeyAt(child_index + 1);
          std::optional<KeyType> new_seperator =
              RedistributeInternal(internal, right_internal, seperator, RIGHT_TO_LEFT);
          parent->SetKeyAt(child_index + 1, new_seperator.value());
          if (IsInternalBalanced(internal)) {
            redistributed = true;
          }
        }
      }

      if (redistributed) {
        return;
      }

      if (left_sibling != INVALID_PAGE_ID) {
        auto left_guard = bpm_->WritePage(left_sibling);
        auto *left_internal = left_guard.AsMut<InternalPage>();

        const KeyType seperator = parent->KeyAt(child_index);

        MergeInternal(left_internal, internal, seperator);
        DeleteFromInternal(parent, child_index);

        bpm_->DeletePage(guard.GetPageId());
      } else if (right_sibling != INVALID_PAGE_ID) {
        auto right_guard = bpm_->WritePage(right_sibling);
        auto *right_internal = right_guard.AsMut<InternalPage>();

        const KeyType seperator = parent->KeyAt(child_index + 1);

        MergeInternal(internal, right_internal, seperator);
        DeleteFromInternal(parent, child_index + 1);

        bpm_->DeletePage(right_sibling);
      }

      if (IsInternalBalanced(parent)) {
        return;
      }
    }
  }
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::RedistributeInternal(InternalPage *reciever, InternalPage *donor, const KeyType &seperator,
                                          Direction direction) -> std::optional<KeyType> {
  const int need = reciever->GetMinSize() - reciever->GetSize();
  const int can_move = donor->GetSize() - donor->GetMinSize();
  const int move_count = std::min(need, can_move);

  if (move_count <= 0) {
    return std::nullopt;
  }

  if (direction == LEFT_TO_RIGHT) {
    int right_idx = reciever->GetSize() + move_count - 1;

    for (int i = reciever->GetSize() - 1; i > 0; i -= 1) {
      reciever->key_array_[right_idx] = reciever->key_array_[i];
      reciever->page_id_array_[right_idx] = reciever->page_id_array_[i];
      right_idx -= 1;
    }

    reciever->key_array_[right_idx] = seperator;
    reciever->page_id_array_[right_idx] = reciever->page_id_array_[0];

    int left_idx = donor->GetSize() - 1;

    for (int i = right_idx - 1; i > 0; i -= 1) {
      reciever->key_array_[i] = donor->key_array_[left_idx];
      reciever->page_id_array_[i] = donor->page_id_array_[left_idx];
      left_idx -= 1;
    }

    reciever->page_id_array_[0] = donor->page_id_array_[left_idx];
    auto new_seperator = donor->key_array_[left_idx];

    reciever->ChangeSizeBy(move_count);
    donor->ChangeSizeBy(-move_count);

    return new_seperator;
  }

  int left_idx = reciever->GetSize() - 1;

  reciever->key_array_[left_idx] = seperator;
  reciever->page_id_array_[left_idx] = donor->page_id_array_[0];
  left_idx += 1;

  for (int i = 1; i < move_count; i += 1) {
    reciever->key_array_[left_idx] = donor->key_array_[i];
    reciever->page_id_array_[left_idx] = donor->page_id_array_[i];
    left_idx += 1;
  }

  auto new_seperator = donor->key_array_[move_count];
  donor->page_id_array_[0] = donor->page_id_array_[move_count];

  for (int i = move_count + 1; i < donor->GetSize(); i += 1) {
    donor->key_array_[i - move_count] = donor->key_array_[i];
    donor->page_id_array_[i - move_count] = donor->page_id_array_[i];
  }

  donor->ChangeSizeBy(-move_count);
  reciever->ChangeSizeBy(move_count);

  return new_seperator;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::MergeInternal(InternalPage *receiver, InternalPage *donor, const KeyType &separator) -> void {
  const int receiver_size = receiver->GetSize();

  receiver->SetKeyAt(receiver_size, separator);

  receiver->page_id_array_[receiver_size] = donor->ValueAt(0);

  for (int i = 1; i < donor->GetSize(); i++) {
    receiver->SetKeyAt(receiver_size + i, donor->KeyAt(i));
    receiver->page_id_array_[receiver_size + i] = donor->ValueAt(i);
  }

  receiver->ChangeSizeBy(donor->GetSize());
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::DeleteFromInternal(InternalPage *internal, int child_index) -> void {
  for (int i = child_index; i < internal->GetSize() - 1; i++) {
    internal->page_id_array_[i] = internal->page_id_array_[i + 1];
  }

  const int key_index = (child_index == 0) ? 1 : child_index;

  for (int i = key_index; i < internal->GetSize() - 1; i++) {
    internal->SetKeyAt(i, internal->KeyAt(i + 1));
  }

  internal->ChangeSizeBy(-1);
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::DeleteFromLeaf(LeafPage *leaf, const KeyType &key) -> std::optional<int> {
  int left = 0;
  int right = leaf->GetSize() - 1;

  while (left <= right) {
    int mid = left + (right - left) / 2;

    if (comparator_(leaf->KeyAt(mid), key) < 0) {
      left = mid + 1;
    } else {
      right = mid - 1;
    }
  }

  int remove_idx = left;

  if (remove_idx >= leaf->GetSize() || comparator_(leaf->KeyAt(remove_idx), key) != 0) {
    return std::nullopt;
  }

  if (IsTombStoned(leaf, remove_idx)) {
    return std::nullopt;
  }

  if (LEAF_PAGE_TOMB_CNT == 0) {
    for (int i = remove_idx; i < leaf->GetSize() - 1; i += 1) {
      leaf->key_array_[i] = leaf->key_array_[i + 1];
      leaf->rid_array_[i] = leaf->rid_array_[i + 1];
    }
    leaf->ChangeSizeBy(-1);
  } else if (leaf->num_tombstones_ == LEAF_PAGE_TOMB_CNT) {
    int old_tomb = leaf->tombstones_[0];

    for (int i = old_tomb; i < leaf->GetSize() - 1; i += 1) {
      leaf->key_array_[i] = leaf->key_array_[i + 1];
      leaf->rid_array_[i] = leaf->rid_array_[i + 1];
    }

    leaf->ChangeSizeBy(-1);
    UpdateTombStoned(leaf, old_tomb, -1);

    if (remove_idx > old_tomb) {
      remove_idx -= 1;
    }
    RemoveTombStoned(leaf, 0);

    InsertTombstone(leaf, remove_idx);
  } else {
    InsertTombstone(leaf, remove_idx);
  }

  if (IsLeafBalanced(leaf)) {
    return std::nullopt;
  }
  return remove_idx;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::CountTombstonesInRange(LeafPage *donor, int lo, int hi) -> int {
  int count = 0;
  for (size_t i = 0; i < donor->num_tombstones_; i += 1) {
    int t = static_cast<int>(donor->tombstones_[i]);
    if (t >= lo && t < hi) {
      count += 1;
    }
  }
  return count;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::EvictOldestTombstone(LeafPage *leaf) -> void {
  int old_idx = static_cast<int>(leaf->tombstones_[0]);

  for (int i = old_idx; i < leaf->GetSize() - 1; i += 1) {
    leaf->key_array_[i] = leaf->key_array_[i + 1];
    leaf->rid_array_[i] = leaf->rid_array_[i + 1];
  }
  leaf->ChangeSizeBy(-1);

  RemoveTombStoned(leaf, 0);
  UpdateTombStoned(leaf, old_idx, -1);
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::RemoveTombStoned(LeafPage *leaf, int tomb_idx) -> void {
  leaf->num_tombstones_ -= 1;
  for (size_t i = tomb_idx; i < leaf->num_tombstones_; i += 1) {
    leaf->tombstones_[i] = leaf->tombstones_[i + 1];
  }
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::InsertTombstone(LeafPage *leaf, int idx) -> void {
  leaf->tombstones_[leaf->num_tombstones_] = idx;
  leaf->num_tombstones_ += 1;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsTombStoned(LeafPage *leaf, int index) -> std::optional<size_t> {
  for (size_t i = 0; i < leaf->num_tombstones_; i += 1) {
    if (leaf->tombstones_[i] == static_cast<size_t>(index)) {
      return i;
    }
  }
  return std::nullopt;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::UpdateTombStoned(LeafPage *leaf, int index, int amt) -> void {
  for (size_t i = 0; i < leaf->num_tombstones_; i += 1) {
    if (leaf->tombstones_[i] >= static_cast<size_t>(index)) {
      leaf->tombstones_[i] += amt;
    }
  }
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::MergeLeaf(LeafPage *receiver, LeafPage *donor) -> void {
  int incoming_tombs = static_cast<int>(donor->num_tombstones_);
  int available = LEAF_PAGE_TOMB_CNT - static_cast<int>(receiver->num_tombstones_);
  int to_evict = incoming_tombs - available;
  for (int e = 0; e < to_evict && receiver->num_tombstones_ > 0; e += 1) {
    EvictOldestTombstone(receiver);
  }

  int receiver_idx = receiver->GetSize();

  for (int i = 0; i < donor->GetSize(); i++) {
    receiver->key_array_[receiver_idx + i] = donor->key_array_[i];
    receiver->rid_array_[receiver_idx + i] = donor->rid_array_[i];
  }

  for (size_t t = 0; t < donor->num_tombstones_; t += 1) {
    int donor_local_idx = static_cast<int>(donor->tombstones_[t]);
    InsertTombstone(receiver, receiver_idx + donor_local_idx);
  }

  receiver->ChangeSizeBy(donor->GetSize());
  receiver->SetNextPageId(donor->GetNextPageId());
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::RedistributeLeaf(LeafPage *reciever, LeafPage *donor, Direction direction) -> bool {
  int can_move = donor->GetSize() - donor->GetMinSize();
  int need = reciever->GetMinSize() - reciever->GetSize();
  int move_count = std::min(can_move, need);
  if (move_count <= 0) {
    return false;
  }

  int incoming_tombs;
  if (direction == RIGHT_TO_LEFT) {
    incoming_tombs = CountTombstonesInRange(donor, 0, move_count);
  } else {
    incoming_tombs = CountTombstonesInRange(donor, donor->GetSize() - move_count, donor->GetSize());
  }
  int available = LEAF_PAGE_TOMB_CNT - static_cast<int>(reciever->num_tombstones_);
  int to_evict = incoming_tombs - available;
  for (int e = 0; e < to_evict && reciever->num_tombstones_ > 0; e += 1) {
    EvictOldestTombstone(reciever);
  }

  if (direction == RIGHT_TO_LEFT) {
    int left_idx = reciever->GetSize();
    int right_idx = 0;

    for (int i = left_idx; i < left_idx + move_count; i += 1) {
      reciever->key_array_[i] = donor->key_array_[right_idx];
      reciever->rid_array_[i] = donor->rid_array_[right_idx];
      right_idx++;
    }

    size_t write = 0;
    for (size_t t = 0; t < donor->num_tombstones_; t += 1) {
      int idx = static_cast<int>(donor->tombstones_[t]);
      if (idx < move_count) {
        InsertTombstone(reciever, left_idx + idx);
      } else {
        donor->tombstones_[write] = idx;
        write++;
      }
    }
    donor->num_tombstones_ = write;

    int start = 0;
    while (right_idx < donor->GetSize()) {
      donor->key_array_[start] = donor->key_array_[right_idx];
      donor->rid_array_[start] = donor->rid_array_[right_idx];
      start += 1;
      right_idx += 1;
    }

    reciever->ChangeSizeBy(move_count);
    donor->ChangeSizeBy(-move_count);
    UpdateTombStoned(donor, 0, -move_count);
    return true;
  }

  int left_idx = reciever->GetSize() + move_count - 1;
  for (int i = reciever->GetSize() - 1; i >= 0; i -= 1) {
    reciever->key_array_[left_idx] = reciever->key_array_[i];
    reciever->rid_array_[left_idx] = reciever->rid_array_[i];
    left_idx -= 1;
  }
  UpdateTombStoned(reciever, 0, move_count);
  reciever->ChangeSizeBy(move_count);

  int right_idx = donor->GetSize() - 1;
  for (int i = move_count - 1; i >= 0; i -= 1) {
    reciever->key_array_[i] = donor->key_array_[right_idx];
    reciever->rid_array_[i] = donor->rid_array_[right_idx];
    right_idx -= 1;
  }

  int donor_move_start = donor->GetSize() - move_count;
  size_t write = 0;
  for (size_t t = 0; t < donor->num_tombstones_; t += 1) {
    int idx = static_cast<int>(donor->tombstones_[t]);
    if (idx >= donor_move_start) {
      InsertTombstone(reciever, idx - donor_move_start);
    } else {
      donor->tombstones_[write] = idx;
      write++;
    }
  }
  donor->num_tombstones_ = write;

  donor->ChangeSizeBy(-move_count);
  return true;
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsLeafSafeToRemove(LeafPage *leaf) -> bool { return (leaf->GetSize() - 1) >= leaf->GetMinSize(); }

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsLeafBalanced(LeafPage *leaf) -> bool { return leaf->GetSize() >= leaf->GetMinSize(); }

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsInternalBalanced(InternalPage *internal) -> bool {
  return internal->GetSize() >= internal->GetMinSize();
}

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsInternalSafeToRemove(InternalPage *internal) -> bool {
  return (internal->GetSize() - 1) >= internal->GetMinSize();
}

/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/**
 * @brief Input parameter is void, find the leftmost leaf page first, then construct
 * index iterator
 *
 * You may want to implement this while implementing Task #3.
 *
 * @return : index iterator
 */

FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin() -> INDEXITERATOR_TYPE {
  page_id_t root_page_id = GetRootPageId();
  if (root_page_id == INVALID_PAGE_ID) {
    return End();
  }

  ReadPageGuard guard = bpm_->ReadPage(root_page_id);
  auto current_page = guard.As<BPlusTreePage>();

  while (!current_page->IsLeafPage()) {
    const auto *internal_page = guard.As<InternalPage>();
    page_id_t child_page_id = internal_page->ValueAt(0);

    guard = bpm_->ReadPage(child_page_id);
    current_page = guard.As<BPlusTreePage>();
  }

  return INDEXITERATOR_TYPE(bpm_, std::move(guard), 0);
}

/**
 * @brief Input parameter is low key, find the leaf page that contains the input key
 * first, then construct index iterator
 * @return : index iterator
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin(const KeyType &key) -> INDEXITERATOR_TYPE {
  page_id_t root_page_id = GetRootPageId();
  if (root_page_id == INVALID_PAGE_ID) {
    return End();
  }

  ReadPageGuard guard = bpm_->ReadPage(root_page_id);
  auto current_page = guard.As<BPlusTreePage>();

  while (!current_page->IsLeafPage()) {
    const auto *internal_page = guard.As<InternalPage>();

    int left = 1;
    int right = internal_page->GetSize() - 1;
    int child_index = 0;
    while (left <= right) {
      int mid = left + (right - left) / 2;
      if (comparator_(internal_page->KeyAt(mid), key) <= 0) {
        child_index = mid;
        left = mid + 1;
      } else {
        right = mid - 1;
      }
    }

    page_id_t child_page_id = internal_page->ValueAt(child_index);
    guard = bpm_->ReadPage(child_page_id);
    current_page = guard.As<BPlusTreePage>();
  }

  const auto *leaf = guard.As<LeafPage>();

  int left = 0;
  int right = leaf->GetSize() - 1;
  int idx = leaf->GetSize();
  while (left <= right) {
    int mid = left + (right - left) / 2;
    if (comparator_(leaf->KeyAt(mid), key) >= 0) {
      idx = mid;
      right = mid - 1;
    } else {
      left = mid + 1;
    }
  }

  return INDEXITERATOR_TYPE(bpm_, std::move(guard), idx);
}

/**
 * @brief Input parameter is void, construct an index iterator representing the end
 * of the key/value pair in the leaf node
 * @return : index iterator
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::End() -> INDEXITERATOR_TYPE { return INDEXITERATOR_TYPE(); }
/**
 * @return Page id of the root of this tree
 *
 * You may want to implement this while implementing Task #3.
 */
FULL_INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetRootPageId() -> page_id_t {
  // UNIMPLEMENTED("TODO(P2): Add implementation.");
  ReadPageGuard guard = bpm_->ReadPage(header_page_id_);
  auto header = guard.As<BPlusTreeHeaderPage>();
  return header->root_page_id_;
}

template class BPlusTree<GenericKey<4>, RID, GenericComparator<4>>;

template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>, 3>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>, 2>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>, 1>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>, -1>;

template class BPlusTree<GenericKey<16>, RID, GenericComparator<16>>;

template class BPlusTree<GenericKey<32>, RID, GenericComparator<32>>;

template class BPlusTree<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub
