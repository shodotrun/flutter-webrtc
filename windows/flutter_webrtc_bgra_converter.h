#ifndef FLUTTER_WEBRTC_WINDOWS_BGRA_CONVERTER_H_
#define FLUTTER_WEBRTC_WINDOWS_BGRA_CONVERTER_H_

#include "rtc_video_frame.h"

#include <cstdint>
#include <vector>

namespace flutter_webrtc_plugin {

struct WindowsBgraFrame {
  uint32_t width = 0;
  uint32_t height = 0;
  uint32_t bytes_per_row = 0;
  std::vector<uint8_t> bytes;
};

bool ConvertWindowsFrameToBgra(
    libwebrtc::scoped_refptr<libwebrtc::RTCVideoFrame> frame,
    WindowsBgraFrame* output);

}  // namespace flutter_webrtc_plugin

#endif  // FLUTTER_WEBRTC_WINDOWS_BGRA_CONVERTER_H_
