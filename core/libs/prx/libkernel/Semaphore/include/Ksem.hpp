#ifndef CORE_LIBS_PRX_LIBKERNEL_SEMAPHORE_KSEM_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SEMAPHORE_KSEM_HPP

#include <cstdint>

bool IsKsemDescriptor(int descriptor);

extern "C" int KsemOpen_nid_no_patch(std::intptr_t* idp, const char* name, int oflag, std::uint16_t mode, unsigned int value);
extern "C" int KsemUnlink_nid_no_patch(const char* name);
extern "C" int KsemClose_nid_no_patch(std::intptr_t id);

#ifdef ANYPS5_KSEM_TESTING
extern "C" unsigned KsemWaiterCountForTests_nid_no_patch(std::intptr_t id);
#endif

#endif
