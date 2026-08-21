// :bustub-keep-private:
//===----------------------------------------------------------------------===//
//
//                         BusTub
//
// arc_replacer.cpp
//
// Identification: src/buffer/arc_replacer.cpp
//
// Copyright (c) 2015-2025, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//

#include "buffer/arc_replacer.h"
#include <optional>
#include "common/config.h"

namespace bustub {

/**
 *
 * TODO(P1): Add implementation
 *
 * @brief a new ArcReplacer, with lists initialized to be empty and target size to 0
 * @param num_frames the maximum number of frames the ArcReplacer will be required to cache
 */
ArcReplacer::ArcReplacer(size_t num_frames) : replacer_size_(num_frames) {
  curr_size_ = 0;
  mru_target_size_ = 0;
}

/**
 * Different Helper functions created in order to ease the tasks
 * of the RecordAccess function that exists in there.
 */

void ArcReplacer::MoveMRUToFrontOfMFU(const std::shared_ptr<FrameStatus> &frame_status) {
  mru_.erase(frame_status->list_iterator_);

  mfu_.push_front(frame_status->frame_id_);

  frame_status->arc_status_ = ArcStatus::MFU;
  frame_status->list_iterator_ = mfu_.begin();
}

void ArcReplacer::MoveMFUToFront(const std::shared_ptr<FrameStatus> &frame_status) {
  mfu_.erase(frame_status->list_iterator_);

  mfu_.push_front(frame_status->frame_id_);

  frame_status->list_iterator_ = mfu_.begin();
}

void ArcReplacer::InsertMRU(frame_id_t frame_id, page_id_t page_id) {
  BUSTUB_ASSERT(alive_map_.find(frame_id) == alive_map_.end(), "InsertMRU: frame_id already alive");
  auto frame_status = std::make_shared<FrameStatus>(page_id, frame_id, false, ArcStatus::MRU);

  mru_.push_front(frame_id);
  frame_status->list_iterator_ = mru_.begin();

  alive_map_[frame_id] = frame_status;
}

void ArcReplacer::InsertMFU(frame_id_t frame_id, page_id_t page_id) {
  BUSTUB_ASSERT(alive_map_.find(frame_id) == alive_map_.end(), "InsertMRU: frame_id already alive");
  auto frame_status = std::make_shared<FrameStatus>(page_id, frame_id, false, ArcStatus::MFU);

  mfu_.push_front(frame_id);
  frame_status->list_iterator_ = mfu_.begin();

  alive_map_[frame_id] = frame_status;
}

void ArcReplacer::RemoveAlive(const std::shared_ptr<FrameStatus> &frame_status) {
  if (frame_status->arc_status_ == ArcStatus::MRU) {
    mru_.erase(frame_status->list_iterator_);
  } else {
    mfu_.erase(frame_status->list_iterator_);
  }

  alive_map_.erase(frame_status->frame_id_);
}

void ArcReplacer::RemoveGhost(const std::shared_ptr<FrameStatus> &frame_status) {
  if (frame_status->arc_status_ == ArcStatus::MRU_GHOST) {
    mru_ghost_.erase(frame_status->list_iterator_);
  } else {
    mfu_ghost_.erase(frame_status->list_iterator_);
  }

  ghost_map_.erase(frame_status->page_id_);
}

void ArcReplacer::IncreaseTargetOnMRUGhostHit() {
  if (mru_ghost_.size() >= mfu_ghost_.size()) {
    mru_target_size_ += 1;
  } else {
    size_t delta = 1;

    if (!mru_ghost_.empty()) {
      delta = mfu_ghost_.size() / mru_ghost_.size();
    }

    mru_target_size_ += delta;
  }
  mru_target_size_ = std::min(replacer_size_, mru_target_size_);
}

void ArcReplacer::DecreaseTargetOnMFUGhostHit() {
  if (mfu_ghost_.size() >= mru_ghost_.size()) {
    if (mru_target_size_ > 0) {
      mru_target_size_ -= 1;
    }
  } else {
    size_t delta = 1;

    if (!mfu_ghost_.empty()) {
      delta = mru_ghost_.size() / mfu_ghost_.size();
    }

    if (delta >= mru_target_size_) {
      mru_target_size_ = 0;
    } else {
      mru_target_size_ -= delta;
    }
  }
}

/**
 * Helper function for Evict() implementation.
 */

auto ArcReplacer::MoveAliveToGhost(const std::shared_ptr<FrameStatus> &frame_status) -> frame_id_t {
  page_id_t page_id = frame_status->page_id_;
  frame_id_t frame_id = frame_status->frame_id_;
  ArcStatus old_status = frame_status->arc_status_;

  RemoveAlive(frame_status);

  auto ghost_it = ghost_map_.find(page_id);
  if (ghost_it != ghost_map_.end()) {
    RemoveGhost(ghost_it->second);
  }

  if (old_status == ArcStatus::MRU) {
    mru_ghost_.push_front(page_id);

    auto ghost = std::make_shared<FrameStatus>(page_id, static_cast<frame_id_t>(-1), true, ArcStatus::MRU_GHOST);

    ghost->list_iterator_ = mru_ghost_.begin();
    BUSTUB_ASSERT(ghost_map_.find(page_id) == ghost_map_.end(),
                  "MoveAliveToGhost: page_id already in ghost_map_ (MRU)");
    ghost_map_[page_id] = ghost;

  } else {
    mfu_ghost_.push_front(page_id);

    auto ghost = std::make_shared<FrameStatus>(page_id, static_cast<frame_id_t>(-1), true, ArcStatus::MFU_GHOST);

    ghost->list_iterator_ = mfu_ghost_.begin();
    BUSTUB_ASSERT(ghost_map_.find(page_id) == ghost_map_.end(),
                  "MoveAliveToGhost: page_id already in ghost_map_ (MFU)");
    ghost_map_[page_id] = ghost;
  }

  return frame_id;
}

auto ArcReplacer::TryEvictMRU() -> std::optional<frame_id_t> {
  for (auto it = mru_.rbegin(); it != mru_.rend(); ++it) {
    auto frame_status = alive_map_.find(*it)->second;

    if (!frame_status->evictable_) {
      continue;
    }

    auto fid = MoveAliveToGhost(frame_status);
    curr_size_--;
    return fid;
  }

  return std::nullopt;
}

auto ArcReplacer::TryEvictMFU() -> std::optional<frame_id_t> {
  for (auto it = mfu_.rbegin(); it != mfu_.rend(); ++it) {
    auto frame_status = alive_map_.find(*it)->second;

    if (!frame_status->evictable_) {
      continue;
    }

    auto fid = MoveAliveToGhost(frame_status);
    curr_size_--;
    return fid;
  }

  return std::nullopt;
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Performs the Replace operation as described by the writeup
 * that evicts from either mfu_ or mru_ into its corresponding ghost list
 * according to balancing policy.
 *
 * If you wish to refer to the original ARC paper, please note that there are
 * two changes in our implementation:
 * 1. When the size of mru_ equals the target size, we don't check
 * the last access as the paper did when deciding which list to evict from.
 * This is fine since the original decision is stated to be arbitrary.
 * 2. Entries that are not evictable are skipped. If all entries from the desired side
 * (mru_ / mfu_) are pinned, we instead try victimize the other side (mfu_ / mru_),
 * and move it to its corresponding ghost list (mfu_ghost_ / mru_ghost_).
 *
 * @return frame id of the evicted frame, or std::nullopt if cannot evict
 */
auto ArcReplacer::Evict() -> std::optional<frame_id_t> {
  std::lock_guard<std::mutex> lock(latch_);

  if (curr_size_ == 0) {
    return std::nullopt;
  }

  if (mru_.size() >= mru_target_size_) {
    auto victim = TryEvictMRU();
    if (victim.has_value()) {
      return victim;
    }
    return TryEvictMFU();
  }

  auto victim = TryEvictMFU();
  if (victim.has_value()) {
    return victim;
  }

  return TryEvictMRU();
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Record access to a frame, adjusting ARC bookkeeping accordingly
 * by bring the accessed page to the front of mfu_ if it exists in any of the lists
 * or the front of mru_ if it does not.
 *
 * Performs the operations EXCEPT REPLACE described in original paper, which is
 * handled by `Evict()`.
 *
 * Consider the following four cases, handle accordingly:
 * 1. Access hits mru_ or mfu_
 * 2/3. Access hits mru_ghost_ / mfu_ghost_
 * 4. Access misses all the lists
 *
 * This routine performs all changes to the four lists as preperation
 * for `Evict()` to simply find and evict a victim into ghost lists.
 *
 * Note that frame_id is used as identifier for alive pages and
 * page_id is used as identifier for the ghost pages, since page_id is
 * the unique identifier to the page after it's dead.
 * Using page_id for alive pages should be the same since it's one to one mapping,
 * but using frame_id is slightly more intuitive.
 *
 * @param frame_id id of frame that received a new access.
 * @param page_id id of page that is mapped to the frame.
 * @param access_type type of access that was received. This parameter is only needed for
 * leaderboard tests.
 */
void ArcReplacer::RecordAccess(frame_id_t frame_id, page_id_t page_id, [[maybe_unused]] AccessType access_type) {
  std::lock_guard<std::mutex> lock(latch_);

  /**
   * Case 1
   * if my frame_id and page_id exists in mru_ or mfu_
   * then simply just move it from of MFU.
   * do we need to do any change in mru_targe_size_ or curr_size_ ?
   * No we don't need to do.
   */

  auto alive_it = alive_map_.find(frame_id);

  if (alive_it != alive_map_.end()) {
    if (alive_it->second->page_id_ == page_id) {
      auto frame_status = alive_it->second;

      if (frame_status->arc_status_ == ArcStatus::MRU) {
        MoveMRUToFrontOfMFU(frame_status);
      } else {
        MoveMFUToFront(frame_status);
      }

      return;
    }
  }

  /**
   * Case 2 & 3
   * Check if page_id --> ghost_map_  ?
   * if yes, check arc status mru_ghost_or mfu_ghost_
   *
   * if mru_ghost_  ----> increment the divider according to condition.
   *                      remove the ghost_status from the ghost_map_
   *                      remove the page_id from the mru_ghost_
   *                      insert into the mfu_  and also the alive map.
   *
   * do we need to check any limit here ?
   * No , we don't need to check limit as we have added frame into alive_map
   *       why ? because we assume here that evict() will be called for full frames_
   *     , we don't need to check limit of mru_ + ghost_mru_ <= replacer_size_ and the global limit.
   *       why ? because we remove and then add.
   *
   * if mfu_ghost_  ----> decrement the divider according to condition.
   *                      remove the ghost_status from the ghost_map_
   *                      remove the page_id from the mru_ghost_
   *                      insert into the mfu_  and also the alive map.
   *
   * do we need to check any limit here ?
   * No , we don't need to check limit as we have added frame into alive_map
   *       why ? because we assume here that evict() will be called for full frames_
   *     , we don't need to check limit of mru_ + ghost_mru_ <= replacer_size_ and the global limit.
   *       why ? because we remove and then add.
   *
   * if no, not in ghost_map_  --> cache miss.
   */

  auto ghost_it = ghost_map_.find(page_id);
  if (ghost_it != ghost_map_.end()) {
    auto ghost_status = ghost_it->second;

    if (ghost_status->arc_status_ == ArcStatus::MRU_GHOST) {
      IncreaseTargetOnMRUGhostHit();

      RemoveGhost(ghost_status);
      BUSTUB_ASSERT(alive_map_.size() < replacer_size_, "alive_map_ exceeded replacer capacity");
      InsertMFU(frame_id, page_id);

      return;
    }

    if (ghost_status->arc_status_ == ArcStatus::MFU_GHOST) {
      DecreaseTargetOnMFUGhostHit();

      RemoveGhost(ghost_status);
      BUSTUB_ASSERT(alive_map_.size() < replacer_size_, "alive_map_ exceeded replacer capacity");
      InsertMFU(frame_id, page_id);

      return;
    }
  }

  /**
   * Case 4
   *
   * if mru_ + mru_ghost_ == replacer_size_ then
   *    simply just remove from mru_ghost_ if it is empty then it must have place in
   *    alive_map
   * else
   *    just check global condition satisfies and then do accordingly.
   *
   */

  if (mru_.size() + mru_ghost_.size() == replacer_size_) {
    if (!mru_ghost_.empty()) {
      page_id_t oldest_ghost_page = mru_ghost_.back();
      auto it = ghost_map_.find(oldest_ghost_page);
      if (it != ghost_map_.end()) {
        RemoveGhost(it->second);
      }
    }
  }

  if (alive_map_.size() + ghost_map_.size() == 2 * replacer_size_) {
    if (!mfu_ghost_.empty()) {
      page_id_t oldest_ghost_page = mfu_ghost_.back();
      auto it = ghost_map_.find(oldest_ghost_page);
      if (it != ghost_map_.end()) {
        RemoveGhost(it->second);
      }
    } else if (!mru_ghost_.empty()) {
      page_id_t oldest_ghost_page = mru_ghost_.back();
      auto it = ghost_map_.find(oldest_ghost_page);
      if (it != ghost_map_.end()) {
        RemoveGhost(it->second);
      }
    }
  }

  InsertMRU(frame_id, page_id);
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Toggle whether a frame is evictable or non-evictable. This function also
 * controls replacer's size. Note that size is equal to number of evictable entries.
 *
 * If a frame was previously evictable and is to be set to non-evictable, then size should
 * decrement. If a frame was previously non-evictable and is to be set to evictable,
 * then size should increment.
 *
 * If frame id is invalid, throw an exception or abort the process.
 *
 * For other scenarios, this function should terminate without modifying anything.
 *
 * @param frame_id id of frame whose 'evictable' status will be modified
 * @param set_evictable whether the given frame is evictable or not
 */
void ArcReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
  std::lock_guard<std::mutex> lock(latch_);

  auto it = alive_map_.find(frame_id);
  if (it == alive_map_.end()) {
    return;
  }

  auto frame_status = it->second;

  if (frame_status->evictable_ != set_evictable) {
    if (set_evictable) {
      curr_size_++;
    } else {
      curr_size_--;
    }
    frame_status->evictable_ = set_evictable;
  }
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Remove an evictable frame from replacer.
 * This function should also decrement replacer's size if removal is successful.
 *
 * Note that this is different from evicting a frame, which always remove the frame
 * decided by the ARC algorithm.
 *
 * If Remove is called on a non-evictable frame, throw an exception or abort the
 * process.
 *
 * If specified frame is not found, directly return from this function.
 *
 * @param frame_id id of frame to be removed
 */
void ArcReplacer::Remove(frame_id_t frame_id) {
  std::lock_guard<std::mutex> lock(latch_);

  auto it = alive_map_.find(frame_id);
  if (it == alive_map_.end()) {
    return;
  }

  auto frame_status = it->second;
  if (!frame_status->evictable_) {
    throw std::runtime_error("thread called upon non-evictable frame.");
  }

  if (frame_status->arc_status_ == ArcStatus::MRU) {
    mru_.erase(frame_status->list_iterator_);
  } else if (frame_status->arc_status_ == ArcStatus::MFU) {
    mfu_.erase(frame_status->list_iterator_);
  }

  alive_map_.erase(it);
  curr_size_--;
}

/**
 * TODO(P1): Add implementation
 *
 * @brief Return replacer's size, which tracks the number of evictable frames.
 *
 * @return size_t
 */
auto ArcReplacer::Size() -> size_t {
  std::lock_guard<std::mutex> lock(latch_);
  return curr_size_;
}

}  // namespace bustub
