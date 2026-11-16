# libwebrtc

## add Public source implementations

- [webrtc::AudioSendStream::Config::SendCodecSpec constructor and destructor](../../../core/libs/prx/libwebrtc/src/SendCodecSpec.cpp) (libwebrtc) - not measured on a console. Follows [audio_send_stream.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/call/audio_send_stream.cc) on the 120-byte layout of `audio_send_stream.h`: `payload_type` at 0, the `SdpAudioFormat` at 8, the flags from 88 and three `std::optional<int>` at 92, 100 and 108. M127 adds `transport_cc_enabled` after `nack_enabled`, moving `enable_non_sender_rtt` from 89 to 90 without moving the optionals, so the constructor clears bytes 88 to 90, correct at both revisions. The format is copied and destroyed through the `SdpAudioFormat` copy constructor and destructor, so an invalid format throws before the spec is written. A null spec or format throws. Closes when: the revision is identified and compared with the console.
