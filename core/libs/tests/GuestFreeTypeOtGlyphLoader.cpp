#include "prx/libSceFreeTypeOt/include/GlyphLoader.hpp"
#include "prx/libSceFreeTypeOt/include/Validation.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>

namespace {

using FreeType::GuestFaceRec;
using FreeType::GuestGlyphLoaderRec;
using FreeType::GuestMemoryRec;
using FreeType::GuestVector;

struct Recorder {
    GuestMemoryRec memory{};
    int calls = 0;
    int allocs = 0;
    int frees = 0;
    int reallocs = 0;
    int failAt = 0;
    std::int64_t lastSize = 0;
    std::int64_t lastCurrent = 0;
    void* lastFreed = nullptr;
};

Recorder* Owner(GuestMemoryRec* memory) {
    return static_cast<Recorder*>(memory->user);
}

bool Fails(Recorder* recorder) {
    ++recorder->calls;
    return recorder->failAt == recorder->calls;
}

void* APS5_VABI RecordAlloc(GuestMemoryRec* memory, std::int64_t size) {
    auto* recorder = Owner(memory);
    ++recorder->allocs;
    recorder->lastSize = size;
    if (Fails(recorder)) return nullptr;
    void* block = std::malloc(static_cast<std::size_t>(size));
    std::memset(block, 0xAA, static_cast<std::size_t>(size));
    return block;
}

void APS5_VABI RecordFree(GuestMemoryRec* memory, void* block) {
    auto* recorder = Owner(memory);
    ++recorder->frees;
    recorder->lastFreed = block;
    std::free(block);
}

void* APS5_VABI RecordRealloc(GuestMemoryRec* memory, std::int64_t current, std::int64_t size, void* block) {
    auto* recorder = Owner(memory);
    ++recorder->reallocs;
    recorder->lastCurrent = current;
    recorder->lastSize = size;
    if (Fails(recorder)) return nullptr;
    auto* resized = static_cast<unsigned char*>(std::realloc(block, static_cast<std::size_t>(size)));
    if (size > current) std::memset(resized + current, 0xAA, static_cast<std::size_t>(size - current));
    return resized;
}

void Reset(Recorder& recorder) {
    recorder = Recorder{};
    recorder.memory = {&recorder, RecordAlloc, RecordFree, RecordRealloc};
}

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "FreeType Ot glyph loader: %s\n", message);
    std::exit(1);
}

template <typename TCall> bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

bool Zero(const void* block, std::size_t size) {
    const auto* bytes = static_cast<const unsigned char*>(block);
    for (std::size_t i = 0; i < size; ++i) {
        if (bytes[i] != 0) return false;
    }
    return true;
}

GuestGlyphLoaderRec* NewLoader(Recorder& recorder) {
    GuestGlyphLoaderRec* loader = nullptr;
    Require(FT_GlyphLoader_New_nid_postfix(&recorder.memory, &loader) == 0 && loader, "New succeeds");
    return loader;
}

void Creation() {
    Recorder recorder;
    Reset(recorder);
    GuestGlyphLoaderRec* loader = nullptr;
    Require(FT_GlyphLoader_New_nid_postfix(&recorder.memory, &loader) == 0, "New returns 0");
    Require(loader && recorder.allocs == 1 && recorder.lastSize == 176, "New allocates the 176-byte loader record");
    Require(loader->memory == &recorder.memory, "New stores the memory");
    Require(Zero(reinterpret_cast<unsigned char*>(loader) + 8, sizeof(GuestGlyphLoaderRec) - 8), "New zeroes the rest of the record");
    FT_GlyphLoader_Done_nid_postfix(loader);
    Require(recorder.frees == 1 && recorder.lastFreed == loader, "Done of an empty loader frees only the record");

    Reset(recorder);
    recorder.failAt = 1;
    auto* sentinel = reinterpret_cast<GuestGlyphLoaderRec*>(std::uintptr_t{0x1234});
    loader = sentinel;
    Require(FT_GlyphLoader_New_nid_postfix(&recorder.memory, &loader) == 0x40, "New reports Out_Of_Memory");
    Require(loader == sentinel, "a failed New leaves the output untouched");

    Reset(recorder);
    Require(Throws([&] { FT_GlyphLoader_New_nid_postfix(nullptr, &loader); }), "New with null memory throws");
    Require(Throws([&] { FT_GlyphLoader_New_nid_postfix(&recorder.memory, nullptr); }), "New with a null output throws");
    Require(recorder.allocs == 0, "a throwing New allocates nothing");
    FT_GlyphLoader_Done_nid_postfix(nullptr);
    FT_GlyphLoader_Add_nid_postfix(nullptr);
}

