#include "Task.h"
#include <iostream>

Task::Task() {}

Task::~Task() {
  std::lock_guard<std::mutex> lock(mutex_);
  // Clean up any remaining tasks
  for (auto& pair : tasks_) {
    if (pair.second->running_) {
      pair.second->running_ = false;
      if (pair.second->thread_.joinable()) {
        pair.second->thread_.join();
      }
    }
    delete pair.second;
  }
}

TaskIntf::Task *Task::start(const char *name, void (*taskFunc)(void *), void *arg, int stackSize, int priority) {
  auto* impl = new TaskImpl(name);
  
  std::lock_guard<std::mutex> lock(mutex_);
  tasks_[impl] = impl;
  
  impl->running_ = true;
  impl->thread_ = std::thread([impl, taskFunc, arg]() {
    taskFunc(arg);
    impl->running_ = false;
  });
  
  return impl;
}

TaskIntf::Task *Task::start(const char *name, const std::function<void()> &func, int stackSize, int priority) {
  auto* impl = new TaskImpl(name);
  
  std::lock_guard<std::mutex> lock(mutex_);
  tasks_[impl] = impl;
  
  impl->running_ = true;
  impl->thread_ = std::thread([impl, func]() {
    func();
    impl->running_ = false;
  });
  
  return impl;
}

void Task::stop(TaskIntf::Task *task) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = tasks_.find(task);
  if (it != tasks_.end()) {
    auto* impl = it->second;
    impl->running_ = false;
    // Note: In a real implementation, we'd need a more sophisticated way to stop threads
    // For now, we just mark it as not running
    std::cout << "Task::stop() called for task: " << impl->name_ << " (mock implementation)" << std::endl;
  }
}

void Task::yield() {
  std::this_thread::yield();
}

void Task::destroy(TaskIntf::Task *task) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = tasks_.find(task);
  if (it != tasks_.end()) {
    auto* impl = it->second;
    if (impl->thread_.joinable()) {
      impl->thread_.join();
    }
    delete impl;
    tasks_.erase(it);
  }
}