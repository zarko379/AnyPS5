#include "prx/libSceXml/src/NodeList.hpp"

#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

namespace XmlDom {

namespace {

constexpr int NodeListNotInitialized = static_cast<int>(0x80850002);
constexpr int NodeListAlreadyInitialized = 0x00850003;
constexpr std::size_t NodeListBlockSize = 72;

void RequireHandle(const XmlHandle* self, const char* function) {
    if (self == nullptr) throw std::invalid_argument(std::string("sce::Xml::Dom::NodeList::") + function + ": null object");
}

bool Attach(XmlHandle* self, XmlMemAllocator* allocator, void* userData, const std::vector<XmlNodeId>& items) {
    auto state = std::make_unique<NodeListState>();
    state->allocator = allocator;
    state->userData = userData;
    state->items = items;
    void* block = allocator->vtable->allocate(allocator, NodeListBlockSize, userData);
    if (block == nullptr) return false;
    NodeListState* owned = state.release();
    std::memcpy(block, &owned, sizeof(owned));
    self->impl = block;
    return true;
}

}

NodeListState* NodeListStateOf(const XmlHandle* self) {
    NodeListState* state = nullptr;
    std::memcpy(&state, self->impl, sizeof(state));
    return state;
}

std::optional<int> NodeListInitialize(XmlHandle* self, const XmlHandle* initializer) {
    RequireHandle(self, "initialize");
    if (self->impl != nullptr) return NodeListAlreadyInitialized;
    if (initializer == nullptr || initializer->impl == nullptr) return NodeListNotInitialized;
    const auto* parameter = static_cast<const XmlInitParameter*>(initializer->impl);
    if (parameter->allocator == nullptr) throw std::invalid_argument("sce::Xml::Dom::NodeList::initialize: null allocator");
    if (!Attach(self, parameter->allocator, parameter->userData, {})) return std::nullopt;
    return 0;
}

void NodeListDestroy(XmlHandle* self) {
    RequireHandle(self, "~NodeList");
    if (self->impl == nullptr) return;
    NodeListState* state = NodeListStateOf(self);
    state->allocator->vtable->deallocate(state->allocator, self->impl, state->userData);
    delete state;
    self->impl = nullptr;
}

int NodeListTerminate(XmlHandle* self) {
    RequireHandle(self, "terminate");
    if (self->impl == nullptr) return NodeListNotInitialized;
    NodeListDestroy(self);
    return 0;
}

bool NodeListCopy(XmlHandle* self, const XmlHandle* other) {
    RequireHandle(self, "NodeList");
    RequireHandle(other, "NodeList");
    self->impl = nullptr;
    if (other->impl == nullptr) return true;
    const NodeListState* source = NodeListStateOf(other);
    return Attach(self, source->allocator, source->userData, source->items);
}

bool NodeListIsAvailable(const XmlHandle* self) {
    RequireHandle(self, "isAvailable");
    return self->impl != nullptr;
}

std::optional<std::size_t> NodeListFind(const NodeListState* state, const XmlNodeId& node) {
    for (std::size_t index = 0; index < state->items.size(); ++index) {
        const XmlNodeId& item = state->items[index];
        if (item.first == node.first && item.second == node.second && item.flag == node.flag) return index;
    }
    return std::nullopt;
}

bool NodeListIdIsNull(const XmlNodeId& node) {
    return node.first == nullptr && node.second == nullptr && node.flag == 0;
}

}
