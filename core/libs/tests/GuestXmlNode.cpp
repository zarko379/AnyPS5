#include "prx/libSceXml/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

struct TestAllocator {
    XmlMemAllocator base;
    std::vector<void*> blocks;
};

void* APS5_VABI TestAllocate(XmlMemAllocator* self, std::size_t size, void* userData) {
    auto* allocator = reinterpret_cast<TestAllocator*>(self);
    Require(userData == reinterpret_cast<void*>(0x1234), "allocate receives the user data");
    void* block = std::malloc(size);
    allocator->blocks.push_back(block);
    return block;
}

void APS5_VABI TestDeallocate(XmlMemAllocator* self, void* pointer, void* userData) {
    auto* allocator = reinterpret_cast<TestAllocator*>(self);
    Require(userData == reinterpret_cast<void*>(0x1234), "deallocate receives the user data");
    std::size_t index = 0;
    while (index < allocator->blocks.size() && allocator->blocks[index] != pointer) ++index;
    Require(index < allocator->blocks.size(), "deallocate receives a live block");
    std::free(pointer);
    allocator->blocks[index] = nullptr;
}

void APS5_VABI TestDestroy(XmlMemAllocator*) {}

const XmlMemAllocatorVtable g_testVtable{TestDestroy, TestDestroy, TestAllocate, TestDeallocate};

struct Fixture {
    TestAllocator allocator{};
    XmlInitParameter parameter{};
    XmlHandle initializer{nullptr};
    XmlHandle builder{nullptr};

    Fixture() {
        allocator.base.vtable = &g_testVtable;
        parameter = {&allocator.base, reinterpret_cast<void*>(0x1234), {0, 0}};
        Require(_ZN3sce3Xml11Initializer10initializeEPKNS0_13InitParameterE(&initializer, &parameter) == 0, "the initializer starts");
        Require(_ZN3sce3Xml3Dom15DocumentBuilder10initializeEPKNS0_11InitializerE(&builder, &initializer) == 0, "the builder starts");
    }

    ~Fixture() {
        _ZN3sce3Xml3Dom15DocumentBuilder9terminateEv(&builder);
        _ZN3sce3Xml11Initializer9terminateEv(&initializer);
        for (void* block : allocator.blocks) {
            if (block != nullptr) std::free(block);
        }
    }

    int Parse(const char* text, bool last = true) {
        const XmlString string{text, std::strlen(text)};
        return _ZN3sce3Xml3Dom15DocumentBuilder5parseEPKNS0_6StringEb(&builder, &string, last);
    }
};

