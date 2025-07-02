#pragma once

#include "TaskIntf.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <map>
#include <mutex>
#include <string>

class Task : public TaskIntf {
public:
  class TaskImpl : public TaskIntf::Task {
  public:
    TaskImpl(const std::string& name) : name_(name), handle_(nullptr) {}
    
    std::string name_;
    TaskHandle_t handle_;
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
  
  struct TaskWrapper {
    std::function<void()> func;
  };
};