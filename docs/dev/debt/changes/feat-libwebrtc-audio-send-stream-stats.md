# libwebrtc

## add Public source implementations

- [webrtc::AudioSendStream::Stats destructor](../../../core/libs/prx/libwebrtc/src/SendCodecSpec.cpp) (libwebrtc) - nothing is checked on the console. Defaulted in [audio_send_stream.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/call/audio_send_stream.cc) on the 360-byte layout of `audio_send_stream.h`, the same at M127 (only the optional's spelling differs): the only owning members are `codec_name` at 64 and the `report_block_datas` vector at 328, whose `ReportBlockData` elements are trivially destructible at both revisions, after the 56-byte `ANAStats` at 152 and the 112-byte `AudioProcessingStats` at 208. The destructor frees the vector storage, then a long codec name. A malformed string or vector, or null stats, throws before anything is freed. Closes when: compared with the console.
