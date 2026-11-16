# libwebrtc

## add Public source implementations

- [webrtc::RTCStatsReport::begin, end and the ConstIterator operators and destructor](../../../core/libs/prx/libwebrtc/src/RtcStatsReport.cpp) (libwebrtc) - not measured on a console. Adapted from [rtc_stats_report.cc](https://webrtc.googlesource.com/src/+/8d78f5de6c27b2c793039989ea381f1428fb0100/stats/rtc_stats_report.cc), identical at M127, on the 40-byte layout of `rtc_stats_report.h` (count at 0, libc++ `std::map` at 16, 80-byte nodes, stats pointer at 72) and the 16-byte iterator. `begin`/`end` add a reference and return through the hidden result pointer. Dropping the last reference deletes as libc++ does: stats through guest slot 1 (`~RTCStats` is the first virtual), ids, nodes in post-order, then the report. A malformed map or stats without slot 1 throws and keeps the reference. Incrementing or dereferencing the end, `*` on null stats and null arguments throw. Closes when: compared with the console.
