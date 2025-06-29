#pragma once

#include <stdint.h>
#include <functional>

/**
 * Abstract interface for task/thread creation and sleep/yield.
 * For host: can use std::thread or pthreads.
 * For target: wraps FreeRTOS tasks.
 */
class TaskIntf {
public:
  virtual ~TaskIntf() {}

  // Start a new task executing func(arg).
  // Return true on success, false on failure.
  virtual bool startTask(const char *name, std::function<void(void *)> func, void *arg, uint32_t stackSize = 4096, int priority = 5) = 0;

  // Sleep current task for ms milliseconds.
  virtual void sleepMs(uint32_t ms) = 0;

  // Yield the current task.
  virtual void yield() = 0;

  // Optionally: stop current task (terminate)
  // virtual void stopTask() = 0;
};