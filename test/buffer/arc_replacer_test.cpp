//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// arc_replacer_test.cpp
//
// Identification: test/buffer/arc_replacer_test.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

/**
 * arc_replacer_test.cpp
 */

#include "buffer/arc_replacer.h"

#include "gtest/gtest.h"

#include <random>
#include <unordered_map>
#include <unordered_set>

namespace bustub {

TEST(ArcReplacerTest, SampleTest) {
  // for the sake of simplicity
  // we use (a, fb) to notate page a on frame b,
  // (a, _) to mark ghost page with page id a
  // p(a, fb) to mark pinned page a on frame b
  // we use [<-mru_ghost-][<-mru-]![-mfu->][->mfu_ghost->] p=x
  // to denote the 4 lists where pages closer to ! is are more fresh
  // and record the mru target size as x
  ArcReplacer arc_replacer(7);
  // Add six frames to the replacer.
  // We set frame 6 as non-evictable. These pages all go to mru list
  // We now have frames [][(1,f1), (2,f2), (3,f3), (4,f4), (5,f5), p(6,f6)]![][]
  arc_replacer.RecordAccess(1, 1);
  arc_replacer.RecordAccess(2, 2);
  arc_replacer.RecordAccess(3, 3);
  arc_replacer.RecordAccess(4, 4);
  arc_replacer.RecordAccess(5, 5);
  arc_replacer.RecordAccess(6, 6);
  arc_replacer.SetEvictable(1, true);
  arc_replacer.SetEvictable(2, true);
  arc_replacer.SetEvictable(3, true);
  arc_replacer.SetEvictable(4, true);
  arc_replacer.SetEvictable(5, true);
  arc_replacer.SetEvictable(6, false);

  // The size of the replacer is the number of frames that can be evicted, _not_ the total number of frames entered.
  ASSERT_EQ(5, arc_replacer.Size());
  // Record an access for frame 1. Now frame 1 goes into mfu list
  arc_replacer.RecordAccess(1, 1);
  // Now [][(2,f2), (3,f3), (4,f4), (5,f5), p(6,f6)]![(1,f1)][] p=0
  //
  // Now Evict three pages from the replacer.
  // Since target size is still 0, mru side should be evicted
  ASSERT_EQ(2, arc_replacer.Evict());
  ASSERT_EQ(3, arc_replacer.Evict());
  ASSERT_EQ(4, arc_replacer.Evict());
  ASSERT_EQ(2, arc_replacer.Size());
  // Now [(2,_), (3,_), (4,_)][(5,f5), p(6,f6)]![(1,f1)][] p=0

  // Insert new page 7 on frame 2, this should NOT be a hit on ghost
  // list since we've never seen page 7, this goes into mru list
  arc_replacer.RecordAccess(2, 7);
  arc_replacer.SetEvictable(2, true);
  // Insert page 2 on frame 3, this should be a hit on mru ghost list
  // since we've just evicted page 2, this goes into mfu list
  // also target size should be bumped up by 1, since mru ghost has
  // size 3 and mfu ghost has size 0
  // starting with x is pageid on ghost list, starting with _ is pinned
  arc_replacer.RecordAccess(3, 2);
  arc_replacer.SetEvictable(3, true);
  // Now [(3,_), (4,_)][(5,f5), p(6,f6), (7,f2)]![(2,f3), (1,f1)][] p=1
  ASSERT_EQ(4, arc_replacer.Size());

  // Continue to insert page 3 on frame 4 and page 4 on frame 7
  arc_replacer.RecordAccess(4, 3);
  arc_replacer.SetEvictable(4, true);
  arc_replacer.RecordAccess(7, 4);
  arc_replacer.SetEvictable(7, true);
  // Now [][(5,f5), p(6,f6), (7,f2)]![(4,f7), (3,f4), (2,f3), (1,f1)][] p=3
  ASSERT_EQ(6, arc_replacer.Size());

  // Evict an entry, now target size is 3, we still evict from mru
  ASSERT_EQ(5, arc_replacer.Evict());
  // Now [(5,_)][p(6,f6), (7,f2)]![(4,f7), (3,f4), (2,f3), (1,f1)][] p=3
  // Evict another entry, this time mru is smaller than target,
  // mfu is victimized
  ASSERT_EQ(1, arc_replacer.Evict());
  // Now [(5,_)][p(6,f6), (7,f2)]![(4,f7), (3,f4), (2,f3)][(1,_)] p=3

  // Make another access to page 1 on frame 5, now page 1 is back to mfu
  // with a different frame, also p is adjusted down by 1/1=1
  arc_replacer.RecordAccess(5, 1);
  arc_replacer.SetEvictable(5, true);
  // Now [(5,_)][p(6,f6), (7,f2)]![(1,f5), (4,f7), (3,f4), (2,f3)][] p=2
  ASSERT_EQ(5, arc_replacer.Size());

  // We evict again, this time target size is 2, we evict from mru,
  // note that page 6 is pinned. Victim is page 7
  // Now [(5,_), (7,_)][p(6,f6)]![(1,f5), (4,f7), (3,f4), (2,f3)][] p=2
  ASSERT_EQ(2, arc_replacer.Evict());
}

TEST(ArcReplacerTest, SampleTest2) {
  // Test a smaller capacity
  ArcReplacer arc_replacer(3);
  // Fill up the replacer
  arc_replacer.RecordAccess(1, 1);
  arc_replacer.SetEvictable(1, true);
  arc_replacer.RecordAccess(2, 2);
  arc_replacer.SetEvictable(2, true);
  arc_replacer.RecordAccess(3, 3);
  arc_replacer.SetEvictable(3, true);
  ASSERT_EQ(3, arc_replacer.Size());
  // Now [][(1,f1), (2,f2), (3,f3)]![][] p=0
  // Evict all pages
  ASSERT_EQ(1, arc_replacer.Evict());
  ASSERT_EQ(2, arc_replacer.Evict());
  ASSERT_EQ(3, arc_replacer.Evict());
  ASSERT_EQ(0, arc_replacer.Size());
  // Now [(1,_), (2,_), (3,_)][]![][] p=0

  // Insert a new page 4 with frame 3. This is case 4A
  // and ghost pages 1 should be driven out
  arc_replacer.RecordAccess(3, 4);
  arc_replacer.SetEvictable(3, true);
  // Now [(2,_), (3,_)][(4,f3)]![][] p=0

  // Access page 1 on frame 2, it should NOT be a hit on
  // the ghost list. Ghost page 2 should be driven out
  arc_replacer.RecordAccess(2, 1);
  arc_replacer.SetEvictable(2, true);
  ASSERT_EQ(2, arc_replacer.Size());
  // Now [(3,_)][(4,f3), (1,f2)]![][] p=0

  // Access page 3 with frame 1, this should be a ghost hit,
  // page 3 is placed on mfu and target size is bumped up by 1
  arc_replacer.RecordAccess(1, 3);
  arc_replacer.SetEvictable(1, true);
  // Now [][(4,f3), (1,f2)]![(3,f1)][] p=1

  // Make some more ghosts by evicting all pages again
  ASSERT_EQ(3, arc_replacer.Evict());
  ASSERT_EQ(2, arc_replacer.Evict());
  ASSERT_EQ(1, arc_replacer.Evict());
  // Now [(4,_), (1,_)][]![][(3,_)] p=1

  // Let's make even more ghost to fill the list to "full"
  // Insert page 1 again so it goes to mfu side,
  // target is bumped up by 1
  arc_replacer.RecordAccess(1, 1);
  arc_replacer.SetEvictable(1, true);
  // Now [(4,_)][]![(1,f1)][(3,_)] p=2

  // Insert page 4 again so it goes to mfu side,
  // target is bumped up by 1
  arc_replacer.RecordAccess(2, 4);
  arc_replacer.SetEvictable(2, true);
  // Now [][]![(4,f2),(1,f1)][(3,_)] p=3

  // Now insert and evict one new page at a time
  // Insert page 5 and evict, since target size is 3,
  // should victimize page 1
  arc_replacer.RecordAccess(3, 5);
  arc_replacer.SetEvictable(3, true);
  ASSERT_EQ(1, arc_replacer.Evict());
  // Now [][(5,f3)]![(4,f2)][(1,_),(3,_)] p=3
  // Insert page 6 and evict, notice target size is 3,
  // so page 4 gets evicted
  arc_replacer.RecordAccess(1, 6);
  arc_replacer.SetEvictable(1, true);
  ASSERT_EQ(2, arc_replacer.Evict());
  // Now [][(5,f3),(6,f1)]![(4,_),(1,_),(3,_)] p=3
  // Insert page 7 and evict, notice target size is 3,
  // so page 5 gets evicted
  arc_replacer.RecordAccess(2, 7);
  arc_replacer.SetEvictable(2, true);
  ASSERT_EQ(3, arc_replacer.Evict());
  // Now [(5,_)][(6,f1),(7,f2)]![][(4,_),(1,_),(3,_)] p=3

  // Now the list is full! reaching 2*capacity
  // adjust page 5 to mfu list
  arc_replacer.RecordAccess(3, 5);
  arc_replacer.SetEvictable(3, true);
  // Now [][(6,f1),(7,f2)]![(5,f3)][(4,_),(1,_),(3,_)] p=3

  // Now evict, target should be mfu
  ASSERT_EQ(3, arc_replacer.Evict());
  // Now [][(6,f1),(7,f2)]![][(5,_),(4,_),(1,_),(3,_)] p=3

  // Now mru and mru_ghost together has
  // less than 3 records. When inserting a new page 2
  // this should be case 4B and
  // four lists total size equals 2 * capacity case,
  // So mfu ghost will be shrinked
  arc_replacer.RecordAccess(3, 2);
  arc_replacer.SetEvictable(3, true);
  // Now [][(6,f1),(7,f2),(2,f3)]![][(5,_),(4,_),(1,_)] p=3

  // Evict a page 6
  ASSERT_EQ(1, arc_replacer.Evict());
  // Now [(6,_)][(7,f2),(2,f3)]![][(5,_),(4,_),(1,_)] p=3
  // And access page 3 who was removed
  // then this is case 4A, ghost page 6 will be removed
  arc_replacer.RecordAccess(1, 3);
  arc_replacer.SetEvictable(1, true);
  // Now [][(7,f2),(2,f3),(3,f1)]![][(5,_),(4,_),(1,_)] p=3

  // Finally we evict all pages and see if the order is right,
  // note that target size is 3
  ASSERT_EQ(2, arc_replacer.Evict());
  ASSERT_EQ(3, arc_replacer.Evict());
  ASSERT_EQ(1, arc_replacer.Evict());
}

TEST(ArcReplacerTest, RemoveBehaviorTest) {
  // Scenario 1: Remove from empty replacer — should be a no-op
  {
    ArcReplacer arc_replacer(5);
    arc_replacer.Remove(0);
    ASSERT_EQ(0, arc_replacer.Size());
  }

  // Scenario 2: Remove a non-existent frame_id — should be a no-op
  {
    ArcReplacer arc_replacer(5);
    arc_replacer.RecordAccess(0, 10);
    arc_replacer.SetEvictable(0, true);
    ASSERT_EQ(1, arc_replacer.Size());
    arc_replacer.Remove(99);  // never inserted
    ASSERT_EQ(1, arc_replacer.Size());
  }

  // Scenario 3: Remove an evictable frame from MRU
  {
    ArcReplacer arc_replacer(5);
    arc_replacer.RecordAccess(0, 10);
    arc_replacer.SetEvictable(0, true);
    arc_replacer.RecordAccess(1, 11);
    arc_replacer.SetEvictable(1, true);
    arc_replacer.RecordAccess(2, 12);
    arc_replacer.SetEvictable(2, true);
    // State: [][(10,f0),(11,f1),(12,f2)]![][] p=0
    ASSERT_EQ(3, arc_replacer.Size());

    arc_replacer.Remove(1);  // remove middle frame from MRU
    ASSERT_EQ(2, arc_replacer.Size());

    // Evict remaining: should be f0 then f2 (oldest to newest in MRU)
    ASSERT_EQ(0, arc_replacer.Evict());
    ASSERT_EQ(2, arc_replacer.Evict());
    ASSERT_FALSE(arc_replacer.Evict().has_value());
  }

  // Scenario 4: Remove an evictable frame from MFU
  {
    ArcReplacer arc_replacer(4);
    arc_replacer.RecordAccess(0, 20);
    arc_replacer.SetEvictable(0, true);
    arc_replacer.RecordAccess(1, 21);
    arc_replacer.SetEvictable(1, true);
    // Move frame 0 to MFU by accessing again
    arc_replacer.RecordAccess(0, 20);
    // State: [][(21,f1)]![(20,f0)][] p=0
    ASSERT_EQ(2, arc_replacer.Size());

    arc_replacer.Remove(0);  // remove from MFU
    ASSERT_EQ(1, arc_replacer.Size());

    // Only frame 1 remains
    ASSERT_EQ(1, arc_replacer.Evict());
    ASSERT_FALSE(arc_replacer.Evict().has_value());
  }

  // Scenario 5: Remove does NOT create ghost entries
  // If Evict were used instead of Remove, page_id 30 would go into a ghost list.
  // On re-access, it would be a ghost hit → placed in MFU and target size adjusted.
  // With Remove, no ghost is created → re-access goes to MRU as a fresh entry.
  {
    ArcReplacer arc_replacer(3);
    arc_replacer.RecordAccess(0, 30);
    arc_replacer.SetEvictable(0, true);
    arc_replacer.RecordAccess(1, 31);
    arc_replacer.SetEvictable(1, true);
    arc_replacer.RecordAccess(2, 32);
    arc_replacer.SetEvictable(2, true);
    // State: [][(30,f0),(31,f1),(32,f2)]![][] p=0

    // Remove frame 0 (page 30) — no ghost created
    arc_replacer.Remove(0);
    ASSERT_EQ(2, arc_replacer.Size());
    // State: [][(31,f1),(32,f2)]![][] p=0

    // Re-add frame 0 with the same page_id 30.
    // Since no ghost exists, this is Case IV (miss all lists) → goes to MRU.
    arc_replacer.RecordAccess(0, 30);
    arc_replacer.SetEvictable(0, true);
    // State: [][(31,f1),(32,f2),(30,f0)]![][] p=0

    // Evict order from MRU (target=0, all in MRU): f1, f2, f0
    ASSERT_EQ(1, arc_replacer.Evict());
    ASSERT_EQ(2, arc_replacer.Evict());
    ASSERT_EQ(0, arc_replacer.Evict());
  }

  // Scenario 6: Double remove (same frame_id twice)
  {
    ArcReplacer arc_replacer(3);
    arc_replacer.RecordAccess(0, 40);
    arc_replacer.SetEvictable(0, true);
    ASSERT_EQ(1, arc_replacer.Size());

    arc_replacer.Remove(0);
    ASSERT_EQ(0, arc_replacer.Size());

    // Second remove is a no-op (frame no longer in alive_map_)
    arc_replacer.Remove(0);
    ASSERT_EQ(0, arc_replacer.Size());
  }

  // Scenario 7: Remove then re-insert same frame with a different page_id
  {
    ArcReplacer arc_replacer(3);
    arc_replacer.RecordAccess(0, 50);
    arc_replacer.SetEvictable(0, true);

    arc_replacer.Remove(0);
    ASSERT_EQ(0, arc_replacer.Size());

    // Re-use frame 0 with a new page
    arc_replacer.RecordAccess(0, 51);
    arc_replacer.SetEvictable(0, true);
    ASSERT_EQ(1, arc_replacer.Size());
    ASSERT_EQ(0, arc_replacer.Evict());
  }

  // Scenario 8: Remove middle frame preserves eviction order of remaining frames
  {
    ArcReplacer arc_replacer(5);
    for (int i = 0; i < 5; i++) {
      arc_replacer.RecordAccess(i, 60 + i);
      arc_replacer.SetEvictable(i, true);
    }
    // MRU: [(60,f0),(61,f1),(62,f2),(63,f3),(64,f4)]
    arc_replacer.Remove(2);
    ASSERT_EQ(4, arc_replacer.Size());

    // Evict order: f0, f1, f3, f4 (frame 2 skipped)
    ASSERT_EQ(0, arc_replacer.Evict());
    ASSERT_EQ(1, arc_replacer.Evict());
    ASSERT_EQ(3, arc_replacer.Evict());
    ASSERT_EQ(4, arc_replacer.Evict());
    ASSERT_FALSE(arc_replacer.Evict().has_value());
  }

  // Scenario 9: Remove all frames one by one
  {
    ArcReplacer arc_replacer(5);
    for (int i = 0; i < 5; i++) {
      arc_replacer.RecordAccess(i, 80 + i);
      arc_replacer.SetEvictable(i, true);
    }
    ASSERT_EQ(5, arc_replacer.Size());

    for (int i = 0; i < 5; i++) {
      arc_replacer.Remove(i);
    }
    ASSERT_EQ(0, arc_replacer.Size());
    ASSERT_FALSE(arc_replacer.Evict().has_value());
  }
}

TEST(ArcReplacerTest, TargetSizeAdaptDownRepro) {
  ArcReplacer arc_replacer(6);

  // Build up MRU ghost list to be large (size 4), MFU ghost list to stay small (size 1)
  // Step 1: fill and evict 4 pages into mru_ghost_
  arc_replacer.RecordAccess(0, 10);
  arc_replacer.SetEvictable(0, true);
  arc_replacer.RecordAccess(1, 11);
  arc_replacer.SetEvictable(1, true);
  arc_replacer.RecordAccess(2, 12);
  arc_replacer.SetEvictable(2, true);
  arc_replacer.RecordAccess(3, 13);
  arc_replacer.SetEvictable(3, true);

  ASSERT_EQ(0, arc_replacer.Evict());  // page 10 -> mru_ghost_
  ASSERT_EQ(1, arc_replacer.Evict());  // page 11 -> mru_ghost_
  ASSERT_EQ(2, arc_replacer.Evict());  // page 12 -> mru_ghost_
  ASSERT_EQ(3, arc_replacer.Evict());  // page 13 -> mru_ghost_
  // mru_ghost_ = [13,12,11,10] size 4, mfu_ghost_ = [] size 0

  // Step 2: get one page into mfu_ghost_ (access twice then evict)
  arc_replacer.RecordAccess(4, 14);
  arc_replacer.SetEvictable(4, true);
  arc_replacer.RecordAccess(4, 14);    // second access -> moves to mfu_
  ASSERT_EQ(4, arc_replacer.Evict());  // page 14 -> mfu_ghost_
  // mru_ghost_ size 4, mfu_ghost_ size 1

  // Step 3: hit MRU ghost (page 10) to bump mru_target_size_ up
  // mru_ghost_size(4) >= mfu_ghost_size(1) -> +1
  arc_replacer.RecordAccess(5, 10);
  arc_replacer.SetEvictable(5, true);
  // mru_target_size_ should now be 1

  // Now hit MFU ghost (page 14): mfu_ghost_size(now 1) vs mru_ghost_size(now 3, since 10 was removed)
  // mfu_ghost_size(1) < mru_ghost_size(3) -> ratio = 3/1 = 3
  // mru_target_size_ should decrease by 3, but floor at 0
  arc_replacer.RecordAccess(0, 14);
  arc_replacer.SetEvictable(0, true);

  // PRINT/CHECK: mru_target_size_ should be 0 here (1 - 3, floored)
  // Add a getter or use existing test infra to inspect mru_target_size_ if available
  SUCCEED();
}

// Feel free to write more tests!

// Corrected version: never reuses a frame_id for a different page_id
// while that frame is still alive (matches real BufferPoolManager contract).

TEST(ArcReplacerTest, InvariantStressTestV2) {
  const size_t kCapacity = 8;
  ArcReplacer arc_replacer(kCapacity);

  std::mt19937 rng(12345);
  std::uniform_int_distribution<int> op_dist(0, 3);
  std::uniform_int_distribution<int> frame_dist(0, static_cast<int>(kCapacity) - 1);
  std::uniform_int_distribution<int> page_pool_dist(0, 30);  // pool of reusable page ids for ghost hits

  // Tracks which page (if any) currently occupies each frame while alive.
  std::unordered_map<frame_id_t, page_id_t> frame_to_page;
  std::unordered_map<frame_id_t, bool> evictable_state;

  auto check_invariants = [&](const std::string &label) {
    size_t expected_evictable = 0;
    for (auto &kv : evictable_state) {
      if (frame_to_page.count(kv.first) != 0 && kv.second) {
        expected_evictable++;
      }
    }
    ASSERT_EQ(expected_evictable, arc_replacer.Size()) << "Size() mismatch after " << label;
  };

  for (int step = 0; step < 5000; step++) {
    int op = op_dist(rng);
    frame_id_t fid = frame_dist(rng);
    bool frame_is_alive = frame_to_page.count(fid) != 0;

    if (op == 0) {
      if (!frame_is_alive) {
        // Only assign a new page to a frame that is currently free.
        page_id_t pid = page_pool_dist(rng);
        arc_replacer.RecordAccess(fid, pid);
        frame_to_page[fid] = pid;
        evictable_state[fid] = false;
      } else {
        // Frame already alive: re-access with its SAME page (simulates a repeat pin/hit).
        arc_replacer.RecordAccess(fid, frame_to_page[fid]);
      }
      check_invariants("RecordAccess");
    } else if (op == 1) {
      if (frame_is_alive) {
        bool make_evictable = (rng() % 2) == 0;
        arc_replacer.SetEvictable(fid, make_evictable);
        evictable_state[fid] = make_evictable;
        check_invariants("SetEvictable");
      }
    } else if (op == 2) {
      auto result = arc_replacer.Evict();
      if (result.has_value()) {
        frame_id_t evicted = result.value();
        frame_to_page.erase(evicted);
        evictable_state[evicted] = false;
      }
      check_invariants("Evict");
    } else {
      if (frame_is_alive && evictable_state[fid]) {
        arc_replacer.Remove(fid);
        frame_to_page.erase(fid);
        evictable_state[fid] = false;
        check_invariants("Remove");
      }
    }
  }

  SUCCEED();
}

TEST(ArcReplacerTest, TargetSizeAdaptDownManualTrace) {
  ArcReplacer arc_replacer(6);

  // Build 4 entries into mru_ghost_
  arc_replacer.RecordAccess(0, 10);
  arc_replacer.SetEvictable(0, true);
  arc_replacer.RecordAccess(1, 11);
  arc_replacer.SetEvictable(1, true);
  arc_replacer.RecordAccess(2, 12);
  arc_replacer.SetEvictable(2, true);
  arc_replacer.RecordAccess(3, 13);
  arc_replacer.SetEvictable(3, true);
  ASSERT_EQ(0, arc_replacer.Evict());
  ASSERT_EQ(1, arc_replacer.Evict());
  ASSERT_EQ(2, arc_replacer.Evict());
  ASSERT_EQ(3, arc_replacer.Evict());
  // mru_ghost_ = [13,12,11,10] size 4, mfu_ghost_ = [] size 0

  // Build 1 entry into mfu_ghost_
  arc_replacer.RecordAccess(4, 14);
  arc_replacer.SetEvictable(4, true);
  arc_replacer.RecordAccess(4, 14);  // -> mfu_
  ASSERT_EQ(4, arc_replacer.Evict());
  // mru_ghost_ size 4, mfu_ghost_ size 1

  // MRU ghost hit on page 10: mru_ghost_size(4) >= mfu_ghost_size(1) -> target += 1 -> target=1
  arc_replacer.RecordAccess(5, 10);
  arc_replacer.SetEvictable(5, true);
  // mru_ghost_ now [13,12,11] size 3, mfu_ghost_ still [14] size 1

  // MFU ghost hit on page 14: mfu_ghost_size(1) vs mru_ghost_size(3)
  // mfu_ghost_size(1) < mru_ghost_size(3) -> ratio = 3/1 = 3 -> target = max(0, 1-3) = 0
  arc_replacer.RecordAccess(0, 14);
  arc_replacer.SetEvictable(0, true);
  // target should now be 0

  // Add one more frame to mru_ so Evict() has something to pick from both sides
  arc_replacer.RecordAccess(1, 20);
  arc_replacer.SetEvictable(1, true);
  // mru_ = [20@f1], mfu_ = [14@f0, 10@f5]

  // target=0 means mru_.size()(1) >= target(0) is true -> should evict from mru_ first -> expect frame 1
  auto result = arc_replacer.Evict();
  std::cout << "Evict() returned: " << (result.has_value() ? result.value() : -1) << " (expected 1)\n";
  ASSERT_EQ(1, result);
}

}  // namespace bustub
