//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// b_plus_tree_delete_test.cpp
//
// Identification: test/storage/b_plus_tree_delete_test.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <cstdio>

#include "buffer/buffer_pool_manager.h"
#include "gtest/gtest.h"
#include "storage/b_plus_tree_utils.h"
#include "storage/disk/disk_manager_memory.h"
#include "storage/index/b_plus_tree.h"
#include "test_util.h"  // NOLINT

namespace bustub {

using bustub::DiskManagerUnlimitedMemory;

TEST(BPlusTreeTests, DISABLED_DeleteTestNoIterator) {
  // create KeyComparator and index schema
  auto key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema.get());

  auto disk_manager = std::make_unique<DiskManagerUnlimitedMemory>();
  auto *bpm = new BufferPoolManager(50, disk_manager.get());
  // allocate header_page
  page_id_t page_id = bpm->NewPage();
  // create b+ tree
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("foo_pk", page_id, bpm, comparator, 2, 3);
  GenericKey<8> index_key;
  RID rid;

  std::vector<int64_t> keys = {1, 2, 3, 4, 5};
  for (auto key : keys) {
    int64_t value = key & 0xFFFFFFFF;
    rid.Set(static_cast<int32_t>(key >> 32), value);
    index_key.SetFromInteger(key);
    tree.Insert(index_key, rid);
  }

  std::vector<RID> rids;
  for (auto key : keys) {
    rids.clear();
    index_key.SetFromInteger(key);
    tree.GetValue(index_key, &rids);
    EXPECT_EQ(rids.size(), 1);

    int64_t value = key & 0xFFFFFFFF;
    EXPECT_EQ(rids[0].GetSlotNum(), value);
  }

  std::vector<int64_t> remove_keys = {1, 5, 3, 4};
  for (auto key : remove_keys) {
    index_key.SetFromInteger(key);
    tree.Remove(index_key);
  }

  int64_t size = 0;
  bool is_present;

  for (auto key : keys) {
    rids.clear();
    index_key.SetFromInteger(key);
    is_present = tree.GetValue(index_key, &rids);

    if (!is_present) {
      EXPECT_NE(std::find(remove_keys.begin(), remove_keys.end(), key), remove_keys.end());
    } else {
      EXPECT_EQ(rids.size(), 1);
      EXPECT_EQ(rids[0].GetPageId(), 0);
      EXPECT_EQ(rids[0].GetSlotNum(), key);
      ++size;
    }
  }
  EXPECT_EQ(size, 1);

  // Remove the remaining key
  index_key.SetFromInteger(2);
  tree.Remove(index_key);
  auto root_page_id = tree.GetRootPageId();
  ASSERT_EQ(root_page_id, INVALID_PAGE_ID);

  delete bpm;
}

TEST(BPlusTreeTests, DISABLED_OptimisticDeleteTest) {
  auto key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema.get());

  auto disk_manager = std::make_unique<DiskManagerUnlimitedMemory>();
  auto *bpm = new BufferPoolManager(50, disk_manager.get());
  // allocate header_page
  page_id_t page_id = bpm->NewPage();
  // create b+ tree
  BPlusTree<GenericKey<8>, RID, GenericComparator<8>> tree("foo_pk", page_id, bpm, comparator, 4, 3);
  GenericKey<8> index_key;
  RID rid;

  size_t num_keys = 25;
  for (size_t i = 0; i < num_keys; i++) {
    int64_t value = i & 0xFFFFFFFF;
    rid.Set(static_cast<int32_t>(i >> 32), value);
    index_key.SetFromInteger(i);
    tree.Insert(index_key, rid);
  }

  size_t to_delete = num_keys + 1;
  auto leaf = IndexLeaves<GenericKey<8>, RID, GenericComparator<8>>(tree.GetRootPageId(), bpm);
  while (leaf.Valid()) {
    if ((*leaf)->GetSize() > (*leaf)->GetMinSize()) {
      to_delete = (*leaf)->KeyAt(0).GetAsInteger();
    }
    ++leaf;
  }

  auto base_reads = tree.bpm_->GetReads();
  auto base_writes = tree.bpm_->GetWrites();

  index_key.SetFromInteger(to_delete);
  tree.Remove(index_key);

  auto new_reads = tree.bpm_->GetReads();
  auto new_writes = tree.bpm_->GetWrites();

  EXPECT_GT(new_reads - base_reads, 0);
  EXPECT_EQ(new_writes - base_writes, 1);

  delete bpm;
}

