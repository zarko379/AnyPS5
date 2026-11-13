// Derived from FreeType src/base/ftotval.c and src/base/ftgxval.c (FreeType 2.4.11, commit c6a66b49e64e9a21c013f23c867d238cf1aa98cb).
// Copyright 2004, 2005, 2006, 2008, 2010 by Masatake YAMATO, Redhat K.K, David Turner, Robert Wilhelm, and Werner Lemberg.
// This file is part of the FreeType project, and may only be used, modified, and distributed
// under the terms of the FreeType project license, LICENSE.TXT. By continuing to use, modify,
// or distribute this file you indicate that you have read the license and understand and accept it fully.

#include "prx/libSceFreeTypeOt/include/Validation.hpp"
#include "prx/libSceFreeTypeOt/include/Memory.hpp"
#include "prx/libc/include/General.hpp"

using namespace FreeType;

namespace {

constexpr int InvalidArgument = 0x06;
constexpr int InvalidFaceHandle = 0x23;

void FreeTable(GuestFaceRec* face, const unsigned char* table, const char* function) {
    if (!face) return;
    auto* memory = static_cast<GuestMemoryRec*>(face->memory);
    if (table && !memory) NotImplemented_nid_no_patch(function);
    ft_mem_free_nid_postfix(memory, table);
}

}

extern "C" {

int APS5_VABI FT_OpenType_Validate_nid_postfix(GuestFaceRec* face, std::uint32_t validationFlags, const unsigned char** baseTable,
                                               const unsigned char** gdefTable, const unsigned char** gposTable, const unsigned char** gsubTable,
                                               const unsigned char** jstfTable) {
    static_cast<void>(validationFlags);
    if (!face) return InvalidFaceHandle;
    if (!(baseTable && gdefTable && gposTable && gsubTable && jstfTable)) return InvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI FT_OpenType_Free_nid_postfix(GuestFaceRec* face, const unsigned char* table) {
    FreeTable(face, table, __func__);
}

int APS5_VABI FT_TrueTypeGX_Validate_nid_postfix(GuestFaceRec* face, std::uint32_t validationFlags, const unsigned char** tables, std::uint32_t tableLength) {
    static_cast<void>(validationFlags);
    static_cast<void>(tableLength);
    if (!face) return InvalidFaceHandle;
    if (!tables) return InvalidArgument;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI FT_TrueTypeGX_Free_nid_postfix(GuestFaceRec* face, const unsigned char* table) {
    FreeTable(face, table, __func__);
}

}
