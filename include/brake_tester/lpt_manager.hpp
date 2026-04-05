#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "brake_tester/interfaces.hpp"

namespace brake_tester {

class LptManager {
public:
  LptManager(std::unique_ptr<ILptListener> listener,
             std::unique_ptr<IPrnPatcher> patcher,
             std::unique_ptr<IPrnRenderer> renderer,
             std::unique_ptr<IRenderedDocumentWriter> writer)
      : listener_(std::move(listener)),
        patcher_(std::move(patcher)),
        renderer_(std::move(renderer)),
        writer_(std::move(writer)) {}

  ~LptManager() {
    stop();
  }

  void start() {
    if (running_.exchange(true)) {
      return;
    }

    worker_ = std::thread([this] {
      while (running_) {
        auto incoming_bytes = listener_->captureTransmission();
        if (incoming_bytes.empty()) {
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
          continue;
        }

        auto patched = patcher_->patch(incoming_bytes);
        auto pages = renderer_->render(patched);
        writer_->writePages(pages, "capture");
      }
    });
  }

  void stop() {
    if (!running_.exchange(false)) {
      return;
    }

    if (worker_.joinable()) {
      worker_.join();
    }
  }

private:
  std::atomic_bool running_{false};
  std::thread worker_;

  std::unique_ptr<ILptListener> listener_;
  std::unique_ptr<IPrnPatcher> patcher_;
  std::unique_ptr<IPrnRenderer> renderer_;
  std::unique_ptr<IRenderedDocumentWriter> writer_;
};

} // namespace brake_tester
