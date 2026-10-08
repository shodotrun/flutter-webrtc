#ifndef PLUGINS_FLUTTER_WEBRTC_PLUGIN_CPP_H_
#define PLUGINS_FLUTTER_WEBRTC_PLUGIN_CPP_H_

#include <flutter_plugin_registrar.h>
#include <stdint.h>

#ifdef FLUTTER_PLUGIN_IMPL
#define FLUTTER_PLUGIN_EXPORT __declspec(dllexport)
#else
#define FLUTTER_PLUGIN_EXPORT __declspec(dllimport)
#endif

namespace flutter_webrtc_plugin {
class FlutterWebRTC;
}  // namespace flutter_webrtc_plugin

#if defined(__cplusplus)
extern "C" {
#endif

FLUTTER_PLUGIN_EXPORT void FlutterWebRTCPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar);

FLUTTER_PLUGIN_EXPORT flutter_webrtc_plugin::FlutterWebRTC* FlutterWebRTCPluginSharedInstance();

// Synchronously receives a copied BGRA frame. The byte pointer remains valid
// only for the duration of the callback. Consumers must copy before returning.
typedef void (*FlutterWebRTCWindowsBgraFrameCallback)(
    void* context,
    const uint8_t* bytes,
    uint32_t width,
    uint32_t height,
    uint32_t bytes_per_row,
    uint64_t frame_id);

// Attaches a bounded frame sink to an existing flutter_webrtc video track.
// Returns zero when the plugin/track is unavailable. The returned opaque lease
// owns a track reference until FlutterWebRTCWindowsDetachBgraFrameSink returns.
FLUTTER_PLUGIN_EXPORT uint64_t FlutterWebRTCWindowsAttachBgraFrameSink(
    const char* track_id,
    const char* peer_connection_id,
    FlutterWebRTCWindowsBgraFrameCallback callback,
    void* context);

// Stops delivery and waits for an in-flight callback before returning.
FLUTTER_PLUGIN_EXPORT void FlutterWebRTCWindowsDetachBgraFrameSink(
    uint64_t lease);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif  // PLUGINS_FLUTTER_WEBRTC_PLUGIN_CPP_H_
