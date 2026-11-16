# libwebrtc

## add Public source implementations

- [webrtc::AudioFrame::CopyFrom](../../../core/libs/prx/libwebrtc/src/AudioFrame.cpp) (libwebrtc) - nothing is checked on the console. Adapted from [audio_frame.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/audio/audio_frame.cc), identical at M127, on the 15456-byte layout of the constructor entry. A muted frame copying an unmuted one clears its samples first; the fields and the absolute capture timestamp are copied and the samples copied unless the source is muted. The packet infos reference is taken and the old one released first, with the shared `RtpPacketInfos` release, so a malformed old vector throws before anything changes. More than 7680 samples, which upstream `RTC_CHECK`s, throw after the fields are copied. Null frames throw. Closes when: compared with the console.
