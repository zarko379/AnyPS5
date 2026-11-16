# libwebrtc

## add Public source implementations

- [webrtc::VideoDecoderFactory::QueryCodecSupport](../../../core/libs/prx/libwebrtc/src/VideoDecoderFactory.cpp) (libwebrtc) - not measured on a console, and an H265 match throws as `SdpVideoFormat::IsCodecInList` does. The default implementation of [video_decoder_factory.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/api/video_codecs/video_decoder_factory.cc), identical at M127: with `reference_scaling` it returns unsupported without calling the factory; otherwise it calls `GetSupportedFormats` (guest slot 2 of `video_decoder_factory.h`, after the two destructor entries) through the hidden result pointer, checks the format with `IsCodecInList`, destroys the returned formats and frees the vector. The two-byte `CodecSupport` returns in a register. A null factory or format, a missing slot or a malformed vector throws. Closes when: compared with the console.
