# libwebrtc

## add Public source implementations

- [cricket::Codec destructor](../../../core/libs/prx/libwebrtc/src/SdpAudioFormat.cpp) (libwebrtc) - nothing is checked on the console. Defaulted in [codec.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/media/base/codec.cc) on the 264-byte layout of `codec.h`, the same members at M127: vtable, type and id, name at 16, clockrate, bitrate and channels, `std::optional<std::string>` packetization at 72, the `InlinedVector` of scalability modes at 120, tx mode at 168, parameter map at 216 and the vector of 80-byte `FeedbackParam`s at 240. Everything is validated first, then freed in reverse member order with the `SdpVideoFormat` helpers; the vtable is left alone. Malformed members or a null codec throw before anything is freed. `CreateVideoCodec` stays unimplemented: its codec id is `kIdNotSet` (-1) at M134 and 0 at M127. Closes when: compared with the console.
