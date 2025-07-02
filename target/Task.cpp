#include "Task.h"
#include "esp_log.h"

static const char* TAG = "Task";

Task::Task() {}

Task::~Task() {
  std::lock_guard<std::mutex> lock(mutex_);
  for (auto& pair : tasks_) {
    // Note: FreeRTOS tasks can't be safely deleted from another task
    // This is a limitation of the FreeRTOS task model
    ESP_LOGW(TAG, "Task %s still running at destruction", pair.second->name_.c_str());
    delete pair.second;
  }
}

TaskIntf::Task *Task::start(const char *name, void (*taskFunc)(void *), void *arg, int stackSize, int priority) {
  auto* impl = new TaskImpl(name);
  
  BaseType_t result = xTaskCreate(
    taskFunc,
    name,
    stackSize,
    arg,
    priority,
    &impl->handle_
  );
  
  if (result == pdPASS) {
    std::lock_guard<std::mutex> lock(mutex_);
    tasks_[impl] = impl;
    return impl;
  } else {
    delete impl;
    ESP_LOGE(TAG, "Failed to create task %s", name);
    return nullptr;
  }
}

static void taskFunctionWrapper(void* param) {
  TaskWrapper* wrapper = static_cast<TaskWrapper*>(param);
  if (wrapper && wrapper->func) {
    wrapper->func();
  }
  delete wrapper;
  vTaskDelete(NULL);
}

TaskIntf::Task *Task::start(const char *name, const std::function<void()> &func, int stackSize, int priority) {
  auto* impl = new TaskImpl(name);
  auto* wrapper = new TaskWrapper{func};
  
  BaseType_t result = xTaskCreate(
    taskFunctionWrapper,
    name,
    stackSize,
    wrapper,
    priority,
    &impl->handle_
  );
  
  if (result == pdPASS) {
    std::lock_guard<std::mutex> lock(mutex_);
    tasks_[impl] = impl;
    return impl;
  } else {
    delete wrapper;
    delete impl;
    ESP_LOGE(TAG, "Failed to create task %s", name);
    return nullptr;
  }
}

void Task::stop(TaskIntf::Task *task) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = tasks_.find(task);
  if (it != tasks_.end()) {
    auto* impl = it->second;
    // Note: FreeRTOS doesn't provide a clean way to stop tasks
    // The task should check for a flag and exit on its own
    ESP_LOGW(TAG, "Task::stop() called for %s - task must exit on its own", impl->name_.c_str());
  }
}

void Task::yield() {
  taskYIELD();
}

void Task::destroy(TaskIntf::Task *task) {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = tasks_.find(task);
  if (it != tasks_.end()) {
    auto* impl = it->second;
    // The task should have already exited and deleted itself
    delete impl;
    tasks_.erase(it);
  }
}