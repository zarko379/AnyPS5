#include "prx/libSceLibreSsl3/include/Objects.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/include/ObjectTable.hpp"
#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr int Asn1Library = 13;
constexpr int AnyFunction = 0xfff;
constexpr int MallocFailure = 65;
constexpr int FirstNumberTooLarge = 122;
constexpr int InvalidDigit = 130;
constexpr int InvalidSeparator = 131;
constexpr int MissingSecondNumber = 138;
constexpr int SecondNumberTooLarge = 147;
constexpr int TooLong = 155;
constexpr int InvalidNumber = 187;
constexpr int Dynamic = 0x01;
constexpr int DynamicData = 0x08;

void Asn1Error(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, AnyFunction, reason);
}

bool ParseTextArc(std::string_view& text, std::uint64_t& arc, char& separator, bool first) {
    if (!first) {
        if (text.empty()) return false;
        const char value = text.front();
        text.remove_prefix(1);
        if ((separator == 0 && value != '.' && value != ' ') || (separator != 0 && value != separator)) {
            Asn1Error(InvalidSeparator);
            return false;
        }
        separator = value;
    }
    std::uint64_t result = 0;
    int digits = 0;
    while (!text.empty()) {
        auto value = static_cast<unsigned char>(text.front());
        if (value == '.' || value == ' ') break;
        text.remove_prefix(1);
        if (value < '0' || value > '9') {
            Asn1Error(first ? InvalidSeparator : InvalidDigit);
            return false;
        }
        value = static_cast<unsigned char>(value - '0');
        if (digits > 0 && result == 0 && value == 0) {
            Asn1Error(InvalidNumber);
            return false;
        }
        ++digits;
        if (result > std::numeric_limits<std::uint64_t>::max() / 10) {
            Asn1Error(TooLong);
            return false;
        }
        result = result * 10 + value;
    }
    if (digits < 1) {
        Asn1Error(InvalidNumber);
        return false;
    }
    arc = result;
    return true;
}

void AddArc(std::vector<unsigned char>& out, std::uint64_t arc) {
    bool started = false;
    for (int index = (sizeof(arc) * 8) / 7; index >= 0; --index) {
        auto value = static_cast<unsigned char>((arc >> (index * 7)) & 0x7f);
        if (!started && index != 0 && value == 0) continue;
        if (index > 0) value |= 0x80;
        out.push_back(value);
        started = true;
    }
}

std::optional<std::vector<unsigned char>> TextToContent(std::string_view text) {
    std::uint64_t first = 0;
    std::uint64_t second = 0;
    char separator = 0;
    if (!ParseTextArc(text, first, separator, true)) return std::nullopt;
    if (text.empty()) {
        Asn1Error(MissingSecondNumber);
        return std::nullopt;
    }
    if (!ParseTextArc(text, second, separator, false)) return std::nullopt;
    if (first > 2) {
        Asn1Error(FirstNumberTooLarge);
        return std::nullopt;
    }
    if ((first < 2 && second >= 40) || second > std::numeric_limits<std::uint64_t>::max() - first * 40) {
        Asn1Error(SecondNumberTooLarge);
        return std::nullopt;
    }
    std::vector<unsigned char> content;
    AddArc(content, first * 40 + second);
    while (!text.empty()) {
        std::uint64_t arc = 0;
        if (!ParseTextArc(text, arc, separator, false)) return std::nullopt;
        AddArc(content, arc);
    }
    return content;
}

bool ParseContentArc(const unsigned char*& data, std::size_t& remaining, std::uint64_t& arc) {
    std::uint64_t result = 0;
    unsigned char value = 0;
    do {
        if (remaining == 0) return false;
        value = *data++;
        --remaining;
        if (result == 0 && value == 0x80) return false;
        if (result > (std::numeric_limits<std::uint64_t>::max() >> 7)) return false;
        result = (result << 7) | (value & 0x7f);
    } while (value & 0x80);
    arc = result;
    return true;
}

void AppendArc(std::string& out, std::uint64_t arc, bool first) {
    char text[22];
    const int length = std::snprintf(text, sizeof(text), first ? "%llu" : ".%llu", static_cast<unsigned long long>(arc));
    out.append(text, static_cast<std::size_t>(length));
}