void Points() {
    Recorder recorder;
    Reset(recorder);
    auto* loader = NewLoader(recorder);
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 5, 1) == 0, "CheckPoints grows an empty loader");
    Require(loader->max_points == 8 && loader->max_contours == 4, "points pad to 8 and contours to 4");
    Require(recorder.allocs == 4 && recorder.reallocs == 0, "a fresh loader allocates points, tags and contours");
    Require(loader->base.outline.points && loader->base.outline.tags && loader->base.outline.contours, "the base arrays exist");
    Require(Zero(loader->base.outline.points, 8 * sizeof(GuestVector)) && Zero(loader->base.outline.tags, 8) && Zero(loader->base.outline.contours, 8),
            "new arrays are zeroed");
    Require(loader->current.outline.points == loader->base.outline.points && loader->current.outline.contours == loader->base.outline.contours,
            "current arrays follow an empty base");

    const int calls = recorder.calls;
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 8, 4) == 0 && recorder.calls == calls, "a request within capacity allocates nothing");

    loader->base.outline.n_points = 6;
    loader->base.outline.n_contours = 2;
    loader->base.outline.points[0] = {11, 12};
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 3, 3) == 0, "CheckPoints grows a used loader");
    Require(loader->max_points == 16 && loader->max_contours == 8, "growth pads the sums");
    Require(recorder.reallocs == 3 && recorder.lastCurrent == 8 && recorder.lastSize == 16, "growth reallocates the contours last from 4 to 8 shorts");
    Require(loader->base.outline.points[0].x == 11 && loader->base.outline.points[0].y == 12, "growth keeps points");
    Require(loader->current.outline.points == loader->base.outline.points + 6 && loader->current.outline.tags == loader->base.outline.tags + 6,
            "current points follow the base count");
    Require(loader->current.outline.contours == loader->base.outline.contours + 2, "current contours follow the base count");

    const int before = recorder.calls;
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 0x7FF9, 0) == 0x0A, "points beyond SHRT_MAX after padding are too large");
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 0, 0x7FFD) == 0x0A, "contours beyond SHRT_MAX after padding are too large");
    Require(recorder.calls == before && loader->max_points == 16 && loader->max_contours == 8, "a too-large request changes nothing");
    FT_GlyphLoader_Done_nid_postfix(loader);
    Require(recorder.frees == 4, "Done frees the three arrays and the record");

    Reset(recorder);
    loader = NewLoader(recorder);
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 3, 0x7FFD) == 0x0A, "too many contours after the points grew");
    Require(loader->max_points == 8 && loader->base.outline.points && !loader->current.outline.points, "the early return skips the adjustment");
    FT_GlyphLoader_Done_nid_postfix(loader);

    Reset(recorder);
    loader = NewLoader(recorder);
    recorder.failAt = 3;
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 1, 1) == 0x40, "a failing tag allocation reports Out_Of_Memory");
    Require(loader->base.outline.points && !loader->base.outline.tags && !loader->base.outline.contours, "the points stay allocated");
    Require(loader->max_points == 0 && loader->max_contours == 0, "the maxima stay unchanged");
    FT_GlyphLoader_Done_nid_postfix(loader);
    Require(recorder.frees == 2, "Done frees the points and the record");

    Reset(recorder);
    loader = NewLoader(recorder);
    recorder.failAt = 4;
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 1, 1) == 0x40, "a failing contour allocation reports Out_Of_Memory");
    Require(loader->max_points == 8 && loader->max_contours == 0 && !loader->current.outline.points, "the failure skips the adjustment");
    FT_GlyphLoader_Done_nid_postfix(loader);
}

