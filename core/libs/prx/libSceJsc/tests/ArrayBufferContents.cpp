#include "prx/libSceJsc/libSceJsc.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace {

using Jsc::JscAbi::ArrayBufferContents;

using ConstructFunction = void (APS5_VABI*)(ArrayBufferContents*);
using MoveFunction = void (APS5_VABI*)(ArrayBufferContents*, ArrayBufferContents*);
using AssignFunction = ArrayBufferContents* (APS5_VABI*)(ArrayBufferContents*, ArrayBufferContents*);

static_assert(std::is_same_v<decltype(&_ZN3JSC19ArrayBufferContentsC1Ev), ConstructFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC19ArrayBufferContentsC2Ev), ConstructFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC19ArrayBufferContentsC1EOS0_), MoveFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC19ArrayBufferContentsC2EOS0_), MoveFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC19ArrayBufferContentsaSEOS0_), AssignFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC19ArrayBufferContentsD1Ev), ConstructFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC19ArrayBufferContentsD2Ev), ConstructFunction>);
static_assert(std::is_same_v<decltype(&_ZN3JSC19ArrayBufferContents5clearEv), ConstructFunction>);

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ArrayBufferContents check failed at line %d\n", line);
        std::abort();
    }
}

template <typename Exception, typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

struct FakeTask;

int runs = 0;
void* lastRunData = nullptr;
FakeTask* lastRunTask = nullptr;
int deletes = 0;
FakeTask* lastDeleted = nullptr;

void APS5_VABI TaskComplete(void*) {}

void APS5_VABI TaskDelete(void* task) {
    ++deletes;
    lastDeleted = static_cast<FakeTask*>(task);
}

void APS5_VABI TaskRun(void* task, void* data) {
    ++runs;
    lastRunTask = static_cast<FakeTask*>(task);
    lastRunData = data;
}

void* const taskVtable[3] = {reinterpret_cast<void*>(&TaskComplete), reinterpret_cast<void*>(&TaskDelete), reinterpret_cast<void*>(&TaskRun)};

struct FakeTask {
    void* const* vptr = taskVtable;
    unsigned refCount = 1;
};

struct FakeShared {
    unsigned refCount = 1;
};

static_assert(offsetof(FakeTask, refCount) == 8);

void* Packed(const std::uint8_t (&storage)[6]) {
    std::uint64_t value = 0;
    std::memcpy(&value, storage, 6);
    return reinterpret_cast<void*>(value);
}

void SetPacked(std::uint8_t (&storage)[6], const void* pointer) {
    const auto value = reinterpret_cast<std::uint64_t>(pointer);
    std::memcpy(storage, &value, 6);
}

ArrayBufferContents Make(void* data, unsigned size, FakeTask* task, FakeShared* shared) {
    ArrayBufferContents contents;
    std::memset(&contents, 0, sizeof(contents));
    contents.data = data;
    contents.sizeInBytes = size;
    SetPacked(contents.destructor, task);
    SetPacked(contents.shared, shared);
    return contents;
}

bool IsEmpty(const ArrayBufferContents& contents) {
    return !contents.data && !contents.sizeInBytes && !Packed(contents.destructor) && !Packed(contents.shared);
}

void ResetCounters() {
    runs = 0;
    deletes = 0;
    lastRunData = nullptr;
    lastRunTask = nullptr;
    lastDeleted = nullptr;
}

void TestDefault() {
    for (ConstructFunction construct : {_ZN3JSC19ArrayBufferContentsC1Ev, _ZN3JSC19ArrayBufferContentsC2Ev}) {
        ArrayBufferContents contents;
        std::memset(&contents, 0xA5, sizeof(contents));
        construct(&contents);
        Check(IsEmpty(contents), __LINE__);
        const auto* bytes = reinterpret_cast<const unsigned char*>(&contents);
        for (std::size_t i = 0; i < sizeof(contents); ++i) Check(bytes[i] == 0, __LINE__);
    }
    Check(Throws<std::invalid_argument>([] { _ZN3JSC19ArrayBufferContentsC1Ev(nullptr); }), __LINE__);
}

void TestMoveConstruct() {
    for (MoveFunction move : {_ZN3JSC19ArrayBufferContentsC1EOS0_, _ZN3JSC19ArrayBufferContentsC2EOS0_}) {
        ResetCounters();
        char data[16];
        FakeTask task;
        FakeShared shared;
        shared.refCount = 3;
        ArrayBufferContents source = Make(data, 16, &task, &shared);
        ArrayBufferContents target;
        std::memset(&target, 0xA5, sizeof(target));
        move(&target, &source);
        Check(target.data == data && target.sizeInBytes == 16, __LINE__);
        Check(Packed(target.destructor) == &task && Packed(target.shared) == &shared, __LINE__);
        Check(IsEmpty(source), __LINE__);
        Check(task.refCount == 1 && shared.refCount == 3 && runs == 0 && deletes == 0, __LINE__);
    }

    ArrayBufferContents empty = Make(nullptr, 0, nullptr, nullptr);
    ArrayBufferContents target;
    _ZN3JSC19ArrayBufferContentsC1EOS0_(&target, &empty);
    Check(IsEmpty(target) && IsEmpty(empty), __LINE__);

    ArrayBufferContents huge = Make(nullptr, 0x80000000u, nullptr, nullptr);
    Check(Throws<std::length_error>([&] { _ZN3JSC19ArrayBufferContentsC1EOS0_(&target, &huge); }), __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZN3JSC19ArrayBufferContentsC1EOS0_(&target, nullptr); }), __LINE__);
}

