#include "prx/libSceFreeTypeOt/include/Outline.hpp"
#include "prx/libSceFreeTypeOt/include/Memory.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"

#include <cstdint>
#include <cstdlib>

namespace {

constexpr int InvalidArgument = 0x06;
constexpr int InvalidLibraryHandle = 0x21;
constexpr std::int32_t OutlineOwner = 0x1;

template <typename T>
T* NewArray(FreeType::GuestMemoryRec* memory, std::int64_t count, std::int32_t* error) {
    return static_cast<T*>(ft_mem_realloc_nid_postfix(memory, static_cast<std::int64_t>(sizeof(T)), 0, count, nullptr, error));
}

void* APS5_VABI SystemAlloc(FreeType::GuestMemoryRec*, std::int64_t size) {
    return std::malloc(static_cast<std::size_t>(size));
}

void APS5_VABI SystemFree(FreeType::GuestMemoryRec*, void* block) {
    std::free(block);
}

void* APS5_VABI SystemRealloc(FreeType::GuestMemoryRec*, std::int64_t, std::int64_t size, void* block) {
    return std::realloc(block, static_cast<std::size_t>(size));
}

FreeType::GuestMemoryRec SystemMemory{nullptr, SystemAlloc, SystemFree, SystemRealloc};

}

extern "C" {

int APS5_VABI FT_Outline_Done_Internal_nid_postfix(FreeType::GuestMemoryRec* memory, FreeType::GuestOutline* outline) {
    if (!memory || !outline) return InvalidArgument;
    if (outline->flags & OutlineOwner) {
        ft_mem_free_nid_postfix(memory, outline->points);
        ft_mem_free_nid_postfix(memory, outline->tags);
        ft_mem_free_nid_postfix(memory, outline->contours);
    }
    *outline = {};
    return 0;
}

int APS5_VABI FT_Outline_New_Internal_nid_postfix(FreeType::GuestMemoryRec* memory, std::uint32_t numPoints, std::int32_t numContours, FreeType::GuestOutline* anoutline) {
    if (!anoutline || !memory) return InvalidArgument;
    *anoutline = {};
    std::int32_t error = 0;
    anoutline->points = NewArray<FreeType::GuestVector>(memory, numPoints, &error);
    if (!error) anoutline->tags = NewArray<unsigned char>(memory, numPoints, &error);
    if (!error) anoutline->contours = NewArray<std::uint16_t>(memory, numContours, &error);
    anoutline->flags |= OutlineOwner;
    if (error) {
        FT_Outline_Done_Internal_nid_postfix(memory, anoutline);
        return error;
    }
    anoutline->n_points = static_cast<std::uint16_t>(numPoints);
    anoutline->n_contours = static_cast<std::uint16_t>(numContours);
    return 0;
}

int APS5_VABI FT_Outline_NewOt_nid_no_patch(FreeType::GuestLibrary* library, std::uint32_t numPoints, std::int32_t numContours, FreeType::GuestOutline* anoutline) {
    if (!library) return InvalidLibraryHandle;
    return FT_Outline_New_Internal_nid_postfix(&SystemMemory, numPoints, numContours, anoutline);
}
APS5_EXPORT("QH2emcgeFck", FT_Outline_NewOt_nid_no_patch);

int APS5_VABI FT_Outline_DoneOt_nid_no_patch(FreeType::GuestLibrary* library, FreeType::GuestOutline* outline) {
    if (!library) return InvalidLibraryHandle;
    return FT_Outline_Done_Internal_nid_postfix(&SystemMemory, outline);
}
APS5_EXPORT("BqGJsJcFYaM", FT_Outline_DoneOt_nid_no_patch);

}