void Extra() {
    Recorder recorder;
    Reset(recorder);
    auto* loader = NewLoader(recorder);
    Require(FT_GlyphLoader_CreateExtra_nid_postfix(loader) == 0, "CreateExtra on an empty loader succeeds");
    Require(loader->use_extra == 1 && !loader->base.extra_points && !loader->base.extra_points2 && recorder.calls == 1,
            "an empty loader enables extra points without allocating");
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 3, 0) == 0, "CheckPoints grows the extra points");
    Require(loader->base.extra_points && loader->base.extra_points2 == loader->base.extra_points + 8, "the second extra table follows max_points");
    for (int i = 0; i < 8; ++i) {
        loader->base.extra_points[i] = {i, -i};
        loader->base.extra_points2[i] = {100 + i, 200 + i};
    }
    loader->base.outline.n_points = 5;
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 6, 0) == 0, "CheckPoints grows the extra points again");
    Require(loader->max_points == 16 && loader->base.extra_points2 == loader->base.extra_points + 16, "the second table moves to the new maximum");
    bool kept = true;
    for (int i = 0; i < 8; ++i) {
        kept = kept && loader->base.extra_points[i].x == i && loader->base.extra_points2[i].x == 100 + i && loader->base.extra_points2[i].y == 200 + i;
    }
    Require(kept, "both extra tables keep their points");
    Require(loader->current.extra_points == loader->base.extra_points + 5 && loader->current.extra_points2 == loader->base.extra_points2 + 5,
            "current extra points follow the base count");
    FT_GlyphLoader_Reset_nid_postfix(loader);
    Require(recorder.frees == 3 && !loader->base.extra_points && !loader->base.extra_points2 && loader->use_extra == 1, "Reset frees the extra points");
    Require(!loader->current.extra_points2 && loader->max_points == 0, "Reset clears the second extra table");
    FT_GlyphLoader_Done_nid_postfix(loader);
    Require(recorder.frees == 4, "Done frees the record");

    Reset(recorder);
    loader = NewLoader(recorder);
    loader->max_points = 4;
    Require(FT_GlyphLoader_CreateExtra_nid_postfix(loader) == 0 && recorder.lastSize == 128, "CreateExtra allocates twice max_points");
    Require(Zero(loader->base.extra_points, 128) && loader->base.extra_points2 == loader->base.extra_points + 4, "the extra tables are zeroed and split");
    loader->max_points = 0;
    FT_GlyphLoader_Done_nid_postfix(loader);

    Reset(recorder);
    loader = NewLoader(recorder);
    loader->max_points = 4;
    recorder.failAt = 2;
    Require(FT_GlyphLoader_CreateExtra_nid_postfix(loader) == 0x40, "a failing CreateExtra reports Out_Of_Memory");
    Require(!loader->use_extra && !loader->base.extra_points, "a failing CreateExtra leaves extra points off");
    loader->max_points = 0;
    FT_GlyphLoader_Done_nid_postfix(loader);
}

