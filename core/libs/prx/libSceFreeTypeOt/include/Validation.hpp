#pragma once

#include "prx/libSceFreeType/include/GuestFreeType.hpp"

#include <cstdint>

extern "C" {

int APS5_VABI FT_OpenType_Validate_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t validationFlags, const unsigned char** baseTable,
                                               const unsigned char** gdefTable, const unsigned char** gposTable, const unsigned char** gsubTable,
                                               const unsigned char** jstfTable);
void APS5_VABI FT_OpenType_Free_nid_postfix(FreeType::GuestFaceRec* face, const unsigned char* table);
int APS5_VABI FT_TrueTypeGX_Validate_nid_postfix(FreeType::GuestFaceRec* face, std::uint32_t validationFlags, const unsigned char** tables, std::uint32_t tableLength);
void APS5_VABI FT_TrueTypeGX_Free_nid_postfix(FreeType::GuestFaceRec* face, const unsigned char* table);

}
