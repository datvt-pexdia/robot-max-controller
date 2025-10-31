#include "TaskRunner.h"

#include "NetClient.h"

namespace {
constexpr uint32_t PROGRESS_INTERVAL_MS = 200;
}

TaskRunner::TaskRunner()
    : armState{&arm, std::queue<TaskEnvelope>(), 0},
      neckState{&neck, std::queue<TaskEnvelope>(), 0},
      wheelsState{&wheels, std::queue<TaskEnvelope>(), 0},
      netClient(nullptr) {}

void TaskRunner::setNetClient(NetClient* client) { netClient = client; }

void TaskRunner::tick(uint32_t now) {
  DeviceState* states[3] = {&armState, &neckState, &wheelsState};
  for (DeviceState* state : states) {
    state->device->tick(now);
    if (state->device->isRunning()) {
      if (state->lastProgressSent == 0 || now - state->lastProgressSent >= PROGRESS_INTERVAL_MS) {
        state->lastProgressSent = now;
        if (netClient) {
          netClient->sendProgress(state->device->currentTaskId(), state->device->progress(now), "");
        }
      }
    } else if (state->device->isCompleted(now)) {
      const String taskId = state->device->currentTaskId();
      state->device->finish();
      if (netClient && taskId.length() > 0) {
        netClient->sendDone(taskId);
      }
      startNext(*state, now);
    }
  }
}

void TaskRunner::replaceTasks(const std::vector<TaskEnvelope>& tasks) {
  acceptTasks(tasks, false, millis());
}

void TaskRunner::enqueueTasks(const std::vector<TaskEnvelope>& tasks) {
  acceptTasks(tasks, true, millis());
}

void TaskRunner::cancelDevice(const String& device) {
  uint32_t now = millis();
  DeviceState* state = stateForDevice(device);
  if (!state) {
    return;
  }
  clearDevice(*state, now, "canceled");
}

void TaskRunner::cancelAll(const char* reason) {
  uint32_t now = millis();
  clearDevice(armState, now, reason);
  clearDevice(neckState, now, reason);
  clearDevice(wheelsState, now, reason);
}

void TaskRunner::onDisconnect() {
  cancelAll("connection lost");
}

TaskRunner::DeviceState* TaskRunner::stateForDevice(const String& device) {
  if (device == "arm") return &armState;
  if (device == "neck") return &neckState;
  if (device == "wheels") return &wheelsState;
  return nullptr;
}

void TaskRunner::startNext(DeviceState& state, uint32_t now) {
  if (state.queue.empty()) {
    return;
  }
  TaskEnvelope next = state.queue.front();
  state.queue.pop();
  state.device->startTask(next, now);
  state.lastProgressSent = now;
}

void TaskRunner::clearDevice(DeviceState& state, uint32_t now, const char* reason) {
  if (state.device->isRunning()) {
    const String taskId = state.device->currentTaskId();
    state.device->cancel(now);
    state.device->finish();
    if (netClient && taskId.length() > 0 && reason) {
      netClient->sendError(taskId, reason);
    }
  }
  while (!state.queue.empty()) {
    TaskEnvelope dropped = state.queue.front();
    state.queue.pop();
    if (netClient && reason) {
      netClient->sendError(dropped.taskId, reason);
    }
  }
  state.lastProgressSent = 0;
}

void TaskRunner::acceptTasks(const std::vector<TaskEnvelope>& tasks, bool enqueueMode, uint32_t now) {
  for (const TaskEnvelope& task : tasks) {
    if (netClient) {
      netClient->sendAck(task.taskId);
    }
  }

  std::vector<TaskEnvelope> armTasks;
  std::vector<TaskEnvelope> neckTasks;
  std::vector<TaskEnvelope> wheelsTasks;

  for (const TaskEnvelope& task : tasks) {
    if (task.device == "arm") {
      armTasks.push_back(task);
    } else if (task.device == "neck") {
      neckTasks.push_back(task);
    } else if (task.device == "wheels") {
      wheelsTasks.push_back(task);
    }
  }

  struct Batch {
    DeviceState* state;
    std::vector<TaskEnvelope>* list;
  } batches[3] = {{&armState, &armTasks}, {&neckState, &neckTasks}, {&wheelsState, &wheelsTasks}};

  for (auto& batch : batches) {
    if (batch.list->empty()) continue;
    
    // Đặc biệt cho wheels: nếu đang trong continuous mode, luôn dùng tryUpdate
    const bool isWheels = (batch.state == &wheelsState);
    WheelsDevice* wheelsDevice = isWheels ? static_cast<WheelsDevice*>(batch.state->device) : nullptr;
    
    if (!enqueueMode) {
      clearDevice(*batch.state, now, "replaced");
    }
    for (const TaskEnvelope& task : *batch.list) {
      // Nếu device đang chạy và hỗ trợ tryUpdate, thì ưu tiên cập nhật trực tiếp
      if (batch.state->device->isRunning()) {
        if (batch.state->device->tryUpdate(task, now)) {
          // đã cập nhật inline, không cần xếp hàng
          // Gửi ack ngay vì task đã được xử lý
          if (netClient) {
            netClient->sendDone(task.taskId);
          }
          continue;
        }
      }
      batch.state->queue.push(task);
    }
    if (!batch.state->device->isRunning()) {
      startNext(*batch.state, now);
    }
  }
}
