#include "prx/libSceXml/src/Node.hpp"

#include "prx/libSceXml/src/Document.hpp"
#include "prx/libSceXml/src/DomReader.hpp"

#include <cstring>
#include <stdexcept>

namespace XmlDom {

namespace {

constexpr std::uint32_t InvalidNodeType = 0xffffffff;

const Generation* GenerationOf(const XmlNode* self) {
    if (self->id.first == nullptr) return nullptr;
    const Generation* generation = static_cast<const Generation*>(self->id.first);
    if (!generation->alive) throw std::logic_error("sce::Xml::Dom::Node: reading a node after its builder parsed again or terminated reads freed memory in 13.42");
    return generation;
}

std::uint64_t OffsetOf(const XmlNode* self) {
    return reinterpret_cast<std::uint64_t>(self->id.second);
}

}

void NodeCreate(XmlNode* self, const XmlNodeId& id) {
    self->id.first = id.first;
    self->id.second = id.second;
    self->id.flag = id.flag;
    self->document = id.first;
    self->node = id.second;
    for (int i = 0; i < 3; ++i) self->state[i] = 0;
}

void NodeDestroy(XmlNode*) {
}

std::uint32_t NodeTypeOf(const XmlNode* self) {
    const Generation* generation = GenerationOf(self);
    if (generation == nullptr) return InvalidNodeType;
    return Reader(generation->buffers).NodeType(OffsetOf(self));
}

bool NodeHasChildNodes(const XmlNode* self) {
    const Generation* generation = GenerationOf(self);
    if (generation == nullptr) return false;
    return Reader(generation->buffers).HasChildNodes(OffsetOf(self));
}

XmlString* NodeName(XmlString* result, const XmlNode* self) {
    if (result == nullptr || self == nullptr) throw std::invalid_argument("sce::Xml::Dom::Node::getNodeName: null object");
    const Generation* generation = GenerationOf(self);
    const char* text = generation != nullptr ? Reader(generation->buffers).NodeName(OffsetOf(self)) : nullptr;
    if (text != nullptr) const_cast<Generation*>(generation)->lent = true;
    result->buffer = text;
    result->size = text != nullptr ? std::strlen(text) : 0;
    return result;
}

bool NodeHasAttributes(const XmlNode* self) {
    const Generation* generation = GenerationOf(self);
    if (generation == nullptr) return false;
    return Reader(generation->buffers).HasAttributes(OffsetOf(self));
}

}
