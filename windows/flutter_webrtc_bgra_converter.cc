#include "flutter_webrtc_bgra_converter.h"

#include <limits>
#include <utility>

namespace flutter_webrtc_plugin {
namespace {

constexpr uint32_t kMaximumFrameDimension = 16384;
constexpr uint64_t kMaximumFrameBytes = 64ull * 1024ull * 1024ull;

}  // namespace

bool ConvertWindowsFrameToBgra(scoped_refptr<RTCVideoFrame> frame,
                               WindowsBgraFrame* output) {
  if (!frame || output == nullptr) return false;
  const int width = frame->width();
  const int height = frame->height();
  if (width <= 0 || height <= 0 ||
      width > static_cast<int>(kMaximumFrameDimension) ||
      height > static_cast<int>(kMaximumFrameDimension)) {
    return false;
  }
  const uint64_t stride = static_cast<uint64_t>(width) * 4u;
  const uint64_t byte_count = stride * static_cast<uint64_t>(height);
  if (byte_count == 0 || byte_count > kMaximumFrameBytes ||
      byte_count > std::numeric_limits<size_t>::max()) {
    return false;
  }
  WindowsBgraFrame converted;
  converted.width = static_cast<uint32_t>(width);
  converted.height = static_cast<uint32_t>(height);
  converted.bytes_per_row = static_cast<uint32_t>(stride);
  try {
    converted.bytes.resize(static_cast<size_t>(byte_count));
  } catch (...) {
    return false;
  }
  // The qualification fixture locks the pinned wrapper's kABGR memory contract
  // to the BGRA8 texture format consumed by Flutter and WGPU.
  if (frame->ConvertToARGB(RTCVideoFrame::Type::kABGR, converted.bytes.data(),
                           static_cast<int>(converted.bytes_per_row), width,
                           height) != 0) {
    return false;
  }
  *output = std::move(converted);
  return true;
}

}  // namespace flutter_webrtc_plugin
