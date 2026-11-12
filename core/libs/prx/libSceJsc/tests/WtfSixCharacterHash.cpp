#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF six character hash check failed at line %d\n", line);
        std::abort();
    }
}

bool Hashes(unsigned hash, const char* expected) {
    const std::array<char, 7> result = _ZN3WTF31integerToSixCharacterHashStringEj(hash);
    return std::memcmp(result.data(), expected, 7) == 0;
}

} // namespace

int main() {
    Check(Hashes(0, "AAAAAA"), __LINE__);
    Check(Hashes(1, "AAAAAB"), __LINE__);
    Check(Hashes(25, "AAAAAZ"), __LINE__);
    Check(Hashes(26, "AAAAAa"), __LINE__);
    Check(Hashes(51, "AAAAAz"), __LINE__);
    Check(Hashes(52, "AAAAA0"), __LINE__);
    Check(Hashes(61, "AAAAA9"), __LINE__);
    Check(Hashes(62, "AAAABA"), __LINE__);
    Check(Hashes(3843, "AAAA99"), __LINE__);
    Check(Hashes(3844, "AAABAA"), __LINE__);
    Check(Hashes(123456789, "AIWAuh"), __LINE__);
    Check(Hashes(0x7FFFFFFFu, "CVUmlB"), __LINE__);
    Check(Hashes(0xFFFFFFFFu, "EqpPMD"), __LINE__);
    return 0;
}
