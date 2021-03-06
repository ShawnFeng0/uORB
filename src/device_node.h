/****************************************************************************
 *
 *   Copyright (c) 2012-2020 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#pragma once

#include <cerrno>
#include <set>

#include "base/condition_variable.h"
#include "base/intrusive_list.h"
#include "base/mutex.h"
#include "base/rw_mutex.h"
#include "src/callback.h"
#include "uorb/uorb.h"

namespace uORBTest {
class UnitTest;
}

namespace uorb {
class DeviceMaster;

/**
 * Per-object device instance.
 */
class DeviceNode : public ListNode<DeviceNode *> {
  friend DeviceMaster;

 public:
  /* do not allow copying this class */
  // no copy, assignment, move, move assignment
  DeviceNode(const DeviceNode &) = delete;
  DeviceNode &operator=(const DeviceNode &) = delete;
  DeviceNode(DeviceNode &&) = delete;
  DeviceNode &operator=(DeviceNode &&) = delete;

  /**
   * Method to publish a data to this node.
   */
  bool Publish(const void *data);

  void add_subscriber();
  void remove_subscriber();
  uint8_t subscriber_count() const { return subscriber_count_; }
  void mark_anonymous_subscriber() { has_anonymous_subscriber_ = true; }

  void add_publisher();
  void remove_publisher();
  uint8_t publisher_count() const { return publisher_count_; }
  void mark_anonymous_publisher() { has_anonymous_publisher_ = true; }

  // Whether meta and instance are the same as the current one
  inline bool IsSameWith(const orb_metadata &meta, uint8_t instance) const {
    return IsSameWith(meta) && (instance_ == instance);
  }

  inline bool IsSameWith(const orb_metadata &meta) const {
    return &meta_ == &meta;
  }

  // add item to list of work items to schedule on node update
  template <typename Callback>
  bool RegisterCallback(Callback *callback) {
    if (!callback) {
      errno = EINVAL;
      return false;
    }

    uorb::base::WriterLockGuard lg(uorb::DeviceNode::lock_);
    uorb::DeviceNode::callbacks_.emplace(callback);
    return true;
  }

  // remove item from list of work items
  template <typename Callback>
  bool UnregisterCallback(Callback *callback) {
    uorb::base::WriterLockGuard lg(uorb::DeviceNode::lock_);
    return uorb::DeviceNode::callbacks_.erase(callback) != 0;
  }

  // Returns the number of updated data relative to the parameter 'generation'
  unsigned updates_available(unsigned generation) const;
  unsigned initial_generation() const;

  unsigned queue_size() const { return queue_size_; }
  bool set_queue_size(unsigned int queue_size);

  const char *name() const { return meta_.o_name; }
  uint8_t instance() const { return instance_; }

  /**
   * Copies data and the corresponding generation
   * from a node to the buffer provided.
   *
   * @param dst
   *   The buffer into which the data is copied.
   * @param sub_generation
   *   The generation that was copied.
   * @return bool
   *   Returns true if the data was copied.
   */
  bool Copy(void *dst, unsigned *sub_generation) const;

 private:
  friend uORBTest::UnitTest;

  const orb_metadata &meta_; /**< object metadata information */
  const uint8_t instance_;   /**< orb multi instance identifier */

  uint8_t *data_{nullptr}; /**< allocated object buffer */
  uint16_t queue_size_;    /**< maximum number of elements in the queue */
  unsigned generation_{0}; /**< object generation count */

  mutable base::RwMutex lock_{};

  uint8_t subscriber_count_{0};
  bool has_anonymous_subscriber_{false};
  uint8_t publisher_count_{0};
  bool has_anonymous_publisher_{false};

  std::set<detail::CallbackBase *> callbacks_;

  DeviceNode(const struct orb_metadata &meta, uint8_t instance,
             unsigned int queue_size = 1);
  ~DeviceNode();
};
}  // namespace uorb
