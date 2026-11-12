#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

using namespace Jsc::WtfAbi;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF time check failed at line %d\n", line);
        std::abort();
    }
}

template<typename Exception, typename Function> bool Throws(Function function) {
    try {
        function();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

MediaTime Rational(std::int64_t value, std::uint32_t scale, std::uint8_t flags = MediaTimeValid) {
    MediaTime time{};
    time.timeValue = value;
    time.timeScale = scale;
    time.timeFlags = flags;
    return time;
}

MediaTime Double(double value, std::uint8_t flags = MediaTimeValid | MediaTimeDoubleValue) {
    MediaTime time{};
    time.timeValueAsDouble = value;
    time.timeScale = 1;
    time.timeFlags = flags;
    return time;
}

bool NotEqual(const MediaTime& lhs, const MediaTime& rhs) {
    return _ZNK3WTF9MediaTimeneERKS0_(&lhs, &rhs);
}

bool Equal(const MediaTime& lhs, const MediaTime& rhs) {
    return !NotEqual(lhs, rhs) && !NotEqual(rhs, lhs);
}

bool Different(const MediaTime& lhs, const MediaTime& rhs) {
    return NotEqual(lhs, rhs) && NotEqual(rhs, lhs);
}

void TestTimeWithDynamicClockType() {
    const TimeWithDynamicClockType wallOne{1.0, ClockType::Wall};
    const TimeWithDynamicClockType wallTwo{2.0, ClockType::Wall};
    const TimeWithDynamicClockType monotonicOne{1.0, ClockType::Monotonic};
    const TimeWithDynamicClockType monotonicTwo{2.0, ClockType::Monotonic};
    const TimeWithDynamicClockType wallNaN{std::numeric_limits<double>::quiet_NaN(), ClockType::Wall};
    Check(_ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(&wallOne, &wallTwo), __LINE__);
    Check(!_ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(&wallTwo, &wallOne), __LINE__);
    Check(!_ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(&wallOne, &wallOne), __LINE__);
    Check(_ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(&monotonicOne, &monotonicTwo), __LINE__);
    Check(!_ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(&wallNaN, &wallTwo), __LINE__);
    Check(!_ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(&wallOne, &wallNaN), __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(&wallOne, &monotonicTwo); }), __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(&monotonicTwo, &wallOne); }), __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(nullptr, &wallOne); }), __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(&wallOne, nullptr); }), __LINE__);

    const Seconds seconds{1.5};
    const TimeWithDynamicClockType monotonic{2.25, ClockType::Monotonic};
    TimeWithDynamicClockType sum = _ZNK3WTF7SecondsplERKNS_24TimeWithDynamicClockTypeE(&seconds, &monotonic);
    Check(sum.value == 3.75 && sum.type == ClockType::Monotonic, __LINE__);
    const Seconds negative{-4.0};
    sum = _ZNK3WTF7SecondsplERKNS_24TimeWithDynamicClockTypeE(&negative, &wallOne);
    Check(sum.value == -3.0 && sum.type == ClockType::Wall, __LINE__);
    const TimeWithDynamicClockType otherClock{0.5, static_cast<ClockType>(7)};
    sum = _ZNK3WTF7SecondsplERKNS_24TimeWithDynamicClockTypeE(&seconds, &otherClock);
    Check(sum.value == 2.0 && sum.type == static_cast<ClockType>(7), __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF7SecondsplERKNS_24TimeWithDynamicClockTypeE(nullptr, &wallOne); }), __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF7SecondsplERKNS_24TimeWithDynamicClockTypeE(&seconds, nullptr); }), __LINE__);
}

void TestMediaTimeSpecialValues() {
    const MediaTime invalid = Rational(5, 1, 0);
    const MediaTime otherInvalid = Rational(7, 3, MediaTimeHasBeenRounded);
    const MediaTime positiveInfinity = Rational(0, 1, MediaTimeValid | MediaTimePositiveInfinite);
    const MediaTime negativeInfinity = Rational(-1, 1, MediaTimeValid | MediaTimeNegativeInfinite);
    const MediaTime indefinite = Rational(0, 1, MediaTimeValid | MediaTimeIndefinite);
    const MediaTime one = Rational(1, 1);
    Check(Equal(invalid, otherInvalid), __LINE__);
    Check(Different(invalid, one), __LINE__);
    Check(Equal(positiveInfinity, Rational(9, 9, MediaTimeValid | MediaTimePositiveInfinite)), __LINE__);
    Check(Equal(positiveInfinity, Rational(9, 9, MediaTimePositiveInfinite)), __LINE__);
    Check(Equal(negativeInfinity, Rational(3, 3, MediaTimeValid | MediaTimeNegativeInfinite)), __LINE__);
    Check(Equal(indefinite, Rational(3, 3, MediaTimeValid | MediaTimeIndefinite)), __LINE__);
    Check(Equal(positiveInfinity, Rational(0, 1, MediaTimeValid | MediaTimePositiveInfinite | MediaTimeIndefinite)), __LINE__);
    Check(Different(positiveInfinity, negativeInfinity), __LINE__);
    Check(Different(positiveInfinity, indefinite), __LINE__);
    Check(Different(negativeInfinity, indefinite), __LINE__);
    Check(Different(positiveInfinity, one), __LINE__);
    Check(Different(negativeInfinity, one), __LINE__);
    Check(Different(indefinite, one), __LINE__);
    Check(Different(invalid, positiveInfinity), __LINE__);
}

