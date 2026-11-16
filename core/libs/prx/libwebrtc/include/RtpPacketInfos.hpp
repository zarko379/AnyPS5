#pragma once

#include "prx/libwebrtc/include/EncodedImage.hpp"

void WebRtcAddRefPacketInfos(WebRtcRtpPacketInfosData* data);
void WebRtcReleasePacketInfos(WebRtcRtpPacketInfosData* data, const char* message);
