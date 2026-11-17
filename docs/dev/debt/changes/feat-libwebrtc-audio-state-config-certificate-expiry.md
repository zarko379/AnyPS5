# libwebrtc

## change [webrtc::AudioState::Config constructor]

- [webrtc::AudioState::Config constructor and destructor](../../../core/libs/prx/libwebrtc/src/AudioStateConfig.cpp) (libwebrtc) - nothing is checked on the console. [audio_state.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/call/audio_state.cc), whose WebRTC notice and BSD licence head the file, defaults the constructor, so the four `scoped_refptr` members of `audio_state.h` (mixer, processing, device module and async processing factory) start null, and the destructor releases them in reverse order. Each class derives first from `RefCountInterface`, whose `Release` is guest slot 1 at M127 and M134; every slot is checked before any reference is released, and a missing one throws. A null config throws. Closes when: compared with the console.
