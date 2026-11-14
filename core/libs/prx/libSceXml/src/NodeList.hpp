#pragma once

#include "prx/libSceXml/include/Api.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace XmlDom {

struct NodeListState {
    XmlMemAllocator* allocator = nullptr;
    void* userData = nullptr;
    std::vector<XmlNodeId> items;
};

std::optional<int> NodeListInitialize(XmlHandle* self, const XmlHandle* initializer);
int NodeListTerminate(XmlHandle* self);
void NodeListDestroy(XmlHandle* self);
bool NodeListCopy(XmlHandle* self, const XmlHandle* other);
bool NodeListIsAvailable(const XmlHandle* self);
NodeListState* NodeListStateOf(const XmlHandle* self);
std::optional<std::size_t> NodeListFind(const NodeListState* state, const XmlNodeId& node);
bool NodeListIdIsNull(const XmlNodeId& node);

}