void Stack() {
    Recorder recorder;
    Reset(recorder);
    auto* loader = NewLoader(recorder);
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 8, 4) == 0, "capacity for the stack");
    Require(FT_GlyphLoader_CheckSubGlyphs_nid_postfix(loader, 3) == 0, "CheckSubGlyphs grows");
    Require(loader->max_subglyphs == 4 && recorder.lastSize == 192 && loader->base.subglyphs, "subglyphs pad to 2 of 48 bytes");
    Require(loader->current.subglyphs == loader->base.subglyphs, "current subglyphs follow an empty base");
    loader->base.num_subglyphs = 1;
    loader->current.num_subglyphs = 2;
    Require(FT_GlyphLoader_CheckSubGlyphs_nid_postfix(loader, 1) == 0 && loader->max_subglyphs == 4, "a request within capacity keeps the maximum");
    Require(FT_GlyphLoader_CheckSubGlyphs_nid_postfix(loader, 2) == 0 && loader->max_subglyphs == 6, "a larger request grows");
    Require(loader->current.subglyphs == loader->base.subglyphs + 1, "current subglyphs follow the base count");
    recorder.failAt = recorder.calls + 1;
    Require(FT_GlyphLoader_CheckSubGlyphs_nid_postfix(loader, 4) == 0x40 && loader->max_subglyphs == 6, "a failing growth keeps the maximum");
    recorder.failAt = 0;

    loader->base.outline.n_points = 2;
    loader->base.outline.n_contours = 1;
    loader->base.outline.contours[0] = 1;
    FT_GlyphLoader_Prepare_nid_postfix(loader);
    Require(loader->current.outline.n_points == 0 && loader->current.outline.n_contours == 0 && loader->current.num_subglyphs == 0, "Prepare clears current");
    Require(loader->current.outline.points == loader->base.outline.points + 2 && loader->current.outline.contours == loader->base.outline.contours + 1,
            "Prepare follows the base");
    loader->current.outline.n_points = 3;
    loader->current.outline.n_contours = 2;
    loader->current.outline.contours[0] = 0;
    loader->current.outline.contours[1] = 2;
    loader->current.num_subglyphs = 2;
    FT_GlyphLoader_Add_nid_postfix(loader);
    Require(loader->base.outline.n_points == 5 && loader->base.outline.n_contours == 3 && loader->base.num_subglyphs == 3, "Add sums the counts");
    Require(loader->base.outline.contours[0] == 1 && loader->base.outline.contours[1] == 2 && loader->base.outline.contours[2] == 4,
            "Add offsets the current contours by the base points");
    Require(loader->current.outline.n_points == 0 && loader->current.outline.points == loader->base.outline.points + 5, "Add prepares the next glyph");
    Require(loader->current.subglyphs == loader->base.subglyphs + 3, "Add moves the current subglyphs");

    FT_GlyphLoader_Rewind_nid_postfix(loader);
    Require(loader->base.outline.n_points == 0 && loader->base.outline.n_contours == 0 && loader->base.num_subglyphs == 0, "Rewind clears the base");
    Require(std::memcmp(&loader->current, &loader->base, sizeof(loader->base)) == 0, "Rewind copies the base into current");
    Require(loader->current.extra_points == loader->base.extra_points && loader->current.subglyphs == loader->base.subglyphs, "Rewind resets current pointers");

    const int frees = recorder.frees;
    FT_GlyphLoader_Reset_nid_postfix(loader);
    Require(recorder.frees == frees + 4, "Reset frees the allocated tables");
    Require(!loader->base.outline.points && !loader->base.outline.tags && !loader->base.outline.contours && !loader->base.subglyphs, "Reset clears the tables");
    Require(loader->max_points == 0 && loader->max_contours == 0 && loader->max_subglyphs == 0, "Reset clears the maxima");
    Require(!loader->current.outline.points && !loader->current.subglyphs, "Reset rewinds current");
    FT_GlyphLoader_Done_nid_postfix(loader);

    Reset(recorder);
    loader = NewLoader(recorder);
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(loader, 1, 1) == 0, "capacity before an overflowing Add");
    loader->current.outline.n_contours = 0xFFFF;
    Require(Throws([&] { FT_GlyphLoader_Add_nid_postfix(loader); }), "Add with a negative current contour count throws");
    loader->current.outline.n_contours = 0;
    loader->base.outline.n_points = 0x8000;
    Require(Throws([&] { FT_GlyphLoader_Prepare_nid_postfix(loader); }), "Prepare with a negative base point count throws");
    loader->base.outline.n_points = 0;
    FT_GlyphLoader_Done_nid_postfix(loader);
}

