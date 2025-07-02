#include "EventGroup.h"

EventGroup::EventGroup() {
  eventGroup_ = xEventGroupCreate();
}

EventGroup::~EventGroup() {
  if (eventGroup_ != nullptr) {
    vEventGroupDelete(eventGroup_);
  }
}

uint32_t EventGroup::waitBits(uint32_t bitsToWaitFor, bool clearOnExit, bool waitForAll, uint32_t timeoutMs) {
  if (eventGroup_ == nullptr) {
    return 0;
  }
  
  TickType_t ticks = (timeoutMs == 0xFFFFFFFF) ? portMAX_DELAY : pdMS_TO_TICKS(timeoutMs);
  
  EventBits_t bits = xEventGroupWaitBits(
    eventGroup_,
    bitsToWaitFor,
    clearOnExit ? pdTRUE : pdFALSE,
    waitForAll ? pdTRUE : pdFALSE,
    ticks
  );
  
  return bits;
}

uint32_t EventGroup::setBits(uint32_t bitsToSet) {
  if (eventGroup_ == nullptr) {
    return 0;
  }
  
  return xEventGroupSetBits(eventGroup_, bitsToSet);
}

uint32_t EventGroup::clearBits(uint32_t bitsToClear) {
  if (eventGroup_ == nullptr) {
    return 0;
  }
  
  return xEventGroupClearBits(eventGroup_, bitsToClear);
}

uint32_t EventGroup::getBits() const {
  if (eventGroup_ == nullptr) {
    return 0;
  }
  
  return xEventGroupGetBits(eventGroup_);
}