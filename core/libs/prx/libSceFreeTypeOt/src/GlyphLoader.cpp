// Derived from FreeType src/base/ftgloadr.c (FreeType 2.4.11, commit c6a66b49e64e9a21c013f23c867d238cf1aa98cb).
// Copyright 2002, 2003, 2004, 2005, 2006, 2010 by David Turner, Robert Wilhelm, and Werner Lemberg.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#include "prx/libSceFreeTypeOt/include/GlyphLoader.hpp"
#include "prx/libSceFreeTypeOt/include/Memory.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

using namespace FreeType;

namespace {

constexpr int ArrayTooLarge = 0x0A;
constexpr std::uint32_t OutlinePointsMax = 0x7FFF;
constexpr std::uint32_t OutlineContoursMax = 0x7FFF;
constexpr std::int64_t IntMax = 0x7FFFFFFF;

std::int32_t Signed(std::uint16_t count) {
    return static_cast<std::int16_t>(count);
}

std::uint32_t PadCeil(std::uint32_t value, std::uint32_t step) {
    return (value + (step - 1)) & ~(step - 1);
}

template <typename T> T* Offset(T* base, std::int64_t count) {
    return reinterpret_cast<T*>(reinterpret_cast<std::uintptr_t>(base) + static_cast<std::uintptr_t>(count) * sizeof(T));
}

bool ReachesMemory(std::int64_t itemSize, std::int64_t currentCount, std::int64_t newCount, const void* block) {
    if (currentCount < 0 || newCount < 0 || itemSize < 0) return false;
    if (newCount == 0 || itemSize == 0) return block != nullptr;
    return newCount <= IntMax / itemSize;
}

template <typename T> T* Renew(GuestMemoryRec* memory, std::int64_t currentCount, std::int64_t newCount, T* block, std::int32_t* error, const char* function) {
    if (!memory && ReachesMemory(static_cast<std::int64_t>(sizeof(T)), currentCount, newCount, block)) NotImplemented_nid_no_patch(function);
    return static_cast<T*>(ft_mem_realloc_nid_postfix(memory, static_cast<std::int64_t>(sizeof(T)), currentCount, newCount, block, error));
}

template <typename T> void Free(GuestMemoryRec* memory, T*& block) {
    ft_mem_free_nid_postfix(memory, block);
    block = nullptr;
}

template <typename T> void Copy(T* target, const T* source, std::uint64_t count, const char* function) {
    if (count == 0) return;
    if (!target || !source) NotImplemented_nid_no_patch(function);
    std::memmove(target, source, static_cast<std::size_t>(count * sizeof(T)));
}

void AdjustPoints(GuestGlyphLoaderRec* loader, const char* function) {
    const GuestOutline& base = loader->base.outline;
    GuestOutline& current = loader->current.outline;
    const std::int32_t points = Signed(base.n_points);
    const std::int32_t contours = Signed(base.n_contours);
    if (points < 0 || contours < 0) NotImplemented_nid_no_patch(function);
    current.points = Offset(base.points, points);
    current.tags = Offset(base.tags, points);
    current.contours = Offset(base.contours, contours);
    if (loader->use_extra) {
        loader->current.extra_points = Offset(loader->base.extra_points, points);
        loader->current.extra_points2 = Offset(loader->base.extra_points2, points);
    }
}

void AdjustSubglyphs(GuestGlyphLoaderRec* loader) {
    loader->current.subglyphs = Offset(loader->base.subglyphs, loader->base.num_subglyphs);
}

void Rewind(GuestGlyphLoaderRec* loader) {
    GuestGlyphLoadRec& base = loader->base;
    base.outline.n_points = 0;
    base.outline.n_contours = 0;
    base.num_subglyphs = 0;
    loader->current = base;
}

void Reset(GuestGlyphLoaderRec* loader, const char* function) {
    GuestMemoryRec* memory = loader->memory;
    GuestGlyphLoadRec& base = loader->base;
    if (!memory && (base.outline.points || base.outline.tags || base.outline.contours || base.extra_points || base.subglyphs)) {
        NotImplemented_nid_no_patch(function);
    }
    Free(memory, base.outline.points);
    Free(memory, base.outline.tags);
    Free(memory, base.outline.contours);
    Free(memory, base.extra_points);
    Free(memory, base.subglyphs);
    base.extra_points2 = nullptr;
    loader->max_points = 0;
    loader->max_contours = 0;
    loader->max_subglyphs = 0;
    Rewind(loader);
}

int CheckPoints(GuestGlyphLoaderRec* loader, std::uint32_t nPoints, std::uint32_t nContours, const char* function) {
    GuestMemoryRec* memory = loader->memory;
    std::int32_t error = 0;
    GuestOutline& base = loader->base.outline;
    const GuestOutline& current = loader->current.outline;
    bool adjust = false;
    std::uint32_t newMax = static_cast<std::uint32_t>(Signed(base.n_points) + Signed(current.n_points)) + nPoints;
    std::uint32_t oldMax = loader->max_points;
    if (newMax > oldMax) {
        newMax = PadCeil(newMax, 8);
        if (newMax > OutlinePointsMax) return ArrayTooLarge;
        base.points = Renew(memory, oldMax, newMax, base.points, &error, function);
        if (error) return error;
        base.tags = Renew(memory, oldMax, newMax, base.tags, &error, function);
        if (error) return error;
        if (loader->use_extra) {
            loader->base.extra_points = Renew(memory, oldMax * 2u, newMax * 2u, loader->base.extra_points, &error, function);
            if (error) return error;
            Copy(Offset(loader->base.extra_points, newMax), Offset(loader->base.extra_points, oldMax), oldMax, function);
            loader->base.extra_points2 = Offset(loader->base.extra_points, newMax);
        }
        adjust = true;
        loader->max_points = newMax;
    }
    oldMax = loader->max_contours;
    newMax = static_cast<std::uint32_t>(Signed(base.n_contours) + Signed(current.n_contours)) + nContours;
    if (newMax > oldMax) {
        newMax = PadCeil(newMax, 4);
        if (newMax > OutlineContoursMax) return ArrayTooLarge;
        base.contours = Renew(memory, oldMax, newMax, base.contours, &error, function);
        if (error) return error;
        adjust = true;
        loader->max_contours = newMax;
    }
    if (adjust) AdjustPoints(loader, function);
    return error;
}

void Prepare(GuestGlyphLoaderRec* loader, const char* function) {
    GuestGlyphLoadRec& current = loader->current;
    current.outline.n_points = 0;
    current.outline.n_contours = 0;
    current.num_subglyphs = 0;
    AdjustPoints(loader, function);
    AdjustSubglyphs(loader);
}

}