void Copies() {
    Recorder recorder;
    Reset(recorder);
    auto* source = NewLoader(recorder);
    auto* target = NewLoader(recorder);
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(source, 3, 1) == 0, "source capacity");
    Require(FT_GlyphLoader_CreateExtra_nid_postfix(source) == 0, "source extra points");
    source->base.outline.n_points = 3;
    source->base.outline.n_contours = 1;
    for (int i = 0; i < 3; ++i) {
        source->base.outline.points[i] = {i + 1, i + 2};
        source->base.outline.tags[i] = static_cast<unsigned char>(i + 7);
        source->base.extra_points[i] = {50 + i, 60 + i};
        source->base.extra_points2[i] = {70 + i, 80 + i};
    }
    source->base.outline.contours[0] = 2;
    Require(FT_GlyphLoader_CopyPoints_nid_postfix(target, source) == 0, "CopyPoints succeeds");
    Require(target->max_points == 8 && target->max_contours == 4, "CopyPoints grows the target");
    Require(target->base.outline.n_points == 3 && target->base.outline.n_contours == 1, "CopyPoints sets the counts");
    Require(target->base.outline.points[2].x == 3 && target->base.outline.points[2].y == 4 && target->base.outline.tags[1] == 8 &&
            target->base.outline.contours[0] == 2, "CopyPoints copies points, tags and contours");
    Require(!target->base.extra_points, "extra points are copied only when both loaders use them");
    Require(target->current.outline.points == target->base.outline.points + 3, "CopyPoints adjusts the target");

    Require(FT_GlyphLoader_CreateExtra_nid_postfix(target) == 0, "target extra points");
    Require(FT_GlyphLoader_CopyPoints_nid_postfix(target, source) == 0, "CopyPoints with extra points succeeds");
    Require(target->base.extra_points[1].x == 51 && target->base.extra_points2[2].y == 82, "CopyPoints copies both extra tables");

    auto* empty = NewLoader(recorder);
    recorder.failAt = recorder.calls + 1;
    Require(FT_GlyphLoader_CopyPoints_nid_postfix(empty, source) == 0x40 && empty->base.outline.n_points == 0, "a failing CopyPoints copies nothing");
    recorder.failAt = 0;

    source->base.outline.n_contours = 0x8000;
    Require(Throws([&] { FT_GlyphLoader_CopyPoints_nid_postfix(target, source); }), "a negative source count throws");
    source->base.outline.n_contours = 1;
    FT_GlyphLoader_Done_nid_postfix(empty);
    FT_GlyphLoader_Done_nid_postfix(target);
    FT_GlyphLoader_Done_nid_postfix(source);
}

void Nulls() {
    Require(Throws([] { FT_GlyphLoader_Rewind_nid_postfix(nullptr); }), "Rewind of null throws");
    Require(Throws([] { FT_GlyphLoader_Reset_nid_postfix(nullptr); }), "Reset of null throws");
    Require(Throws([] { FT_GlyphLoader_CreateExtra_nid_postfix(nullptr); }), "CreateExtra of null throws");
    Require(Throws([] { FT_GlyphLoader_CheckPoints_nid_postfix(nullptr, 1, 1); }), "CheckPoints of null throws");
    Require(Throws([] { FT_GlyphLoader_CheckSubGlyphs_nid_postfix(nullptr, 1); }), "CheckSubGlyphs of null throws");
    Require(Throws([] { FT_GlyphLoader_Prepare_nid_postfix(nullptr); }), "Prepare of null throws");
    GuestGlyphLoaderRec loader{};
    Require(Throws([&] { FT_GlyphLoader_CopyPoints_nid_postfix(&loader, nullptr); }), "CopyPoints from null throws");
    Require(Throws([&] { FT_GlyphLoader_CopyPoints_nid_postfix(nullptr, &loader); }), "CopyPoints into null throws");

    FT_GlyphLoader_Reset_nid_postfix(&loader);
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(&loader, 0, 0) == 0, "an empty request needs no memory");
    Require(FT_GlyphLoader_CheckPoints_nid_postfix(&loader, 0x7FF9, 0) == 0x0A, "a too-large request needs no memory");
    Require(FT_GlyphLoader_CreateExtra_nid_postfix(&loader) == 0 && loader.use_extra == 1, "an empty CreateExtra needs no memory");
    Require(Throws([&] { FT_GlyphLoader_CheckPoints_nid_postfix(&loader, 1, 0); }), "growth without memory throws");
    Require(Throws([&] { FT_GlyphLoader_CheckSubGlyphs_nid_postfix(&loader, 1); }), "subglyph growth without memory throws");
    Require(Throws([&] { FT_GlyphLoader_Done_nid_postfix(&loader); }), "Done without memory throws");
    GuestVector point{};
    loader.base.outline.points = &point;
    Require(Throws([&] { FT_GlyphLoader_Reset_nid_postfix(&loader); }), "Reset of a table without memory throws");
    Require(loader.base.outline.points == &point, "a throwing Reset frees nothing");
}

