# libwebrtc

## add Public source implementations

- [webrtc::AudioReceiveStreamInterface::Config destructor](../../../core/libs/prx/libwebrtc/src/SdpAudioFormat.cpp) (libwebrtc) - nothing is checked on the console; the constructor stays unimplemented because the `CryptoOptions` default it stores differs between M127 and M134. Defaulted in [audio_receive_stream.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/call/audio_receive_stream.cc) on the 160-byte layout of `audio_receive_stream.h`, the same at M127: sync group at 48, the `std::map<int, SdpAudioFormat>` decoder map at 88 (120-byte nodes, format at 40), decoder factory at 112 and decryptor and transformer at 144 and 152. The map tree, every format and each `Release` (guest slot 1 of `RefCountInterface`) are checked first; then references are released and members destroyed in reverse order. Malformed members or a null config throw before anything changes. Closes when: compared with the console.
