#include "prx/libSceIcu/Bidi.hpp"
#include <unicode/ubidi.h>
#include <unicode/uchar.h>
#include <unicode/udata.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::int32_t Ok = 0;
constexpr std::int32_t IllegalArgument = 1;
constexpr std::int32_t InvalidState = 27;

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "ICU BIDI: %s\n", message);
        std::abort();
    }
}

template<typename TException, typename TAction>
bool Throws(TAction action) {
    try {
        action();
    } catch (const TException&) {
        return true;
    }
    return false;
}

struct Case {
    std::u16string text;
    std::vector<std::uint8_t> embeddings;
};

const std::vector<Case>& Cases() {
    static const std::vector<Case> cases{
        {u"abc \u05D0\u05D1\u05D2 def", {}},
        {u"\u05D0\u05D1 123 \u05D2", {}},
        {u"\u0627\u0644\u0639\u0631\u0628\u064A\u0629 1.5% abc", {}},
        {u"a\u202Bb\u05D0c\u202Cd", {}},
        {u"x\u2067\u05D0 y\u2069z \u2066q\u2069", {}},
        {u"first \u05D0\n\u05D1 second\u2029third", {}},
        {u"   ", {}},
        {u"", {}},
        {u"ab\u05D0\u05D1cd", {0, 0x81, 0, 0, 2, 2}},
        {u"(\u05D0[b]c)", {}},
        {u"\U0001F600 \u05D0\U00010900", {}},
    };
    return cases;
}

void CompareParagraph(const Case& item, std::uint8_t paraLevel, bool terminated) {
    UBiDi* guest = ubidi_open_nid_postfix();
    UBiDi* host = ubidi_open();
    Require(guest && host, "both objects open");
    std::vector<std::uint8_t> guestLevels = item.embeddings;
    std::vector<UBiDiLevel> hostLevels(item.embeddings.begin(), item.embeddings.end());
    const std::int32_t length = terminated ? -1 : static_cast<std::int32_t>(item.text.size());
    std::int32_t guestStatus = Ok;
    UErrorCode hostStatus = U_ZERO_ERROR;
    ubidi_setPara_nid_postfix(guest, item.text.c_str(), length, paraLevel, guestLevels.empty() ? nullptr : guestLevels.data(), &guestStatus);
    ubidi_setPara(host, item.text.c_str(), length, paraLevel, hostLevels.empty() ? nullptr : hostLevels.data(), &hostStatus);
    Require(guestStatus == hostStatus, "setPara statuses match ICU");
    Require(ubidi_getParaLevel_nid_postfix(guest) == ubidi_getParaLevel(host), "paragraph levels match ICU");
    guestStatus = Ok;
    hostStatus = U_ZERO_ERROR;
    const std::int32_t runs = ubidi_countRuns_nid_postfix(guest, &guestStatus);
    Require(runs == ubidi_countRuns(host, &hostStatus) && guestStatus == hostStatus, "run counts match ICU");
    for (std::int32_t run = -1; run <= runs; ++run) {
        std::int32_t guestStart = -9;
        std::int32_t guestLength = -9;
        std::int32_t hostStart = -9;
        std::int32_t hostLength = -9;
        Require(ubidi_getVisualRun_nid_postfix(guest, run, &guestStart, &guestLength) == ubidi_getVisualRun(host, run, &hostStart, &hostLength) && guestStart == hostStart && guestLength == hostLength, "visual runs match ICU");
    }
    for (std::int32_t position = -1; position <= static_cast<std::int32_t>(item.text.size()); ++position) {
        std::int32_t guestLimit = -9;
        std::int32_t hostLimit = -9;
        std::uint8_t guestLevel = 0xAA;
        UBiDiLevel hostLevel = 0xAA;
        ubidi_getLogicalRun_nid_postfix(guest, position, &guestLimit, &guestLevel);
        ubidi_getLogicalRun(host, position, &hostLimit, &hostLevel);
        Require(guestLimit == hostLimit && guestLevel == hostLevel, "logical runs match ICU");
    }
    Require(guestLevels == std::vector<std::uint8_t>(hostLevels.begin(), hostLevels.end()), "embedding levels are treated as in ICU");
    ubidi_close_nid_postfix(guest);
    ubidi_close(host);
}

