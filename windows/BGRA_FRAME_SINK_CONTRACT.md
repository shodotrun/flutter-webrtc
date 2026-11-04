# Windows BGRA frame sink contract

The Windows frame sink converts each libwebrtc video frame to bytes that the
sho.run D3D11 publisher labels `BGRA8_UNORM`. The fork's Windows CMake file
downloads `libwebrtc.zip` from the `flutter-webrtc` v1.4.0 release. The actual
x64 DLL used by the Windows qualification probe showed that
`RTCVideoFrame::ConvertToARGB` returns the number of bytes
written (`width * height * 4`). A successful conversion does not return zero.
On little-endian Windows, `Type::kARGB` writes B, G, R, A bytes; `Type::kABGR`
writes R, G, B, A bytes. The converter therefore requests `kARGB` and accepts
only the exact expected byte count.

`flutter_webrtc_bgra_qualification` calls the installed DLL directly with a
red/blue I420 frame before testing the converter. It checks the return value,
both byte orders, converted color channels, and null inputs. Run this native
target against the same DLL shipped with the app before treating a Windows Beam
pixel result as qualified. The app's Beam integration test then verifies the
receive, D3D11 publication, DX12 import, layer render, and pixel readback path.

This is a Windows frame-conversion fix within the pinned fork. It does not
change the WebRTC-SDK version, the frame-sink ABI, or non-Windows platforms.
Persistence: deliberately neither `.sho` nor `.stage`; frames and sink state
are transient.