TEST(BPlusTreeTests, DISABLED_SequentialEdgeMixTest) {  // NOLINT
  // create KeyComparator and index schema
  auto key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema.get());

  auto disk_manager = std::make_unique<DiskManagerUnlimitedMemory>();
  auto *bpm = new BufferPoolManager(50, disk_manager.get());

  for (int leaf_max_size = 2; leaf_max_size <= 5; leaf_max_size++) {
    // create and fetch header_page
    page_id_t page_id = bpm->NewPage();

    // create b+ tree
    BPlusTree<GenericKey<8>, RID, GenericComparator<8>, 2> tree("foo_pk", page_id, bpm, comparator, leaf_max_size, 3);
    GenericKey<8> index_key;
    RID rid;

    std::vector<int64_t> keys = {1, 5, 15, 20, 25, 2, -1, -2, 6, 14, 4};
    std::vector<int64_t> inserted = {};
    std::vector<int64_t> deleted = {};
    for (auto key : keys) {
      int64_t value = key & 0xFFFFFFFF;
      rid.Set(static_cast<int32_t>(key >> 32), value);
      index_key.SetFromInteger(key);
      tree.Insert(index_key, rid);
      inserted.push_back(key);
      auto res = TreeValuesMatch<GenericKey<8>, RID, GenericComparator<8>, 2>(tree, inserted, deleted);
      ASSERT_TRUE(res);
    }

    index_key.SetFromInteger(1);
    tree.Remove(index_key);
    deleted.push_back(1);
    inserted.erase(std::find(inserted.begin(), inserted.end(), 1));
    auto res = TreeValuesMatch<GenericKey<8>, RID, GenericComparator<8>, 2>(tree, inserted, deleted);
    ASSERT_TRUE(res);

    index_key.SetFromInteger(3);
    rid.Set(3, 3);
    tree.Insert(index_key, rid);
    inserted.push_back(3);
    res = TreeValuesMatch<GenericKey<8>, RID, GenericComparator<8>, 2>(tree, inserted, deleted);
    ASSERT_TRUE(res);

    keys = {4, 14, 6, 2, 15, -2, -1, 3, 5, 25, 20};
    for (auto key : keys) {
      index_key.SetFromInteger(key);
      tree.Remove(index_key);
      deleted.push_back(key);
      inserted.erase(std::find(inserted.begin(), inserted.end(), key));
      res = TreeValuesMatch<GenericKey<8>, RID, GenericComparator<8>, 2>(tree, inserted, deleted);
      ASSERT_TRUE(res);
    }
  }

  delete bpm;
}

TEST(BPlusTreeTests, DISABLED_LargeScaleMixRepro) {
  auto key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema.get());

  auto disk_manager = std::make_unique<DiskManagerUnlimitedMemory>();
  auto *bpm = new BufferPoolManager(200, disk_manager.get());
  page_id_t page_id = bpm->NewPage();

  BPlusTree<GenericKey<8>, RID, GenericComparator<8>, 2> tree("foo_pk", page_id, bpm, comparator, 4, 4);
  GenericKey<8> index_key;
  RID rid;

  std::vector<int64_t> inserted;
  const int64_t total = 1500;

  // Insert a large number of keys
  for (int64_t i = 0; i < total; i++) {
    int64_t value = i & 0xFFFFFFFF;
    rid.Set(static_cast<int32_t>(i >> 32), value);
    index_key.SetFromInteger(i);
    tree.Insert(index_key, rid);
    inserted.push_back(i);
  }

  // Delete all but 10, leaving a small final set
  for (int64_t i = 10; i < total; i++) {
    index_key.SetFromInteger(i);
    tree.Remove(index_key);
  }

  // Count via GetValue for every originally-inserted key
  int64_t live_count = 0;
  for (int64_t i = 0; i < total; i++) {
    index_key.SetFromInteger(i);
    std::vector<RID> rids;
    if (tree.GetValue(index_key, &rids)) {
      live_count++;
    }
  }

  fprintf(stderr, "live_count via GetValue = %ld (expected 10)\n", live_count);

  // Also count via the iterator
  int64_t iter_count = 0;
  for (auto it = tree.Begin(); it != tree.End(); ++it) {
    iter_count++;
  }
  fprintf(stderr, "iter_count via Begin/End = %ld (expected 10)\n", iter_count);

  EXPECT_EQ(live_count, 10);
  EXPECT_EQ(iter_count, 10);

  delete bpm;
}