void CheckParagraphs() {
    const std::uint8_t levels[] = {0, 1, 2, 61, 125, 126, UBIDI_DEFAULT_LTR, UBIDI_DEFAULT_RTL};
    for (const Case& item : Cases()) {
        for (const std::uint8_t level : levels) {
            CompareParagraph(item, level, false);
            if (item.embeddings.empty()) CompareParagraph(item, level, true);
        }
    }

    UBiDi* bidi = ubidi_open_nid_postfix();
    const std::u16string text = u"abc \u05D0\u05D1\u05D2 def";
    std::int32_t status = Ok;
    ubidi_setPara_nid_postfix(bidi, text.c_str(), -1, UBIDI_DEFAULT_LTR, nullptr, &status);
    Require(status == Ok && ubidi_getParaLevel_nid_postfix(bidi) == 0, "a default paragraph level resolves to the first strong character");
    status = Ok;
    Require(ubidi_countRuns_nid_postfix(bidi, &status) == 3 && status == Ok, "three runs");
    std::int32_t start = 0;
    std::int32_t length = 0;
    Require(ubidi_getVisualRun_nid_postfix(bidi, 1, &start, &length) == UBIDI_RTL && start == 4 && length == 3, "the Hebrew run");
    Require(ubidi_getVisualRun_nid_postfix(bidi, 2, nullptr, nullptr) == UBIDI_LTR, "null run outputs are allowed");
    std::int32_t limit = 0;
    std::uint8_t level = 0;
    ubidi_getLogicalRun_nid_postfix(bidi, 5, &limit, &level);
    Require(limit == 7 && level == 1, "the logical run of a Hebrew letter");
    ubidi_getLogicalRun_nid_postfix(bidi, 0, nullptr, nullptr);

    limit = -5;
    ubidi_getLogicalRun_nid_postfix(bidi, 100, &limit, &level);
    Require(limit == -5, "an out-of-range position writes nothing");
    start = -5;
    Require(ubidi_getVisualRun_nid_postfix(bidi, 3, &start, &length) == UBIDI_LTR && start == -5, "an out-of-range run writes nothing");

    status = Ok;
    ubidi_setPara_nid_postfix(bidi, text.c_str(), -1, 126, nullptr, &status);
    Require(status == IllegalArgument, "a paragraph level between 126 and 253 is an illegal argument");
    status = Ok;
    ubidi_setPara_nid_postfix(bidi, nullptr, 0, 0, nullptr, &status);
    Require(status == IllegalArgument, "null text is an illegal argument");
    status = Ok;
    ubidi_setPara_nid_postfix(bidi, text.c_str(), -2, 0, nullptr, &status);
    Require(status == IllegalArgument, "a length below -1 is an illegal argument");
    status = Ok;
    ubidi_setPara_nid_postfix(nullptr, text.c_str(), -1, 0, nullptr, &status);
    Require(status == IllegalArgument, "a null object is an illegal argument in setPara");
    status = InvalidState;
    ubidi_setPara_nid_postfix(bidi, text.c_str(), -1, 0, nullptr, &status);
    Require(status == InvalidState, "a failed status stays in setPara");
    ubidi_setPara_nid_postfix(bidi, text.c_str(), -1, 0, nullptr, nullptr);

    UBiDi* fresh = ubidi_open_nid_postfix();
    status = Ok;
    Require(ubidi_countRuns_nid_postfix(fresh, &status) == -1 && status == InvalidState, "runs before setPara are an invalid state");
    status = Ok;
    Require(ubidi_countRuns_nid_postfix(nullptr, &status) == -1 && status == InvalidState, "runs of a null object are an invalid state");
    Require(ubidi_countRuns_nid_postfix(bidi, nullptr) == -1, "a null run status returns -1 as in ICU");
    status = IllegalArgument;
    Require(ubidi_countRuns_nid_postfix(bidi, &status) == -1 && status == IllegalArgument, "a failed run status stays");
    Require(ubidi_getParaLevel_nid_postfix(nullptr) == 0 && ubidi_getParaLevel_nid_postfix(fresh) == 0, "no paragraph level without a paragraph");
    start = -5;
    Require(ubidi_getVisualRun_nid_postfix(nullptr, 0, &start, &length) == UBIDI_LTR && start == -5, "a null object has no visual runs");
    limit = -5;
    ubidi_getLogicalRun_nid_postfix(fresh, 0, &limit, &level);
    Require(limit == -5, "a fresh object has no logical runs");
    Require(Throws<std::invalid_argument>([&] { ubidi_getLogicalRun_nid_postfix(nullptr, 0, &limit, &level); }), "a null object throws in getLogicalRun");
    ubidi_close_nid_postfix(fresh);
    ubidi_close_nid_postfix(bidi);
    ubidi_close_nid_postfix(nullptr);
}

