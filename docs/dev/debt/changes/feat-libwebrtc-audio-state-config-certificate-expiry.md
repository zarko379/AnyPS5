# libwebrtc

## change [webrtc::AudioState::Config constructor]

- [webrtc::AudioState::Config constructor and destructor](../../../core/libs/prx/libwebrtc/src/AudioStateConfig.cpp) (libwebrtc) - nothing is checked on the console. [audio_state.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/call/audio_state.cc), whose WebRTC notice and BSD licence head the file, defaults the constructor, so the four `scoped_refptr` members of `audio_state.h` (mixer, processing, device module and async processing factory) start null, and the destructor releases them in reverse order. Each class derives first from `RefCountInterface`, whose `Release` is guest slot 1 at M127 and M134; every slot is checked before any reference is released, and a missing one throws. A null config throws. Closes when: compared with the console.

## add Public source implementations

- [rtc::RTCCertificate::HasExpired](../../../core/libs/prx/libwebrtc/src/RtcCertificate.cpp) (libwebrtc) - nothing is checked on the console. Follows `Expires` and `HasExpired` in [rtc_certificate.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/rtc_base/rtc_certificate.cc), identical at M127: `identity_->certificate()` (guest slot 2 of `SSLIdentity` in `ssl_identity.h`) then `CertificateExpirationTime()` (slot 7 of `SSLCertificate` in `ssl_certificate.h`), both orders the same at M127; -1 counts as expired at 0, other values are seconds times 1000 compared with `now` in milliseconds. A null certificate, identity or returned certificate, or a missing slot, throws. Closes when: compared with the console.
