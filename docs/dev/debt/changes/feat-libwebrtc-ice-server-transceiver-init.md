# libwebrtc

## add Public source implementations

- [webrtc::RtpTransceiverInit constructor and destructor](../../../core/libs/prx/libwebrtc/src/IceServer.cpp) (libwebrtc) - the destructor throws through `NotImplemented_nid_no_patch` when `send_encodings` holds elements, since the `RtpEncodingParameters` layout is not modelled; not measured on a console. Defaulted in [rtp_transceiver_interface.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/rtp_transceiver_interface.cc), the same at M127, on the 56-byte layout of `rtp_transceiver_interface.h`: `direction` (`kSendRecv`, 0) at 0, `stream_ids` at 8 and `send_encodings` at 32. Otherwise the destructor frees the empty encoding storage and the stream ids. Malformed vectors and a null object throw. Closes when: `RtpEncodingParameters` is modelled and compared with the console.
