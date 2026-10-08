#include "flutter_webrtc/flutter_web_r_t_c_plugin.h"

#include "flutter_common.h"
#include "flutter_webrtc.h"
#include "task_runner_windows.h"
#include "flutter_webrtc_bgra_converter.h"
#include "flutter_webrtc_callback_gate.h"

#include <flutter/plugin_registrar_windows.h>

#include <atomic>
#include <map>
#include <mutex>
#include <new>
#include <vector>

const char* kChannelName = "FlutterWebRTC.Method";
static flutter_webrtc_plugin::FlutterWebRTC* g_shared_instance = nullptr;

namespace {

class BgraFrameSink final
    : public flutter_webrtc_plugin::RTCVideoRenderer<
          flutter_webrtc_plugin::scoped_refptr<
              flutter_webrtc_plugin::RTCVideoFrame>> {
 public:
  BgraFrameSink(
      flutter_webrtc_plugin::scoped_refptr<
          flutter_webrtc_plugin::RTCVideoTrack> track,
      FlutterWebRTCWindowsBgraFrameCallback callback,
      void* context)
      : track_(std::move(track)), callback_(callback), context_(context) {
    track_->AddRenderer(this);
  }

  ~BgraFrameSink() override { Close(); }

  void Close() {
    flutter_webrtc_plugin::scoped_refptr<
        flutter_webrtc_plugin::RTCVideoTrack> track;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if (closed_) return;
      closed_ = true;
      track = std::move(track_);
    }
    if (track) track->RemoveRenderer(this);
    callback_gate_.CloseAndWait();
  }

  void OnFrame(
      flutter_webrtc_plugin::scoped_refptr<
          flutter_webrtc_plugin::RTCVideoFrame> frame) override {
    if (!frame) return;
    if (!callback_gate_.TryEnter()) return;
    flutter_webrtc_plugin::WindowsBgraFrame converted;
    try {
      if (flutter_webrtc_plugin::ConvertWindowsFrameToBgra(frame, &converted)) {
        callback_(context_, converted.bytes.data(), converted.width,
                  converted.height, converted.bytes_per_row, ++frame_id_);
      }
    } catch (...) {
      // A renderer callback must never unwind through libwebrtc.
    }
    callback_gate_.Leave();
  }

 private:
  std::mutex mutex_;
  flutter_webrtc_plugin::scoped_refptr<
      flutter_webrtc_plugin::RTCVideoTrack> track_;
  FlutterWebRTCWindowsBgraFrameCallback callback_;
  void* context_;
  std::atomic<uint64_t> frame_id_{0};
  flutter_webrtc_plugin::WindowsCallbackGate callback_gate_;
  bool closed_ = false;
};

std::mutex g_frame_sinks_mutex;
std::map<uint64_t, std::unique_ptr<BgraFrameSink>> g_frame_sinks;
uint64_t g_next_frame_sink_lease = 1;

void DetachAllBgraFrameSinks() {
  std::map<uint64_t, std::unique_ptr<BgraFrameSink>> sinks;
  {
    std::lock_guard<std::mutex> lock(g_frame_sinks_mutex);
    sinks.swap(g_frame_sinks);
  }
  for (auto& entry : sinks) entry.second->Close();
}

}  // namespace

namespace flutter_webrtc_plugin {

// A webrtc plugin for windows/linux.
class FlutterWebRTCPluginImpl : public FlutterWebRTCPlugin {
 public:
  static void RegisterWithRegistrar(PluginRegistrar* registrar) {
    auto channel = std::make_unique<MethodChannel>(
        registrar->messenger(), kChannelName,
        &flutter::StandardMethodCodec::GetInstance());

    auto* channel_pointer = channel.get();

    // Uses new instead of make_unique due to private constructor.
    std::unique_ptr<FlutterWebRTCPluginImpl> plugin(
        new FlutterWebRTCPluginImpl(registrar, std::move(channel)));
    channel_pointer->SetMethodCallHandler(
        [plugin_pointer = plugin.get()](const auto& call, auto result) {
          plugin_pointer->HandleMethodCall(call, std::move(result));
        });

    registrar->AddPlugin(std::move(plugin));
  }

