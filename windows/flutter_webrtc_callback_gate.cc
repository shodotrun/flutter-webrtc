#include "flutter_webrtc_callback_gate.h"

namespace flutter_webrtc_plugin {

bool WindowsCallbackGate::TryEnter() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (closed_) return false;
  ++callbacks_in_flight_;
  return true;
}

void WindowsCallbackGate::Leave() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (callbacks_in_flight_ == 0) return;
  --callbacks_in_flight_;
  if (callbacks_in_flight_ == 0) drained_.notify_all();
}

void WindowsCallbackGate::CloseAndWait() {
  std::unique_lock<std::mutex> lock(mutex_);
  closed_ = true;
  drained_.wait(lock, [this] { return callbacks_in_flight_ == 0; });
}

}  // namespace flutter_webrtc_plugin
