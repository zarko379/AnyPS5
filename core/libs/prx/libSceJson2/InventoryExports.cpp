#include "prx/libSceJson2/include/Api.hpp"
#include <stdexcept>
#include "SceTypes.hpp"

extern "C" {

void APS5_VABI _ZN3sce4Json12MemAllocatorD0Ev(void*) {
    throw std::logic_error("sce::Json::MemAllocator: deleting destructor of the abstract base");
}

}