void Validation() {
    Recorder recorder;
    Reset(recorder);
    GuestFaceRec face{};
    const unsigned char* tables[5]{};
    Require(FT_OpenType_Validate_nid_postfix(nullptr, 0, nullptr, nullptr, nullptr, nullptr, nullptr) == 0x23, "OpenType_Validate checks the face first");
    Require(FT_OpenType_Validate_nid_postfix(&face, 0, &tables[0], &tables[1], &tables[2], &tables[3], nullptr) == 0x06, "a null JSTF output is invalid");
    Require(FT_OpenType_Validate_nid_postfix(&face, 0, nullptr, &tables[1], &tables[2], &tables[3], &tables[4]) == 0x06, "a null BASE output is invalid");
    Require(Throws([&] { FT_OpenType_Validate_nid_postfix(&face, 0, &tables[0], &tables[1], &tables[2], &tables[3], &tables[4]); }),
            "OpenType validation itself throws");
    Require(FT_TrueTypeGX_Validate_nid_postfix(nullptr, 0, nullptr, 0) == 0x23, "TrueTypeGX_Validate checks the face first");
    Require(FT_TrueTypeGX_Validate_nid_postfix(&face, 0, nullptr, 0) == 0x06, "a null GX table array is invalid");
    Require(Throws([&] { FT_TrueTypeGX_Validate_nid_postfix(&face, 0, tables, 5); }), "GX validation itself throws");

    auto* table = static_cast<unsigned char*>(std::malloc(4));
    FT_OpenType_Free_nid_postfix(nullptr, table);
    FT_TrueTypeGX_Free_nid_postfix(nullptr, table);
    FT_OpenType_Free_nid_postfix(&face, nullptr);
    FT_TrueTypeGX_Free_nid_postfix(&face, nullptr);
    Require(Throws([&] { FT_OpenType_Free_nid_postfix(&face, table); }), "OpenType_Free without face memory throws");
    Require(Throws([&] { FT_TrueTypeGX_Free_nid_postfix(&face, table); }), "TrueTypeGX_Free without face memory throws");
    face.memory = &recorder.memory;
    FT_OpenType_Free_nid_postfix(&face, nullptr);
    Require(recorder.frees == 0, "freeing a null table calls nothing");
    FT_OpenType_Free_nid_postfix(&face, table);
    Require(recorder.frees == 1 && recorder.lastFreed == table, "OpenType_Free frees through the face memory");
    table = static_cast<unsigned char*>(std::malloc(4));
    FT_TrueTypeGX_Free_nid_postfix(&face, table);
    Require(recorder.frees == 2 && recorder.lastFreed == table, "TrueTypeGX_Free frees through the face memory");
}

}

int main() {
    Creation();
    Points();
    Extra();
    Stack();
    Copies();
    Nulls();
    Validation();
    std::puts("FreeType Ot glyph loader tests passed");
    return 0;
}
