#ifndef FLUTTER_WEBRTC_WINDOWS_CALLBACK_GATE_H_
#define FLUTTER_WEBRTC_WINDOWS_CALLBACK_GATE_H_

#include <condition_variable>
#include <cstddef>
#include <mutex>

namespace flutter_webrtc_plugin {

// Synchronizes renderer callback entry with detach. RemoveRenderer prevents new
// libwebrtc dispatches; this gate also makes already-dispatched callbacks drain
// before the sink and its callback context are destroyed.
class WindowsCallbackGate {
 public:
  bool TryEnter();
  void Leave();
  void CloseAndWait();

 private:
  std::mutex mutex_;
  std::condition_variable drained_;
  size_t callbacks_in_flight_ = 0;
  bool closed_ = false;
};

}  // namespace flutter_webrtc_plugin

#endif  // FLUTTER_WEBRTC_WINDOWS_CALLBACK_GATE_H_
