#include "flutter_webrtc_bgra_converter.h"

#include <limits>
#include <utility>

namespace flutter_webrtc_plugin {
namespace {

constexpr uint32_t kMaximumFrameDimension = 16384;
constexpr uint64_t kMaximumFrameBytes = 64ull * 1024ull * 1024ull;

}  // namespace

bool ConvertWindowsFrameToBgra(
    libwebrtc::scoped_refptr<libwebrtc::RTCVideoFrame> frame,
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
      byte_count > (std::numeric_limits<size_t>::max)()) {
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
  // libwebrtc names types after libyuv, whose names describe a little-endian
  // word: kARGB writes B,G,R,A bytes (BGRA8), while kABGR writes R,G,B,A. The
  // qualification fixture locks that byte order to the BGRA8 texture format
  // consumed by Flutter and WGPU. ConvertToARGB returns the bytes written
  // (width * height * 4), not a status code.
  if (frame->ConvertToARGB(libwebrtc::RTCVideoFrame::Type::kARGB,
                           converted.bytes.data(),
                           static_cast<int>(converted.bytes_per_row), width,
                           height) != static_cast<int>(byte_count)) {
    return false;
  }
  *output = std::move(converted);
  return true;
}

}  // namespace flutter_webrtc_plugin
