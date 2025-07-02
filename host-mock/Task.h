#pragma once

#include "TaskIntf.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <map>
#include <string>

class Task : public TaskIntf {
public:
  class TaskImpl : public TaskIntf::Task {
  public:
    TaskImpl(const std::string& name) : name_(name), running_(false) {}
    
    std::string name_;
    std::atomic<bool> running_;
    std::thread thread_;
  };

  Task();
  ~Task();

  // Start a new task/thread, returns a Task object pointer
  TaskIntf::Task *start(const char *name, void (*taskFunc)(void *), void *arg, int stackSize = 4096, int priority = 1) override;

  // Overload to support std::function, if desired
  TaskIntf::Task *start(const char *name, const std::function<void()> &func, int stackSize = 4096, int priority = 1) override;

  // Stop a running task
  void stop(TaskIntf::Task *task) override;

  // Yield to other tasks
  void yield() override;

  // Destroy task object and release resources
  void destroy(TaskIntf::Task *task) override;

private:
  std::mutex mutex_;
  std::map<TaskIntf::Task*, TaskImpl*> tasks_;
};