  virtual ~FlutterWebRTCPluginImpl() {
    DetachAllBgraFrameSinks();
    g_shared_instance = nullptr;
  }

  BinaryMessenger* messenger() { return messenger_; }

  TextureRegistrar* textures() { return textures_; }

  TaskRunner* task_runner() { return task_runner_.get(); }

 private:
  // Creates a plugin that communicates on the given channel.
  FlutterWebRTCPluginImpl(PluginRegistrar* registrar,
                          std::unique_ptr<MethodChannel> channel)
      : channel_(std::move(channel)),
        messenger_(registrar->messenger()),
        textures_(registrar->texture_registrar()),
        task_runner_(std::make_unique<TaskRunnerWindows>()) {
    webrtc_ = std::make_unique<FlutterWebRTC>(this);
    g_shared_instance = webrtc_.get();
  }

  // Called when a method is called on |channel_|;
  void HandleMethodCall(const MethodCall& method_call,
                        std::unique_ptr<MethodResult> result) {
    // handle method call and forward to webrtc native sdk.
    auto method_call_proxy = MethodCallProxy::Create(method_call);
    webrtc_->HandleMethodCall(*method_call_proxy.get(),
                              MethodResultProxy::Create(std::move(result)));
  }

 private:
  std::unique_ptr<MethodChannel> channel_;
  std::unique_ptr<FlutterWebRTC> webrtc_;
  BinaryMessenger* messenger_;
  TextureRegistrar* textures_;
  std::unique_ptr<TaskRunner> task_runner_;
};

}  // namespace flutter_webrtc_plugin


void FlutterWebRTCPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  flutter_webrtc_plugin::FlutterWebRTCPluginImpl::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}

flutter_webrtc_plugin::FlutterWebRTC* FlutterWebRTCPluginSharedInstance() {
  return g_shared_instance;
}

uint64_t FlutterWebRTCWindowsAttachBgraFrameSink(
    const char* track_id,
    const char* peer_connection_id,
    FlutterWebRTCWindowsBgraFrameCallback callback,
    void* context) {
  if (track_id == nullptr || track_id[0] == '\0' || callback == nullptr) {
    return 0;
  }
  std::lock_guard<std::mutex> lock(g_frame_sinks_mutex);
  auto* instance = g_shared_instance;
  if (instance == nullptr) return 0;
  flutter_webrtc_plugin::scoped_refptr<flutter_webrtc_plugin::RTCMediaTrack>
      media_track;
  if (peer_connection_id != nullptr && peer_connection_id[0] != '\0') {
    auto* observer =
        instance->PeerConnectionObserversForId(peer_connection_id);
    if (observer != nullptr) media_track = observer->MediaTrackForId(track_id);
  } else {
    media_track = instance->MediaTrackForId(track_id);
  }
  if (!media_track || media_track->kind().std_string() != "video") {
    return 0;
  }
  auto video_track = flutter_webrtc_plugin::scoped_refptr<
      flutter_webrtc_plugin::RTCVideoTrack>(
      static_cast<flutter_webrtc_plugin::RTCVideoTrack*>(media_track.get()));
  uint64_t lease = g_next_frame_sink_lease++;
  if (lease == 0) lease = g_next_frame_sink_lease++;
  try {
    g_frame_sinks.emplace(
        lease, std::make_unique<BgraFrameSink>(std::move(video_track), callback,
                                               context));
  } catch (...) {
    return 0;
  }
  return lease;
}

void FlutterWebRTCWindowsDetachBgraFrameSink(uint64_t lease) {
  std::unique_ptr<BgraFrameSink> sink;
  {
    std::lock_guard<std::mutex> lock(g_frame_sinks_mutex);
    auto found = g_frame_sinks.find(lease);
    if (found == g_frame_sinks.end()) return;
    sink = std::move(found->second);
    g_frame_sinks.erase(found);
  }
  sink->Close();
}