void TestAssign() {
    ResetCounters();
    char oldData[8];
    char newData[32];
    FakeTask oldTask;
    FakeTask newTask;
    FakeShared oldShared;
    oldShared.refCount = 2;
    FakeShared newShared;
    ArrayBufferContents target = Make(oldData, 8, &oldTask, &oldShared);
    ArrayBufferContents source = Make(newData, 32, &newTask, &newShared);
    Check(_ZN3JSC19ArrayBufferContentsaSEOS0_(&target, &source) == &target, __LINE__);
    Check(runs == 1 && lastRunTask == &oldTask && lastRunData == oldData, __LINE__);
    Check(deletes == 1 && lastDeleted == &oldTask && oldTask.refCount == 1, __LINE__);
    Check(oldShared.refCount == 1, __LINE__);
    Check(target.data == newData && target.sizeInBytes == 32, __LINE__);
    Check(Packed(target.destructor) == &newTask && Packed(target.shared) == &newShared, __LINE__);
    Check(newTask.refCount == 1 && newShared.refCount == 1, __LINE__);
    Check(IsEmpty(source), __LINE__);

    ResetCounters();
    FakeTask sharedTask;
    sharedTask.refCount = 2;
    ArrayBufferContents keep = Make(oldData, 8, &sharedTask, nullptr);
    ArrayBufferContents empty = Make(nullptr, 0, nullptr, nullptr);
    _ZN3JSC19ArrayBufferContentsaSEOS0_(&keep, &empty);
    Check(runs == 1 && deletes == 0 && sharedTask.refCount == 1, __LINE__);
    Check(IsEmpty(keep), __LINE__);

    ResetCounters();
    FakeShared last;
    ArrayBufferContents lastHolder = Make(oldData, 8, nullptr, &last);
    ArrayBufferContents other = Make(newData, 32, nullptr, nullptr);
    Check(Throws<std::runtime_error>([&] { _ZN3JSC19ArrayBufferContentsaSEOS0_(&lastHolder, &other); }), __LINE__);
    Check(lastHolder.data == oldData && Packed(lastHolder.shared) == &last && last.refCount == 1, __LINE__);
    Check(other.data == newData, __LINE__);

    Check(Throws<std::invalid_argument>([&] { _ZN3JSC19ArrayBufferContentsaSEOS0_(nullptr, &other); }), __LINE__);
    Check(Throws<std::invalid_argument>([&] { _ZN3JSC19ArrayBufferContentsaSEOS0_(&other, nullptr); }), __LINE__);
}

void TestDestroyAndClear() {
    for (ConstructFunction destroy : {_ZN3JSC19ArrayBufferContentsD1Ev, _ZN3JSC19ArrayBufferContentsD2Ev}) {
        ResetCounters();
        char data[4];
        FakeTask task;
        FakeShared shared;
        shared.refCount = 2;
        ArrayBufferContents contents = Make(data, 4, &task, &shared);
        destroy(&contents);
        Check(runs == 1 && lastRunData == data && deletes == 1 && lastDeleted == &task, __LINE__);
        Check(shared.refCount == 1, __LINE__);

        ResetCounters();
        ArrayBufferContents empty = Make(nullptr, 0, nullptr, nullptr);
        destroy(&empty);
        Check(runs == 0 && deletes == 0, __LINE__);

        FakeShared last;
        ArrayBufferContents lastHolder = Make(data, 4, &task, &last);
        task.refCount = 1;
        Check(Throws<std::runtime_error>([&] { destroy(&lastHolder); }), __LINE__);
        Check(runs == 0 && last.refCount == 1, __LINE__);
        Check(Throws<std::invalid_argument>([&] { destroy(nullptr); }), __LINE__);
    }

    ResetCounters();
    char data[4];
    FakeTask task;
    task.refCount = 3;
    FakeShared shared;
    shared.refCount = 4;
    ArrayBufferContents contents = Make(data, 4, &task, &shared);
    _ZN3JSC19ArrayBufferContents5clearEv(&contents);
    Check(runs == 1 && deletes == 0 && task.refCount == 2 && shared.refCount == 3, __LINE__);
    Check(IsEmpty(contents), __LINE__);
    _ZN3JSC19ArrayBufferContents5clearEv(&contents);
    Check(runs == 1 && IsEmpty(contents), __LINE__);

    FakeShared last;
    ArrayBufferContents lastHolder = Make(data, 4, nullptr, &last);
    Check(Throws<std::runtime_error>([&] { _ZN3JSC19ArrayBufferContents5clearEv(&lastHolder); }), __LINE__);
    Check(lastHolder.data == data && last.refCount == 1, __LINE__);
    Check(Throws<std::invalid_argument>([] { _ZN3JSC19ArrayBufferContents5clearEv(nullptr); }), __LINE__);
}

void TestSelfAssign() {
    ResetCounters();
    char data[4];
    FakeTask task;
    task.refCount = 2;
    FakeShared shared;
    shared.refCount = 2;
    ArrayBufferContents contents = Make(data, 4, &task, &shared);
    _ZN3JSC19ArrayBufferContentsaSEOS0_(&contents, &contents);
    Check(IsEmpty(contents), __LINE__);
    Check(runs == 1 && task.refCount == 1 && shared.refCount == 1 && deletes == 0, __LINE__);
}

}

int main() {
    TestDefault();
    TestMoveConstruct();
    TestAssign();
    TestDestroyAndClear();
    TestSelfAssign();
    std::puts("ArrayBufferContents tests passed");
    return 0;
}