extern "C" {

int APS5_VABI FT_GlyphLoader_New_nid_postfix(GuestMemoryRec* memory, GuestGlyphLoaderRec** aloader) {
    if (!memory || !aloader) NotImplemented_nid_no_patch(__func__);
    std::int32_t error = 0;
    auto* loader = static_cast<GuestGlyphLoaderRec*>(ft_mem_alloc_nid_postfix(memory, static_cast<std::int64_t>(sizeof(GuestGlyphLoaderRec)), &error));
    if (!error) {
        loader->memory = memory;
        *aloader = loader;
    }
    return error;
}

void APS5_VABI FT_GlyphLoader_Rewind_nid_postfix(GuestGlyphLoaderRec* loader) {
    if (!loader) NotImplemented_nid_no_patch(__func__);
    Rewind(loader);
}

void APS5_VABI FT_GlyphLoader_Reset_nid_postfix(GuestGlyphLoaderRec* loader) {
    if (!loader) NotImplemented_nid_no_patch(__func__);
    Reset(loader, __func__);
}

void APS5_VABI FT_GlyphLoader_Done_nid_postfix(GuestGlyphLoaderRec* loader) {
    if (!loader) return;
    GuestMemoryRec* memory = loader->memory;
    if (!memory) NotImplemented_nid_no_patch(__func__);
    Reset(loader, __func__);
    ft_mem_free_nid_postfix(memory, loader);
}

int APS5_VABI FT_GlyphLoader_CreateExtra_nid_postfix(GuestGlyphLoaderRec* loader) {
    if (!loader) NotImplemented_nid_no_patch(__func__);
    std::int32_t error = 0;
    loader->base.extra_points = Renew<GuestVector>(loader->memory, 0, 2u * loader->max_points, nullptr, &error, __func__);
    if (!error) {
        loader->use_extra = 1;
        loader->base.extra_points2 = Offset(loader->base.extra_points, loader->max_points);
        AdjustPoints(loader, __func__);
    }
    return error;
}

int APS5_VABI FT_GlyphLoader_CheckPoints_nid_postfix(GuestGlyphLoaderRec* loader, std::uint32_t nPoints, std::uint32_t nContours) {
    if (!loader) NotImplemented_nid_no_patch(__func__);
    return CheckPoints(loader, nPoints, nContours, __func__);
}

int APS5_VABI FT_GlyphLoader_CheckSubGlyphs_nid_postfix(GuestGlyphLoaderRec* loader, std::uint32_t nSubs) {
    if (!loader) NotImplemented_nid_no_patch(__func__);
    std::int32_t error = 0;
    GuestGlyphLoadRec& base = loader->base;
    std::uint32_t newMax = base.num_subglyphs + loader->current.num_subglyphs + nSubs;
    const std::uint32_t oldMax = loader->max_subglyphs;
    if (newMax > oldMax) {
        newMax = PadCeil(newMax, 2);
        base.subglyphs = Renew(loader->memory, oldMax, newMax, base.subglyphs, &error, __func__);
        if (error) return error;
        loader->max_subglyphs = newMax;
        AdjustSubglyphs(loader);
    }
    return error;
}

void APS5_VABI FT_GlyphLoader_Prepare_nid_postfix(GuestGlyphLoaderRec* loader) {
    if (!loader) NotImplemented_nid_no_patch(__func__);
    Prepare(loader, __func__);
}

void APS5_VABI FT_GlyphLoader_Add_nid_postfix(GuestGlyphLoaderRec* loader) {
    if (!loader) return;
    GuestGlyphLoadRec& base = loader->base;
    GuestGlyphLoadRec& current = loader->current;
    const std::int32_t currentContours = Signed(current.outline.n_contours);
    if (currentContours < 0 || (currentContours > 0 && !current.outline.contours)) NotImplemented_nid_no_patch(__func__);
    const auto basePoints = static_cast<std::uint32_t>(Signed(base.outline.n_points));
    base.outline.n_points = static_cast<std::uint16_t>(Signed(base.outline.n_points) + Signed(current.outline.n_points));
    base.outline.n_contours = static_cast<std::uint16_t>(Signed(base.outline.n_contours) + currentContours);
    base.num_subglyphs += current.num_subglyphs;
    for (std::int32_t n = 0; n < currentContours; ++n) {
        current.outline.contours[n] = static_cast<std::uint16_t>(static_cast<std::uint32_t>(Signed(current.outline.contours[n])) + basePoints);
    }
    Prepare(loader, __func__);
}

int APS5_VABI FT_GlyphLoader_CopyPoints_nid_postfix(GuestGlyphLoaderRec* target, GuestGlyphLoaderRec* source) {
    if (!target || !source) NotImplemented_nid_no_patch(__func__);
    const std::int32_t points = Signed(source->base.outline.n_points);
    const std::int32_t contours = Signed(source->base.outline.n_contours);
    if (points < 0 || contours < 0) NotImplemented_nid_no_patch(__func__);
    const int error = CheckPoints(target, static_cast<std::uint32_t>(points), static_cast<std::uint32_t>(contours), __func__);
    if (!error) {
        GuestOutline& out = target->base.outline;
        const GuestOutline& in = source->base.outline;
        Copy(out.points, in.points, static_cast<std::uint32_t>(points), __func__);
        Copy(out.tags, in.tags, static_cast<std::uint32_t>(points), __func__);
        Copy(out.contours, in.contours, static_cast<std::uint32_t>(contours), __func__);
        if (target->use_extra && source->use_extra) {
            Copy(target->base.extra_points, source->base.extra_points, static_cast<std::uint32_t>(points), __func__);
            Copy(target->base.extra_points2, source->base.extra_points2, static_cast<std::uint32_t>(points), __func__);
        }
        out.n_points = static_cast<std::uint16_t>(points);
        out.n_contours = static_cast<std::uint16_t>(contours);
        AdjustPoints(target, __func__);
    }
    return error;
}

}
