#pragma once

#include <functional>

class TaskIntf {
public:
  class Task {
  public:
    virtual ~Task() {}
  };

  virtual ~TaskIntf() {}

  // Start a new task/thread, returns a Task object pointer
  virtual Task *start(const char *name, void (*taskFunc)(void *), void *arg, int stackSize = 4096, int priority = 1) = 0;

  // Overload to support std::function, if desired
  virtual Task *start(const char *name, const std::function<void()> &func, int stackSize = 4096, int priority = 1) = 0;

  // Stop a running task
  virtual void stop(Task *task) = 0;

  // Yield to other tasks
  virtual void yield() = 0;

  // Destroy task object and release resources
  virtual void destroy(Task *task) = 0;
};