TEST(BPlusTreeTests, DISABLED_LargeScaleMixRepro2) {
  auto key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema.get());

  auto disk_manager = std::make_unique<DiskManagerUnlimitedMemory>();
  auto *bpm = new BufferPoolManager(200, disk_manager.get());
  page_id_t page_id = bpm->NewPage();

  BPlusTree<GenericKey<8>, RID, GenericComparator<8>, 2> tree("foo_pk", page_id, bpm, comparator, 4, 4);
  GenericKey<8> index_key;
  RID rid;

  std::set<int64_t> present;
  const int64_t total = 1500;

  // Interleave insert and delete: insert i, and every 3rd step, delete
  // something further back to keep the tree churning.
  for (int64_t i = 0; i < total; i++) {
    int64_t value = i & 0xFFFFFFFF;
    rid.Set(static_cast<int32_t>(i >> 32), value);
    index_key.SetFromInteger(i);
    tree.Insert(index_key, rid);
    present.insert(i);

    if (i >= 20 && i % 3 == 0) {
      int64_t to_remove = i - 20;
      if (present.count(to_remove)) {
        index_key.SetFromInteger(to_remove);
        tree.Remove(index_key);
        present.erase(to_remove);
      }
    }
  }

  // Now delete down to exactly 10 remaining keys
  std::vector<int64_t> remaining(present.begin(), present.end());
  while (remaining.size() > 10) {
    int64_t to_remove = remaining.back();
    remaining.pop_back();
    index_key.SetFromInteger(to_remove);
    tree.Remove(index_key);
    present.erase(to_remove);
  }

  int64_t live_count = 0;
  for (int64_t i = 0; i < total; i++) {
    index_key.SetFromInteger(i);
    std::vector<RID> rids;
    if (tree.GetValue(index_key, &rids)) {
      live_count++;
    }
  }

  int64_t iter_count = 0;
  for (auto it = tree.Begin(); it != tree.End(); ++it) {
    iter_count++;
  }

  fprintf(stderr, "present.size()=%zu, live_count=%ld, iter_count=%ld (expected %zu)\n",
          present.size(), live_count, iter_count, present.size());

  EXPECT_EQ(static_cast<size_t>(live_count), present.size());
  EXPECT_EQ(static_cast<size_t>(iter_count), present.size());

  delete bpm;
}

TEST(BPlusTreeTests, DISABLED_LargeScaleMixRepro3) {
  auto key_schema = ParseCreateStatement("a bigint");
  GenericComparator<8> comparator(key_schema.get());

  auto disk_manager = std::make_unique<DiskManagerUnlimitedMemory>();
  auto *bpm = new BufferPoolManager(200, disk_manager.get());
  page_id_t page_id = bpm->NewPage();

  BPlusTree<GenericKey<8>, RID, GenericComparator<8>, 2> tree("foo_pk", page_id, bpm, comparator, 4, 4);
  GenericKey<8> index_key;
  RID rid;

  const int64_t total = 1250;

  for (int64_t i = 0; i < total; i++) {
    int64_t value = i & 0xFFFFFFFF;
    rid.Set(static_cast<int32_t>(i >> 32), value);
    index_key.SetFromInteger(i);
    tree.Insert(index_key, rid);
  }

  // Delete every key EXCEPT every 125th one (leaves exactly 10 survivors: 0, 125, 250, ...)
  int64_t deleted_count = 0;
  for (int64_t i = 0; i < total; i++) {
    if (i % 125 != 0) {
      index_key.SetFromInteger(i);
      tree.Remove(index_key);
      deleted_count++;

      // Check correctness incrementally, not just at the end
      std::vector<RID> rids;
      if (tree.GetValue(index_key, &rids)) {
        fprintf(stderr, "KEY %ld STILL PRESENT after Remove! (delete #%ld)\n", i, deleted_count);
      }
    }
  }

  int64_t size = 0;
  for (auto it = tree.Begin(); it != tree.End(); ++it) {
    size++;
  }
  fprintf(stderr, "final size=%ld (expected 10)\n", size);
  EXPECT_EQ(size, 10);

  delete bpm;
}

}  // namespace bustub
