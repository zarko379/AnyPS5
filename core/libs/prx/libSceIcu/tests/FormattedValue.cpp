#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libSceIcu/tests/RecoveredSignatureChecks.hpp"
#include <array>
#include <cstdint>
#include <type_traits>

namespace UfmtvalNextPosition {

using IcuRecoveredTest::Require;
using IcuRecoveredTest::RequireThrows;

static_assert(std::is_same_v<decltype(&ufmtval_nextPosition_nid_postfix), std::int8_t (APS5_VABI*)(const UFormattedValue*, UConstrainedFieldPosition*, std::int32_t*)>);

namespace {

constexpr std::int32_t IllegalArgument = 1;

constexpr std::uint32_t FillerWord = 0xa5a5a5a5;

std::array<std::uint32_t, 4> Filler() {
    std::array<std::uint32_t, 4> storage{};
    storage.fill(FillerWord);
    return storage;
}

bool Unchanged(const std::array<std::uint32_t, 4>& storage) {
    for (const auto word : storage) {
        if (word != FillerWord) return false;
    }
    return true;
}

} // namespace

void Run() {
    const std::array<std::int32_t, 7> failing{1, 2, 7, 15, 16, 27, 0x7fffffff};
    const std::array<std::int32_t, 4> passing{0, -128, -124, -1};
    auto valueStorage = Filler();
    auto positionStorage = Filler();
    const auto* value = reinterpret_cast<const UFormattedValue*>(valueStorage.data());
    auto* position = reinterpret_cast<UConstrainedFieldPosition*>(positionStorage.data());

    for (const auto incoming : failing) {
        std::int32_t status = incoming;
        Require(ufmtval_nextPosition_nid_postfix(nullptr, nullptr, &status) == 0);
        Require(status == incoming);

        status = incoming;
        Require(ufmtval_nextPosition_nid_postfix(value, position, &status) == 0);
        Require(status == incoming);
        Require(Unchanged(valueStorage) && Unchanged(positionStorage));
    }

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        Require(ufmtval_nextPosition_nid_postfix(nullptr, position, &status) == 0);
        Require(status == IllegalArgument);
        Require(Unchanged(positionStorage));

        status = incoming;
        Require(ufmtval_nextPosition_nid_postfix(value, nullptr, &status) == 0);
        Require(status == IllegalArgument);
        Require(Unchanged(valueStorage));
    }

    RequireThrows([&] { ufmtval_nextPosition_nid_postfix(nullptr, nullptr, nullptr); });

    for (const auto incoming : passing) {
        std::int32_t status = incoming;
        RequireThrows([&] { ufmtval_nextPosition_nid_postfix(value, position, &status); });
        Require(status == incoming);
        Require(Unchanged(valueStorage) && Unchanged(positionStorage));
    }
}

} // namespace UfmtvalNextPosition

void CheckIcuFormattedValue() {
    UfmtvalNextPosition::Run();
}