void TestMediaTimeDoubles() {
    Check(Equal(Double(0.5), Double(0.5)), __LINE__);
    Check(Different(Double(0.5), Double(0.25)), __LINE__);
    Check(Equal(Double(0.0), Double(-0.0)), __LINE__);
    Check(Different(Double(std::numeric_limits<double>::quiet_NaN()), Double(std::numeric_limits<double>::quiet_NaN())), __LINE__);
    Check(Equal(Double(0.5), Rational(1, 2)), __LINE__);
    Check(Equal(Double(0.5), Rational(5000000, 10000000)), __LINE__);
    Check(Different(Double(0.5), Rational(1, 3)), __LINE__);
    Check(Equal(Double(0.5, MediaTimeValid | MediaTimeDoubleValue | MediaTimeHasBeenRounded), Rational(2, 4, MediaTimeValid | MediaTimeHasBeenRounded)), __LINE__);
    Check(Different(Double(0.5), Rational(1, 0)), __LINE__);
    Check(Equal(Double(0.5), Rational(0, 0)), __LINE__);
    Check(Equal(Double(std::numeric_limits<double>::quiet_NaN()), Rational(1, 2)), __LINE__);
}

void TestMediaTimeRationals() {
    Check(Equal(Rational(0, 1), Rational(0, 7)), __LINE__);
    Check(Different(Rational(-1, 2), Rational(1, 2)), __LINE__);
    Check(Different(Rational(0, 2), Rational(-1, 2)), __LINE__);
    Check(Equal(Rational(3, 5), Rational(3, 5)), __LINE__);
    Check(Different(Rational(3, 5), Rational(4, 5)), __LINE__);
    Check(Different(Rational(3, 5), Rational(3, 6)), __LINE__);
    Check(Different(Rational(-3, 5), Rational(-3, 6)), __LINE__);
    Check(Equal(Rational(1, 2), Rational(2, 4)), __LINE__);
    Check(Different(Rational(1, 2), Rational(3, 4)), __LINE__);
    Check(Different(Rational(1, 4), Rational(3, 2)), __LINE__);
    Check(Equal(Rational(-1, 2), Rational(-2, 4)), __LINE__);
    Check(Different(Rational(-1, 2), Rational(-3, 4)), __LINE__);
    Check(Different(Rational(-3, 2), Rational(-1, 4)), __LINE__);
    Check(Equal(Rational(3000000, 10000000), Rational(3, 10)), __LINE__);
    Check(Different(Rational(5, 0), Rational(6, 0)), __LINE__);
    Check(Equal(Rational(5, 0), Rational(5, 0)), __LINE__);

    const std::int64_t big = std::int64_t{1} << 61;
    Check(Equal(Rational(3 * big, 3), Rational(2 * big, 2)), __LINE__);
    Check(Different(Rational(3 * big + 1, 3), Rational(2 * big + 1, 2)), __LINE__);
    Check(Different(Rational(3 * big + 1, 3), Rational(2 * (big - 1), 2)), __LINE__);
    Check(Different(Rational(-3 * big - 1, 3), Rational(-2 * big - 1, 2)), __LINE__);
    Check(Equal(Rational(-3 * big, 3), Rational(-2 * big, 2)), __LINE__);
    Check(Different(Rational(-3 * big, 3), Rational(-2 * (big + 1), 2)), __LINE__);

    const MediaTime rhsZeroScale = Rational((std::int64_t{1} << 62), 0);
    const MediaTime lhsLarge = Rational((std::int64_t{1} << 62) + 1, 4);
    Check(Throws<std::domain_error>([&] { NotEqual(lhsLarge, rhsZeroScale); }), __LINE__);
    Check(Throws<std::domain_error>([&] { NotEqual(Rational(std::int64_t{1} << 62, 0), Rational((std::int64_t{1} << 62) + 1, 4)); }), __LINE__);
    const MediaTime remainderLhs = Rational(0x1FFFFFFEFll, 0xFFFFFFFFu);
    const MediaTime remainderRhs = Rational(0x1FFFFFFEEll, 0xFFFFFFFEu);
    Check(Throws<std::overflow_error>([&] { NotEqual(remainderLhs, remainderRhs); }), __LINE__);

    const MediaTime one = Rational(1, 1);
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF9MediaTimeneERKS0_(nullptr, &one); }), __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZNK3WTF9MediaTimeneERKS0_(&one, nullptr); }), __LINE__);
}

} // namespace

int main() {
    TestTimeWithDynamicClockType();
    TestMediaTimeSpecialValues();
    TestMediaTimeDoubles();
    TestMediaTimeRationals();
    return 0;
}
