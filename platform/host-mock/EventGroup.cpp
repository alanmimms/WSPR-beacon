#include "EventGroup.h"

EventGroup::EventGroup() : bits_(0) {}

EventGroup::~EventGroup() {}

uint32_t EventGroup::waitBits(uint32_t bitsToWaitFor, bool clearOnExit, bool waitForAll, uint32_t timeoutMs) {
  std::unique_lock<std::mutex> lock(mutex_);
  
  auto condition = [this, bitsToWaitFor, waitForAll]() {
    if (waitForAll) {
      return (bits_ & bitsToWaitFor) == bitsToWaitFor;
    } else {
      return (bits_ & bitsToWaitFor) != 0;
    }
  };
  
  bool success = true;
  if (timeoutMs == 0xFFFFFFFF) {
    // Wait indefinitely
    cv_.wait(lock, condition);
  } else {
    // Wait with timeout
    success = cv_.wait_for(lock, std::chrono::milliseconds(timeoutMs), condition);
  }
  
  uint32_t result = bits_ & bitsToWaitFor;
  
  if (success && clearOnExit) {
    bits_ &= ~bitsToWaitFor;
  }
  
  return result;
}

uint32_t EventGroup::setBits(uint32_t bitsToSet) {
  std::lock_guard<std::mutex> lock(mutex_);
  bits_ |= bitsToSet;
  cv_.notify_all();
  return bits_;
}

uint32_t EventGroup::clearBits(uint32_t bitsToClear) {
  std::lock_guard<std::mutex> lock(mutex_);
  bits_ &= ~bitsToClear;
  return bits_;
}

uint32_t EventGroup::getBits() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return bits_;
}