template <typename Exception, typename Call>
bool Throws(Call call) {
    try {
        call();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

std::string NodeNameOf(const XmlNode* node) {
    XmlString name{reinterpret_cast<const char*>(1), 99};
    Require(_ZNK3sce3Xml3Dom4Node11getNodeNameEv(&name, node) == &name, "getNodeName returns its result");
    if (name.buffer == nullptr) {
        Require(name.size == 0, "a missing node name has no size");
        return "<null>";
    }
    Require(std::strlen(name.buffer) == name.size, "a node name size is its length");
    return std::string(name.buffer, name.size);
}

std::string DocumentNodeName(const XmlHandle* document, XmlNodeId id) {
    XmlString name{};
    _ZNK3sce3Xml3Dom8Document11getNodeNameENS1_6NodeIdE(&name, document, id);
    if (name.buffer == nullptr) return "<null>";
    return std::string(name.buffer, name.size);
}

XmlNodeId Root(const XmlHandle* document) {
    XmlNodeId id{};
    _ZNK3sce3Xml3Dom8Document7getRootEv(&id, document);
    return id;
}

XmlNodeId FirstChild(const XmlHandle* document, XmlNodeId node) {
    XmlNodeId id{};
    _ZNK3sce3Xml3Dom8Document13getFirstChildENS1_6NodeIdE(&id, document, node);
    return id;
}

int DocumentType(const XmlHandle* document, XmlNodeId node) {
    return _ZNK3sce3Xml3Dom8Document11getNodeTypeENS1_6NodeIdE(document, node);
}

bool IdIsNull(XmlNodeId node) {
    return node.first == nullptr && node.second == nullptr && node.flag == 0;
}

void TestElement() {
    Fixture f;
    Require(f.Parse("<r a=\"1\"><b/><c>t</c></r>") == 0, "parse");
    XmlHandle document{nullptr};
    _ZN3sce3Xml3Dom15DocumentBuilder11getDocumentEv(&document, &f.builder);
    const XmlNodeId root = Root(&document);

    XmlNode node{};
    _ZN3sce3Xml3Dom4NodeC1ENS1_6NodeIdE(&node, root);
    Require(_ZNK3sce3Xml3Dom4Node11isAvailableEv(&node), "a node built from the root is available");
    Require(node.document == root.first && node.node == root.second, "the node keeps the document and the position of the id");
    Require(static_cast<int>(_ZNK3sce3Xml3Dom4Node11getNodeTypeEv(&node)) == DocumentType(&document, root), "the node type matches the document read");
    Require(_ZNK3sce3Xml3Dom4Node13hasChildNodesEv(&node), "the root has children");
    Require(_ZNK3sce3Xml3Dom4Node13hasAttributesEv(&node), "the root has attributes");

    const XmlNodeId child = FirstChild(&document, root);
    XmlNode childNode{};
    _ZN3sce3Xml3Dom4NodeC2ENS1_6NodeIdE(&childNode, child);
    Require(_ZNK3sce3Xml3Dom4Node11isAvailableEv(&childNode), "a node built from a child is available");
    Require(static_cast<int>(_ZNK3sce3Xml3Dom4Node11getNodeTypeEv(&childNode)) == DocumentType(&document, child), "a child node type");
    Require(!_ZNK3sce3Xml3Dom4Node13hasChildNodesEv(&childNode) && !_ZNK3sce3Xml3Dom4Node13hasAttributesEv(&childNode), "an empty element");

    XmlNodeId text{};
    _ZNK3sce3Xml3Dom8Document13getFirstChildENS1_6NodeIdE(&text, &document, child);
    XmlNodeId next{};
    _ZNK3sce3Xml3Dom8Document10getSiblingENS1_6NodeIdE(&next, &document, child);
    Require(!IdIsNull(next), "the second child exists");
    XmlNode textNode{};
    _ZN3sce3Xml3Dom4NodeC1ENS1_6NodeIdE(&textNode, FirstChild(&document, next));
    Require(_ZNK3sce3Xml3Dom4Node11isAvailableEv(&textNode), "a text node is available");
    Require(static_cast<int>(_ZNK3sce3Xml3Dom4Node11getNodeTypeEv(&textNode)) == DocumentType(&document, FirstChild(&document, next)), "a text node type");
    Require(!_ZNK3sce3Xml3Dom4Node13hasChildNodesEv(&textNode) && !_ZNK3sce3Xml3Dom4Node13hasAttributesEv(&textNode), "a text node has neither children nor attributes");

    XmlNodeId attribute{};
    _ZNK3sce3Xml3Dom8Document12getFirstAttrENS1_6NodeIdE(&attribute, &document, root);
    XmlNode attributeNode{};
    _ZN3sce3Xml3Dom4NodeC1ENS1_6NodeIdE(&attributeNode, attribute);
    Require(_ZNK3sce3Xml3Dom4Node11isAvailableEv(&attributeNode), "an attribute node is available");
    Require(static_cast<int>(_ZNK3sce3Xml3Dom4Node11getNodeTypeEv(&attributeNode)) == DocumentType(&document, attribute), "an attribute node type");
    Require(!_ZNK3sce3Xml3Dom4Node13hasChildNodesEv(&attributeNode) && !_ZNK3sce3Xml3Dom4Node13hasAttributesEv(&attributeNode), "an attribute node has neither children nor attributes");

    XmlNode copy{};
    _ZN3sce3Xml3Dom4NodeC1ERKS2_(&copy, &node);
    Require(copy.id.first == node.id.first && copy.id.second == node.id.second && copy.id.flag == node.id.flag, "a copy keeps the id");
    Require(copy.document == node.document && copy.node == node.node, "a copy keeps the document and the position");
    Require(static_cast<int>(_ZNK3sce3Xml3Dom4Node11getNodeTypeEv(&copy)) == DocumentType(&document, root), "a copy answers as the original");
    XmlNode assigned{};
    _ZN3sce3Xml3Dom4NodeaSERKS2_(&assigned, &attributeNode);
    Require(assigned.document == attributeNode.document && assigned.node == attributeNode.node, "assignment copies the handles");
    _ZN3sce3Xml3Dom4NodeD1Ev(&copy);
    _ZN3sce3Xml3Dom4NodeD2Ev(&assigned);
    _ZN3sce3Xml3Dom4NodeD1Ev(&textNode);
    _ZN3sce3Xml3Dom4NodeD1Ev(&attributeNode);
    _ZN3sce3Xml3Dom4NodeD1Ev(&childNode);
    _ZN3sce3Xml3Dom4NodeD1Ev(&node);
    _ZN3sce3Xml3Dom8Document9terminateEv(&document);
}

void TestUnavailable() {
    XmlNode empty{};
    empty.document = reinterpret_cast<void*>(0x6666);
    empty.node = reinterpret_cast<void*>(0x7777);
    empty.state[0] = 1;
    const XmlNodeId nullId{nullptr, nullptr, 0};
    _ZN3sce3Xml3Dom4NodeC1ENS1_6NodeIdE(&empty, nullId);
    Require(!_ZNK3sce3Xml3Dom4Node11isAvailableEv(&empty), "a node built from a null id is not available");
    Require(empty.document == nullptr && empty.node == nullptr, "a null id clears both handles");
    Require(_ZNK3sce3Xml3Dom4Node11getNodeTypeEv(&empty) == 0xffffffffu, "a node without a document has no node type");
    Require(!_ZNK3sce3Xml3Dom4Node13hasChildNodesEv(&empty) && !_ZNK3sce3Xml3Dom4Node13hasAttributesEv(&empty), "a node without a document has neither children nor attributes");
    _ZN3sce3Xml3Dom4NodeD1Ev(&empty);

    Fixture f;
    Require(f.Parse("<r/>") == 0, "parse");
    XmlHandle document{nullptr};
    _ZN3sce3Xml3Dom15DocumentBuilder11getDocumentEv(&document, &f.builder);
    const XmlNodeId root = Root(&document);

    XmlNode past{};
    _ZN3sce3Xml3Dom4NodeC1ENS1_6NodeIdE(&past, XmlNodeId{root.first, reinterpret_cast<void*>(0xffffffffu), 0});
    bool threw = false;
    try {
        _ZNK3sce3Xml3Dom4Node11getNodeTypeEv(&past);
    } catch (const std::logic_error&) {
        threw = true;
    }
    Require(threw, "a node offset past the document stream throws");
    _ZN3sce3Xml3Dom4NodeD1Ev(&past);

    XmlNode first{};
    _ZN3sce3Xml3Dom4NodeC1ENS1_6NodeIdE(&first, root);
    Require(_ZNK3sce3Xml3Dom4Node13hasChildNodesEv(&first) == false, "an element with no children");
    Require(_ZNK3sce3Xml3Dom4Node13hasAttributesEv(&first) == false, "an element with no attributes");
    Require(_ZNK3sce3Xml3Dom4Node11getNodeTypeEv(&first) != 0xffffffffu, "a live node has a type");

    Require(f.Parse("<s><b/></s>") == 0, "parse again");
    threw = false;
    try {
        _ZNK3sce3Xml3Dom4Node11getNodeTypeEv(&first);
    } catch (const std::logic_error&) {
        threw = true;
    }
    Require(threw, "reading a node after the builder parsed again throws");
    _ZN3sce3Xml3Dom4NodeD1Ev(&first);
    _ZN3sce3Xml3Dom8Document9terminateEv(&document);
}

}

void TestNodeName() {
    XmlNode empty{};
    _ZN3sce3Xml3Dom4NodeC1ENS1_6NodeIdE(&empty, XmlNodeId{nullptr, nullptr, 0});
    Require(NodeNameOf(&empty) == "<null>", "a node without a document has no name");
    XmlString unused{};
    Require(Throws<std::invalid_argument>([&] { _ZNK3sce3Xml3Dom4Node11getNodeNameEv(nullptr, &empty); }), "a null result throws");

    Fixture f;
    Require(f.Parse("<r a=\"1\"><bee/><c>t</c></r>") == 0, "parse");
    XmlHandle document{nullptr};
    _ZN3sce3Xml3Dom15DocumentBuilder11getDocumentEv(&document, &f.builder);
    const XmlNodeId root = Root(&document);
    const XmlNodeId bee = FirstChild(&document, root);
    XmlNodeId c{};
    _ZNK3sce3Xml3Dom8Document10getSiblingENS1_6NodeIdE(&c, &document, bee);
    const XmlNodeId text = FirstChild(&document, c);
    XmlNodeId attribute{};
    _ZNK3sce3Xml3Dom8Document12getFirstAttrENS1_6NodeIdE(&attribute, &document, root);
    XmlNodeId docRoot{};
    _ZNK3sce3Xml3Dom8Document10getDocRootEv(&docRoot, &document);

    const char* expected[] = {"r", "bee", "c"};
    const XmlNodeId elements[] = {root, bee, c};
    for (int i = 0; i < 3; ++i) {
        XmlNode node{};
        _ZN3sce3Xml3Dom4NodeC1ENS1_6NodeIdE(&node, elements[i]);
        Require(NodeNameOf(&node) == expected[i], "an element node name");
        Require(NodeNameOf(&node) == DocumentNodeName(&document, elements[i]), "an element node name matches the document read");
    }
    for (const XmlNodeId id : {text, attribute, docRoot}) {
        Require(!IdIsNull(id), "the node exists");
        XmlNode node{};
        _ZN3sce3Xml3Dom4NodeC1ENS1_6NodeIdE(&node, id);
        Require(NodeNameOf(&node) == DocumentNodeName(&document, id), "a non-element node name matches the document read");
    }

    XmlNode past{};
    _ZN3sce3Xml3Dom4NodeC1ENS1_6NodeIdE(&past, XmlNodeId{root.first, reinterpret_cast<void*>(0xffffffffu), 0});
    Require(Throws<std::logic_error>([&] { _ZNK3sce3Xml3Dom4Node11getNodeNameEv(&unused, &past); }), "a node offset past the document stream throws");

    XmlNode first{};
    _ZN3sce3Xml3Dom4NodeC1ENS1_6NodeIdE(&first, root);
    Require(f.Parse("<s/>") == 0, "parse again");
    Require(Throws<std::logic_error>([&] { _ZNK3sce3Xml3Dom4Node11getNodeNameEv(&unused, &first); }), "reading a node name after the builder parsed again throws");
    _ZN3sce3Xml3Dom8Document9terminateEv(&document);

    Require(f.Parse("<chunked><a/>", false) == 0x00850001, "a first chunk");
    XmlHandle partial{nullptr};
    _ZN3sce3Xml3Dom15DocumentBuilder11getDocumentEv(&partial, &f.builder);
    XmlNode chunkRoot{};
    _ZN3sce3Xml3Dom4NodeC1ENS1_6NodeIdE(&chunkRoot, Root(&partial));
    XmlString lent{};
    _ZNK3sce3Xml3Dom4Node11getNodeNameEv(&lent, &chunkRoot);
    Require(lent.size == 7 && std::memcmp(lent.buffer, "chunked", 7) == 0, "the root name of a partial document");
    Require(f.Parse("<b/></chunked>", true) == 0, "the last chunk");
    Require(lent.size == 7 && std::memcmp(lent.buffer, "chunked", 8) == 0, "a node name taken before the next chunk stays readable");
    _ZN3sce3Xml3Dom8Document9terminateEv(&partial);
}

int main() {
    TestElement();
    TestUnavailable();
    TestNodeName();
    return 0;
}
