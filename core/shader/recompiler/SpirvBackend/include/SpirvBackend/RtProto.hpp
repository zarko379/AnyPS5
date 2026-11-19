#ifndef CORE_SHADER_RECOMPILER_SPIRVBACKEND_RTPROTO_HPP
#define CORE_SHADER_RECOMPILER_SPIRVBACKEND_RTPROTO_HPP

#include <atomic>
#include <cstdint>

namespace ShaderRecompiler::RtProto {

inline std::atomic<std::uint64_t> tableAddress{0};

}

#endif