bool ContentToText(const LibreSslAsn1Object& object, std::string& out) {
    if (object.length < 0) throw std::invalid_argument("OBJ_obj2txt: negative encoded length");
    const unsigned char* data = object.data;
    auto remaining = static_cast<std::size_t>(object.length);
    std::uint64_t arc = 0;
    if (!ParseContentArc(data, remaining, arc)) return false;
    std::uint64_t first = arc / 40;
    if (first > 2) first = 2;
    AppendArc(out, first, true);
    AppendArc(out, arc - first * 40, false);
    while (remaining > 0) {
        if (!ParseContentArc(data, remaining, arc)) return false;
        AppendArc(out, arc, false);
    }
    return true;
}

const char* BuiltInName(int nid) {
    const LibreSslAsn1Object* object = LibreSsl3::ObjectByNid(nid);
    if (object && object->longName) return object->longName;
    object = LibreSsl3::ObjectByNid(nid);
    return object ? object->shortName : nullptr;
}

const char* ObjectName(const LibreSslAsn1Object& object) {
    const int nid = LibreSsl3::ObjectNid(&object);
    if (nid == 0) return nullptr;
    const LibreSslAsn1Object* added = LibreSslAddedObjectByNid_nid_no_patch(nid);
    const bool isAdded = added && (object.nid != 0 ? added == &object : LibreSslAddedObjectNid_nid_no_patch(&object) == nid);
    if (!isAdded) return BuiltInName(nid);
    return added->longName ? added->longName : added->shortName;
}

LibreSslAsn1Object* NamedObject(const char* text) {
    if (const int nid = LibreSslAddedObjectShortNameNid_nid_no_patch(text)) return const_cast<LibreSslAsn1Object*>(LibreSslAddedObjectByNid_nid_no_patch(nid));
    if (const int nid = LibreSsl3::ShortNameNid(text)) return const_cast<LibreSslAsn1Object*>(LibreSsl3::ObjectByNid(nid));
    if (const int nid = LibreSslAddedObjectLongNameNid_nid_no_patch(text)) return const_cast<LibreSslAsn1Object*>(LibreSslAddedObjectByNid_nid_no_patch(nid));
    if (const int nid = LibreSsl3::LongNameNid(text)) return const_cast<LibreSslAsn1Object*>(LibreSsl3::ObjectByNid(nid));
    return nullptr;
}

LibreSslAsn1Object* NewObject(const char* text) {
    const std::optional<std::vector<unsigned char>> content = TextToContent(text);
    if (!content) return nullptr;
    if (content->size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return nullptr;
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(content->size()));
    if (!data) return nullptr;
    std::memcpy(data, content->data(), content->size());
    auto* object = static_cast<LibreSslAsn1Object*>(calloc_nid_postfix(1, sizeof(LibreSslAsn1Object)));
    if (!object) {
        Asn1Error(MallocFailure);
        free_nid_postfix(data);
        return nullptr;
    }
    object->data = data;
    object->length = static_cast<std::int32_t>(content->size());
    object->flags = Dynamic | DynamicData;
    return object;
}

} // namespace

extern "C" {

int APS5_VABI OBJ_obj2txt_nid_postfix(char* buffer, int length, const LibreSslAsn1Object* object, int noName) {
    if (length < 0) return 0;
    if (length > 0) {
        if (!buffer) throw std::invalid_argument("OBJ_obj2txt: null buffer with a positive length");
        buffer[0] = '\0';
    }
    if (!object || !object->data) return 0;
    std::string text;
    const char* name = noName ? nullptr : ObjectName(*object);
    if (name) {
        text = name;
    } else if (!ContentToText(*object, text)) {
        return 0;
    }
    if (length > 0) {
        const std::size_t copied = text.size() < static_cast<std::size_t>(length) ? text.size() : static_cast<std::size_t>(length) - 1;
        std::memcpy(buffer, text.data(), copied);
        buffer[copied] = '\0';
    }
    return static_cast<int>(text.size());
}

LibreSslAsn1Object* APS5_VABI OBJ_txt2obj_nid_postfix(const char* text, int noName) {
    if (!text) throw std::invalid_argument("OBJ_txt2obj: null text");
    if (!noName) {
        if (LibreSslAsn1Object* object = NamedObject(text)) return object;
    }
    return NewObject(text);
}

}
