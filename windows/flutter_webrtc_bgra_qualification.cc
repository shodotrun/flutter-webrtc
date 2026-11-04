#include "flutter_webrtc_bgra_converter.h"
#include "flutter_webrtc_callback_gate.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <future>
#include <iostream>
#include <thread>

#define CHECK(condition)                                                     \
  do {                                                                       \
    if (!(condition)) {                                                      \
      std::cerr << "CHECK failed: " #condition << " at " << __FILE__ << ':' \
                << __LINE__ << std::endl;                                    \
      std::exit(1);                                                          \
    }                                                                        \
  } while (false)

int main() {
  // BT.601 limited-range I420: a chroma-aligned 2x2 red block followed by a
  // 2x2 blue block. Conversion rounding may differ by one code value, while
  // channel order and alpha must be exact.
  const uint8_t y[] = {82, 82, 41, 41, 82, 82, 41, 41};
  const uint8_t u[] = {90, 240};
  const uint8_t v[] = {240, 110};
  auto frame = libwebrtc::RTCVideoFrame::Create(4, 2, y, 4, u, 2, v, 2);
  CHECK(frame.get() != nullptr);

  // Lock the installed libwebrtc DLL contract independently of the adapter.
  // ConvertToARGB returns bytes written, and kARGB is BGRA in memory on
  // little-endian Windows. kABGR would swap red and blue for our texture.
  uint8_t raw_argb[32] = {};
  CHECK(frame->ConvertToARGB(libwebrtc::RTCVideoFrame::Type::kARGB,
                             raw_argb, 16, 4, 2) == 32);
  CHECK(raw_argb[0] <= 2 && raw_argb[1] <= 2 && raw_argb[2] >= 253 &&
        raw_argb[3] == 255);
  CHECK(raw_argb[8] >= 253 && raw_argb[9] <= 2 && raw_argb[10] <= 2 &&
        raw_argb[11] == 255);
  uint8_t raw_abgr[32] = {};
  CHECK(frame->ConvertToARGB(libwebrtc::RTCVideoFrame::Type::kABGR,
                             raw_abgr, 16, 4, 2) == 32);
  CHECK(raw_abgr[0] >= 253 && raw_abgr[1] <= 2 && raw_abgr[2] <= 2 &&
        raw_abgr[3] == 255);
  CHECK(raw_abgr[8] <= 2 && raw_abgr[9] <= 2 && raw_abgr[10] >= 253 &&
        raw_abgr[11] == 255);

  flutter_webrtc_plugin::WindowsBgraFrame converted;
  CHECK(!flutter_webrtc_plugin::ConvertWindowsFrameToBgra(
      libwebrtc::scoped_refptr<libwebrtc::RTCVideoFrame>{}, &converted));
  CHECK(!flutter_webrtc_plugin::ConvertWindowsFrameToBgra(frame, nullptr));
  CHECK(flutter_webrtc_plugin::ConvertWindowsFrameToBgra(frame, &converted));
  CHECK(converted.width == 4);
  CHECK(converted.height == 2);
  CHECK(converted.bytes_per_row == 16);
  CHECK(converted.bytes.size() == 32);
  for (int row = 0; row < 2; ++row) {
    for (int column = 0; column < 4; ++column) {
      const size_t offset = static_cast<size_t>(row * 16 + column * 4);
      const uint8_t b = converted.bytes[offset];
      const uint8_t g = converted.bytes[offset + 1];
      const uint8_t r = converted.bytes[offset + 2];
      const uint8_t a = converted.bytes[offset + 3];
      CHECK(a == 255);
      CHECK(g <= 2);
      if (column < 2) {
        CHECK(b <= 2);
        CHECK(r >= 253);
      } else {
        CHECK(b >= 253);
        CHECK(r <= 2);
      }
    }
  }

  flutter_webrtc_plugin::WindowsCallbackGate gate;
  CHECK(gate.TryEnter());
  std::atomic<bool> detach_completed{false};
  auto detach = std::async(std::launch::async, [&] {
    gate.CloseAndWait();
    detach_completed = true;
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(25));
  CHECK(!detach_completed.load());
  gate.Leave();
  CHECK(detach.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
  CHECK(detach_completed.load());
  CHECK(!gate.TryEnter());
  std::cout << "flutter_webrtc BGRA qualification PASS" << std::endl;
  return 0;
}