void CheckBaseDirection() {
    for (const Case& item : Cases()) {
        Require(ubidi_getBaseDirection_nid_postfix(item.text.c_str(), static_cast<std::int32_t>(item.text.size())) == ubidi_getBaseDirection(item.text.c_str(), static_cast<std::int32_t>(item.text.size())), "base directions match ICU");
        Require(ubidi_getBaseDirection_nid_postfix(item.text.c_str(), -1) == ubidi_getBaseDirection(item.text.c_str(), -1), "terminated base directions match ICU");
    }
    Require(ubidi_getBaseDirection_nid_postfix(u"123 \u05D0", -1) == UBIDI_RTL && ubidi_getBaseDirection_nid_postfix(u"123 abc", -1) == UBIDI_LTR && ubidi_getBaseDirection_nid_postfix(u"\u0627", -1) == UBIDI_RTL, "first strong characters");
    Require(ubidi_getBaseDirection_nid_postfix(u"12 \u05D0abc", 4) == UBIDI_RTL && ubidi_getBaseDirection_nid_postfix(u"12 \u05D0abc", 3) == UBIDI_NEUTRAL, "only the given length is read");
    Require(ubidi_getBaseDirection_nid_postfix(u"123", -1) == UBIDI_NEUTRAL && ubidi_getBaseDirection_nid_postfix(u"\u05D0", 0) == UBIDI_NEUTRAL, "no strong character is neutral");
    Require(ubidi_getBaseDirection_nid_postfix(nullptr, 3) == UBIDI_NEUTRAL && ubidi_getBaseDirection_nid_postfix(u"abc", -2) == UBIDI_NEUTRAL, "bad arguments are neutral");
}

void CheckClasses() {
    for (std::int32_t c = -2; c <= 0x110001; ++c) {
        if (ubidi_getClass_nid_postfix(c) != u_charDirection(c)) {
            std::fprintf(stderr, "ICU BIDI: ubidi_getClass differs at %X\n", static_cast<unsigned>(c));
            std::abort();
        }
    }
    Require(ubidi_getClass_nid_postfix(0x05D0) == U_RIGHT_TO_LEFT && ubidi_getClass_nid_postfix(0x0661) == U_ARABIC_NUMBER && ubidi_getClass_nid_postfix(0x2067) == U_RIGHT_TO_LEFT_ISOLATE, "bidi classes");
}

void CheckReorder() {
    const std::vector<std::vector<std::uint8_t>> cases{{0, 0, 1, 1, 2, 2, 1, 0}, {1, 1, 1}, {0, 0}, {2, 3, 4, 3, 2, 1, 0, 5}, {125, 0, 124}, {0x81, 0}};
    for (const auto& levels : cases) {
        std::vector<std::int32_t> guest(levels.size(), -3);
        std::vector<std::int32_t> host(levels.size(), -3);
        ubidi_reorderVisual_nid_postfix(levels.data(), static_cast<std::int32_t>(levels.size()), guest.data());
        ubidi_reorderVisual(levels.data(), static_cast<std::int32_t>(levels.size()), host.data());
        Require(guest == host, "visual reordering matches ICU");
    }
    const std::uint8_t levels[] = {0, 0, 1, 1, 1, 0};
    std::int32_t map[6]{};
    ubidi_reorderVisual_nid_postfix(levels, 6, map);
    Require(map[0] == 0 && map[1] == 1 && map[2] == 4 && map[3] == 3 && map[4] == 2 && map[5] == 5, "an odd run is reversed");
    std::int32_t untouched[2]{-3, -3};
    ubidi_reorderVisual_nid_postfix(nullptr, 2, untouched);
    ubidi_reorderVisual_nid_postfix(levels, 0, untouched);
    Require(untouched[0] == -3 && untouched[1] == -3, "no levels write nothing");
    ubidi_reorderVisual_nid_postfix(levels, 6, nullptr);
}

}

int main() {
    UErrorCode access = U_ZERO_ERROR;
    udata_setFileAccess(UDATA_NO_FILES, &access);
    CheckParagraphs();
    CheckBaseDirection();
    CheckClasses();
    CheckReorder();
    return 0;
}
